/*
 * Racing: port of ApolloBrickGames src/brick/games/racing.ts (RacingGame of the donor n0Live/BrickGame, MIT) on top of the
 * shared game base. Verified tick by tick against the reference, see tests/conformance and docs/CONFORMANCE.md.
 *
 * A car on two lanes dodges oncoming cars. LEFT / RIGHT change the lane (a lane taken by a car is a crash), UP and
 * ROTATE speed the road up by extra steps. Every car that gets past gives a point; every 50
 * points the speed grows, past speed 10 the level does. A crash is an explosion, a lost life and a fresh road; an extra life
 * every 2000 points. Four lives. One variant only (ADR-036): two lanes on the normal field; the donor's three lanes and the
 * upside-down field are not ported. NOT verified against the ROM of a real device: the rules are the donor's.
 */
#include "brick/games/racing.h"

#include <string.h>

#include "racing_internal.h"

static inline bkr_game_t *self(bk_game_t *g)
{
    return (bkr_game_t *)g;
}

static inline const bkr_game_t *cself(const bk_game_t *g)
{
    return (const bkr_game_t *)g;
}

/* ---------------------------------------------------------------- the car */

/* Rows from the wheels (y = -1) to the nose (y = 2). */
static const char *const CAR_ROWS[4] = {"#.#", ".#.", "###", ".#."};

static bool car_covers(int cx, int cy, int x, int y)
{
    int row = y - cy - BKR_CAR_MIN_Y;
    int col = x - cx + 1;
    return row >= 0 && row < 4 && col >= 0 && col < 3 && CAR_ROWS[row][col] == '#';
}

bool bkr_cars_overlap(int ax, int ay, int bx, int by)
{
    int dx_far = ax > bx ? ax - bx : bx - ax;
    int dy_far = ay > by ? ay - by : by - ay;
    if (dx_far > 2 || dy_far > BKR_CAR_MAX_Y - BKR_CAR_MIN_Y) {
        return false;
    }
    for (int dy = BKR_CAR_MIN_Y; dy <= BKR_CAR_MAX_Y; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (car_covers(ax, ay, ax + dx, ay + dy) && car_covers(bx, by, ax + dx, ay + dy)) {
                return true;
            }
        }
    }
    return false;
}

/* ---------------------------------------------------------------- timing */

/* Math.round for x >= 0 (halves go up), without libm. */
static int js_round(double x)
{
    int i = (int)x;
    return x - (double)i >= 0.5 ? i + 1 : i;
}

static unsigned ms_to_ticks(double ms)
{
    int t = js_round(ms * BK_TICK_HZ / 1000.0);
    return t < 1 ? 1u : (unsigned)t;
}

unsigned bkr_half_step_ticks(int speed, int level)
{
    double k = 1.0 / 2.0; /* the donor's (wide ? 1.5 : 1) / 2 for two lanes */
    double first_ms = (double)(200 - 5 * level) * k;
    double tenth_ms = (double)(50 - level + 1) * k;
    int clamped = speed < BK_MIN_SPEED ? BK_MIN_SPEED : (speed > BK_MAX_SPEED ? BK_MAX_SPEED : speed);
    double step = (first_ms - tenth_ms) / (double)(BK_MAX_SPEED - BK_MIN_SPEED);
    return ms_to_ticks((double)js_round(first_ms - step * (double)(clamped - 1)));
}

/* ---------------------------------------------------------------- score */

void bkr_set_score(bkr_game_t *s, uint32_t value)
{
    bk_game_t *g = &s->base;
    uint32_t before = g->score;
    bk_game_set_score(g, value);
    if (g->score / BKR_LEVELUP_POINT > before / BKR_LEVELUP_POINT) {
        bk_game_set_speed(g, g->speed + 1);
        if (g->speed == BK_MIN_SPEED) { /* the speed wrapped from 10 to 1: the level rises */
            g->level = g->level >= BK_MAX_LEVEL ? BK_MIN_LEVEL : (uint8_t)(g->level + 1);
        }
        bk_game_emit(g, BK_EV_LEVEL_UP, g->speed, 0, 0);
    }
}

static void add_score(bkr_game_t *s, uint32_t points)
{
    uint64_t sum = (uint64_t)s->base.score + points;
    bkr_set_score(s, sum > BK_MAX_SCORE ? BK_MAX_SCORE : (uint32_t)sum);
}

/* ---------------------------------------------------------------- drawing */

static bool border_lit(int y, int phase)
{
    return (y + phase) % BKR_BORDER_SPAN < BKR_BORDER_LIT;
}

/* Put a cell by donor coordinates: y counts from the bottom of the road, the screen row from the top. */
static void plot(uint8_t cells[BK_CELLS], int x, int y, uint8_t value)
{
    if (x < 0 || x >= BK_COLS || y < 0 || y >= BK_ROWS) {
        return;
    }
    cells[(BK_ROWS - 1 - y) * BK_COLS + x] = value;
}

static void plot_car(uint8_t cells[BK_CELLS], int cx, int cy)
{
    for (int index = 0; index < 4; index++) {
        for (int col = 0; col < 3; col++) {
            if (CAR_ROWS[index][col] == '#') {
                plot(cells, cx + col - 1, cy + BKR_CAR_MIN_Y + index, BK_CELL_ON);
            }
        }
    }
}

static void racing_draw(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    const bkr_game_t *s = cself(g);
    memset(cells, BK_CELL_OFF, BK_CELLS);
    for (int y = 0; y < BK_ROWS; y++) {
        if (!border_lit(y, s->border_phase)) {
            continue;
        }
        plot(cells, BK_COLS - 1, y, BK_CELL_ON);
        plot(cells, 0, y, BK_CELL_ON);
    }
    for (unsigned i = 0; i < s->opp_count; i++) {
        plot_car(cells, s->opp[i].x, s->opp[i].y);
    }
    plot_car(cells, s->car_x, BKR_CAR_Y);
}

/* ---------------------------------------------------------------- the road */

static void spawn(bkr_game_t *s)
{
    bk_rng_t *rng = &s->base.rng;
    int distance = 10 - s->base.level / 2 + (int)bk_rng_int(rng, 3);
    int last_y = s->tail_valid ? s->tail_y + BKR_CAR_MAX_Y : BK_ROWS - distance;
    if (last_y > BK_ROWS - distance) {
        return;
    }
    do {
        int lane = (int)bk_rng_int(rng, BKR_LANES);
        int y = last_y + distance - BKR_CAR_MIN_Y;
        if (s->opp_count < BKR_MAX_OPPONENTS) {
            s->opp[s->opp_count].x = (int8_t)BKR_LANE_X[lane];
            s->opp[s->opp_count++].y = (int8_t)y;
        }
        s->tail_valid = true;
        s->tail_y = (int16_t)y;
        last_y = y + BKR_CAR_MAX_Y;
    } while (last_y < BK_ROWS - distance);
}

/* Moves the cars one cell down. Returns true when one of them hit the player's car (the rest stay where they were). */
static bool scroll(bkr_game_t *s, unsigned *passed)
{
    *passed = 0;
    unsigned i = 0;
    while (i < s->opp_count) {
        bkr_car_t *o = &s->opp[i];
        if (o->y + BKR_CAR_MAX_Y > 0) {
            bool crashed = bkr_cars_overlap(o->x, o->y - 1, s->car_x, BKR_CAR_Y);
            o->y--;
            if (crashed) {
                return true;
            }
            i++;
        } else {
            memmove(&s->opp[i], &s->opp[i + 1], (s->opp_count - i - 1u) * sizeof s->opp[0]);
            s->opp_count--;
            (*passed)++;
        }
    }
    if (s->tail_valid) {
        s->tail_y--;
    }
    return false;
}

static void intro(bkr_game_t *s)
{
    bk_game_cutscene(&s->base, BKR_SCENE_INTRO, BKR_INTRO_TICKS);
}

static void crash(bkr_game_t *s, int x, int y)
{
    bk_game_emit(&s->base, BK_EV_CRASH, 0, 0, 0);
    racing_draw(&s->base, s->kaboom_base); /* the explosion paints over the field as it is now */
    s->kaboom_x = (int8_t)x;
    s->kaboom_y = (int8_t)(BK_ROWS - 1 - y);
    bk_game_cutscene(&s->base, BKR_SCENE_KABOOM, BK_KABOOM_TICKS);
}

/* One road step: the border and, with `with_traffic`, the cars. */
static void advance(bkr_game_t *s, bool with_traffic)
{
    s->border_phase = (uint8_t)((s->border_phase + 1u) % BKR_BORDER_SPAN);
    if (!with_traffic) {
        return;
    }
    unsigned passed;
    bool crashed = scroll(s, &passed);
    for (unsigned i = 0; i < passed; i++) {
        add_score(s, 1);
    }
    if (crashed) {
        crash(s, s->car_x, BKR_CAR_Y + BKR_CAR_MAX_Y);
        return;
    }
    spawn(s);
}

static void setup_road(bkr_game_t *s)
{
    s->opp_count = 0;
    s->tail_valid = false;
    s->tail_y = 0;
    s->border_phase = BKR_BORDER_SPAN - 1; /* the first step shows phase 0 */
    s->lane = 1;
    s->car_x = (int8_t)BKR_LANE_X[1];
    s->since_step = 0;
    s->odd_move = false;
    s->steer_left = s->steer_right = s->accel_up = s->accel_rotate = 0;
    spawn(s);
    advance(s, true); /* the first car enters the top row */
}

static void change_lane(bkr_game_t *s, int delta)
{
    int lane = s->lane + delta;
    if (lane < 0 || lane >= BKR_LANES) {
        return;
    }
    int x = BKR_LANE_X[lane];
    bool accident = false;
    for (unsigned i = 0; i < s->opp_count; i++) {
        accident = accident || bkr_cars_overlap(x, BKR_CAR_Y, s->opp[i].x, s->opp[i].y);
    }
    s->lane = (uint8_t)lane;
    s->car_x = (int8_t)x;
    if (accident) {
        crash(s, x, BKR_CAR_Y);
        return;
    }
    bk_game_emit(&s->base, BK_EV_MOVE, 0, 0, 0);
}

/* A key from the game's point of view: FIRE is ROTATE. */
static uint8_t map_keys(uint8_t keys)
{
    return (keys & BK_KEY_FIRE) != 0u ? (uint8_t)((keys & ~BK_KEY_FIRE) | BK_KEY_ROTATE) : keys;
}

/* ---------------------------------------------------------------- game */

static void racing_reset(bk_game_t *g)
{
    bkr_game_t *s = self(g);
    bk_game_set_lives(g, BK_MAX_LIVES);
    g->scores_to_extra_life = BKR_EXTRA_LIFE_AT;
    setup_road(s);
}

static void racing_on_start(bk_game_t *g)
{
    intro(self(g));
}

static void racing_update(bk_game_t *g, const bk_input_t *raw)
{
    bkr_game_t *s = self(g);
    bk_input_t in = bk_input_make(map_keys(raw->pressed), map_keys(raw->held));

    s->since_step++;
    if (s->since_step >= bkr_half_step_ticks(g->speed, g->level)) {
        s->since_step = 0;
        s->odd_move = !s->odd_move;
        advance(s, s->odd_move);
        if (bk_game_busy(g)) {
            return;
        }
    }

    if (bk_repeat_fire(&s->steer_left, BK_KEY_LEFT, &in, BKR_STEER_TICKS, BKR_STEER_TICKS)) {
        change_lane(s, -1);
    }
    if (bk_game_busy(g)) {
        return;
    }
    if (bk_repeat_fire(&s->steer_right, BK_KEY_RIGHT, &in, BKR_STEER_TICKS, BKR_STEER_TICKS)) {
        change_lane(s, +1);
    }
    if (bk_game_busy(g)) {
        return;
    }

    bool up = bk_repeat_fire(&s->accel_up, BK_KEY_UP, &in, BKR_ACCEL_TICKS, BKR_ACCEL_TICKS);
    bool rotate = bk_repeat_fire(&s->accel_rotate, BK_KEY_ROTATE, &in, BKR_ACCEL_TICKS, BKR_ACCEL_TICKS);
    if (up || rotate) {
        s->odd_move = !s->odd_move; /* an extra road step outside the timer */
        advance(s, s->odd_move);
    }
}

static void racing_scene_frame(bk_game_t *g, uint8_t kind, uint16_t elapsed)
{
    const bkr_game_t *s = cself(g);
    if (kind == BKR_SCENE_KABOOM) {
        bk_game_paint_kaboom(g, s->kaboom_base, s->kaboom_x, s->kaboom_y, elapsed);
    } else if (kind == BKR_SCENE_CLEAR) {
        bk_game_paint_clear(g, BK_CLEAR_LOSE_TICKS, elapsed);
    }
}

static void racing_scene_done(bk_game_t *g, uint8_t kind)
{
    bkr_game_t *s = self(g);
    if (kind == BKR_SCENE_KABOOM) {
        bk_game_lose_life_ticks(g, BKR_SCENE_CLEAR, BK_CLEAR_LOSE_TICKS);
    } else if (kind == BKR_SCENE_CLEAR) {
        setup_road(s);
        intro(s);
    }
}

static const bk_game_vtable_t RACING_VTABLE = {
    racing_reset, racing_update, racing_draw, NULL, racing_on_start, racing_scene_frame, racing_scene_done,
};

const uint8_t BKR_LANE_X[BKR_LANES] = {3, 6};

bk_game_t *bk_racing_create(void *storage, const bk_game_options_t *opt)
{
    bkr_game_t *s = (bkr_game_t *)storage;
    memset(s, 0, sizeof *s);
    bk_game_base_init(&s->base, &RACING_VTABLE, opt, 1);
    bk_game_restart(&s->base);
    return &s->base;
}

/*
 * The menu clip (ADR-041, own artwork): the bottom five rows of the road, the border scrolling one row per frame (period
 * five, so thirty frames loop without a seam) and the cars one row every second frame, as in the game. A car passes in
 * the left lane, a second one comes down the right lane and the player swerves into the free left lane at the last moment
 * (frame 17), then slips back to the right after it has passed (the loop point). The cars are the 3 x 4 car of the game.
 */
// clang-format off
static const char RACING_CLIP_FRAMES[][BK_CLIP_CELLS + 1] = {
    /* 0: the car in the left lane enters from the top */
    "..#.#....." "......#..." "#....###.#" "#.....#..#" "#....#.#.#",
    "#.#.#....#" "......#..." ".....###.." "#.....#..#" "#....#.#.#",
    "#..#.....#" "#.#.#.#..#" ".....###.." "......#..." "#....#.#.#",
    "#..#.....#" "#.#.#.#..#" "#....###.#" "......#..." ".....#.#..",
    "..###....." "#..#..#..#" "#.#.####.#" "#.....#..#" ".....#.#..",
    /* 5 */
    "..###....." "...#..#..." "#.#.####.#" "#.....#..#" "#....#.#.#",
    "#..#.....#" "..###.#..." "...#.###.." "#.#.#.#..#" "#....#.#.#",
    "#..#.....#" "#.###.#..#" "...#.###.." "..#.#.#..." "#....#.#.#",
    "#........#" "#..#..#..#" "#.######.#" "...#..#..." "..#.##.#..",
    "..........""#..#..#..#""#.######.#""#..#..#..#""..#.##.#..", /* 9 (poster): the cars side by side */
    /* 10 */
    "..........""......#...""#..#.###.#""#.###.#..#""#..#.#.#.#",
    "#........#""......#...""...#.###..""#.###.#..#""#..#.#.#.#",
    "#........#""#.....#..#"".....###..""...#..#...""#.####.#.#",
    "#........#""#.....#..#""#....###.#""...#..#...""..####.#..",
    ".....#.#..""#.....#..#""#....###.#""#.....#..#""...#.#.#..",
    /* 15: the car in the right lane enters from the top */
    ".....#.#..""......#...""#....###.#""#.....#..#""#..#.#.#.#",
    "#.....#..#"".....###.."".....###..""#.....#..#""#....#.#.#",
    "#.....#..#""#..#.#.#.#""..###.....""...#......""#.#.#....#", /* 17: the player swerves */
    "#....###.#""#..#..#..#""#.####.#.#""...#......""..#.#.....",
    ".....###..""#..#..#..#""#.####.#.#""#..#.....#""..#.#.....",
    /* 20 */
    "......#...""...#.###..""#.###.#..#""#..#.#.#.#""#.#.#....#",
    "#.....#..#""...#.###..""..###.#...""#..#.#.#.#""#.#.#....#",
    "#........#""#..#..#..#""..######..""...#..#...""#.#.##.#.#",
    "#........#""#..#..#..#""#.######.#""...#..#...""..#.##.#..",
    "..........""#..#.....#""#.###.#..#""#..#.###.#""..#.#.#...",
    /* 25 */
    "..........""...#......""#.###.#..#""#..#.###.#""#.#.#.#..#",
    "#........#""...#......""..###.....""#..#..#..#""#.#.####.#",
    "#........#""#..#.....#""..###.....""...#..#...""#.#.####.#",
    "#........#""#..#.....#""#.###....#""...#......""..#.#.#...",
    "..........""#..#.....#""#.###....#""#..#.....#""..#.#.#...", /* 29: the player returns at the loop point */
};
// clang-format on

static const bk_clip_t RACING_CLIP = {
    (uint16_t)BK_ARRAY_LEN(RACING_CLIP_FRAMES),
    10,
    9,
    RACING_CLIP_FRAMES,
};

const bk_game_def_t bk_racing_def = {
    BK_RACING_UID, "racing", "RACING", 1, 0, sizeof(bkr_game_t), _Alignof(bkr_game_t), bk_racing_create, &RACING_CLIP,
};
