/*
 * Snake: port of ApolloBrickGames src/brick/games/library/arcade.ts (class SnakeGame) on top of the shared game base
 * (lives, clear scenes). Behaviour is verified tick by tick against the reference, see tests/conformance and
 * docs/CONFORMANCE.md.
 *
 * Rules (reference comments): the worm starts on the bottom row, head (5,0) and tail (3,0) counting y from the bottom;
 * a direction key moves the head at once and restarts the step timer; ROTATE (the big key) is the fourth direction,
 * "up", never a turbo; the opposite direction swaps head and tail without a step; a meal gives 10 points and 13 meals
 * win the level (+100). Four lives. The step period is one of ten values measured on the ROM (332.358 ... 37.038 ms),
 * not rounded to whole ticks: the timer keeps the fractional remainder.
 *
 * Adaptations the reference itself marks (docs/GAME_SNAKE.md): the 13 meals per level and the +100 bonus, the generator
 * of the food position (seeded RNG instead of the free-running timer of the firmware). One variant only (ADR-036): the
 * reconstructed "Frame" variant of the reference is not ported.
 */
#include "brick/games/snake.h"

#include <string.h>

#include "snake_internal.h"

static const int DX[4] = {0, 1, 0, -1};
static const int DY[4] = {-1, 0, 1, 0};

/* E88 8 in 1, game 5, executed in BrickEmuPy (reference e88.ts): complete movement periods in milliseconds. */
static const double E88_SNAKE_MS[10] = {332.358, 299.334, 266.316, 233.764, 200.738,
                                        168.178, 135.156, 102.128, 70.056,  37.038};

static inline bks_game_t *self(bk_game_t *g)
{
    return (bks_game_t *)g;
}

static inline const bks_game_t *cself(const bk_game_t *g)
{
    return (const bks_game_t *)g;
}

double bks_step_ticks(int speed)
{
    int i = speed - 1;
    i = i < 0 ? 0 : (i > 9 ? 9 : i);
    return E88_SNAKE_MS[i] * 60.0 / 1000.0; /* same operations, same order as the reference */
}

static bool on_body(const bks_game_t *s, int p, unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        if (s->body[i] == p) {
            return true;
        }
    }
    return false;
}

/* rng.pick over the free cells in ascending order: one draw, none when a single cell is free. */
static void spawn_food(bks_game_t *s)
{
    unsigned free_cells = 0;
    for (int p = 0; p < BK_CELLS; p++) {
        if (!on_body(s, p, s->len)) {
            free_cells++;
        }
    }
    unsigned index = bk_rng_int(&s->base.rng, free_cells);
    for (int p = 0; p < BK_CELLS; p++) {
        if (!on_body(s, p, s->len)) {
            if (index == 0) {
                s->food = (uint8_t)p;
                return;
            }
            index--;
        }
    }
}

static void setup(bks_game_t *s)
{
    int row = 19; /* y counted from the top: the worm lies in the bottom row */
    s->body[0] = (uint8_t)(row * BK_COLS + 5);
    s->body[1] = (uint8_t)(row * BK_COLS + 4);
    s->body[2] = (uint8_t)(row * BK_COLS + 3);
    s->len = 3;
    s->dir = 1;
    s->eaten = 0;
    s->timer = 0;
    spawn_food(s);
}

static void snake_reset(bk_game_t *g)
{
    bks_game_t *s = self(g);
    bk_game_set_lives(g, BKS_LIVES);
    setup(s);
}

static int tapped_direction(const bk_input_t *in)
{
    if (in->pressed & BK_KEY_UP) {
        return 0;
    }
    if (in->pressed & BK_KEY_RIGHT) {
        return 1;
    }
    if (in->pressed & BK_KEY_DOWN) {
        return 2;
    }
    if (in->pressed & BK_KEY_LEFT) {
        return 3;
    }
    return -1;
}

static void reverse_worm(bks_game_t *s)
{
    for (unsigned i = 0, j = s->len - 1u; i < j; i++, j--) {
        uint8_t t = s->body[i];
        s->body[i] = s->body[j];
        s->body[j] = t;
    }
    int head = s->body[0];
    int neck = s->body[1];
    /* the direction follows the new head, which may be the end of a bent worm */
    s->dir = head == neck - BK_COLS ? 0 : (head == neck + 1 ? 1 : (head == neck + BK_COLS ? 2 : 3));
}

static void snake_update(bk_game_t *g, const bk_input_t *in)
{
    bks_game_t *s = self(g);
    int d = tapped_direction(in);
    if (d < 0 && (in->pressed & (BK_KEY_ROTATE | BK_KEY_FIRE)) != 0) {
        d = 0; /* ROTATE is the fourth direction (up), not a turbo key */
    }
    if (d == (s->dir + 2) % 4) {
        reverse_worm(s); /* the whole worm turns round, even when its tail is round a corner; no step, no timer reset */
        return;
    }
    if (d >= 0) {
        s->dir = (uint8_t)d;
        s->timer = 0;
    } else {
        s->timer += 1.0;
        double period = bks_step_ticks(g->speed);
        if (s->timer < period) {
            return;
        }
        s->timer -= period;
    }
    int head = s->body[0];
    int x = head % BK_COLS + DX[s->dir];
    int y = head / BK_COLS + DY[s->dir];
    int to = y * BK_COLS + x;
    bool eat = x >= 0 && x < BK_COLS && y >= 0 && y < BK_ROWS && to == s->food;
    /* the cell the tail leaves is free unless the worm is growing */
    if (x < 0 || x >= BK_COLS || y < 0 || y >= BK_ROWS || on_body(s, to, eat ? s->len : s->len - 1u)) {
        bk_game_lose_life(g, BKS_SCENE_LOSE);
        return;
    }
    memmove(s->body + 1, s->body, s->len);
    s->body[0] = (uint8_t)to;
    if (!eat) {
        bk_game_emit(g, BK_EV_MOVE, 0, 0, 0);
        return;
    }
    s->len++;
    bk_game_add_score(g, 10);
    bk_game_emit(g, BK_EV_BONUS, 0, 0, 0);
    if (++s->eaten == BKS_MEALS_PER_LEVEL) {
        bk_game_win_level(g, BKS_SCENE_WIN);
    } else {
        spawn_food(s);
    }
}

static void snake_draw(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    const bks_game_t *s = cself(g);
    memset(cells, BK_CELL_OFF, BK_CELLS);
    for (unsigned i = 0; i < s->len; i++) {
        cells[s->body[i]] = BK_CELL_ON;
    }
    cells[s->body[0]] = BK_CELL_BLINK; /* the head and the food blink */
    cells[s->food] = BK_CELL_BLINK;
}

static void snake_scene_frame(bk_game_t *g, uint8_t kind, uint16_t elapsed)
{
    (void)kind;
    bk_game_paint_clear(g, BK_CLEAR_SCENE_TICKS, elapsed);
}

static void snake_scene_done(bk_game_t *g, uint8_t kind)
{
    if (kind == BKS_SCENE_WIN) {
        bk_game_next_level(g);
    }
    setup(self(g));
}

/* ---------------------------------------------------------------- definition */

static const bk_game_vtable_t SNAKE_VTABLE = {
    snake_reset, snake_update, snake_draw, NULL, NULL, snake_scene_frame, snake_scene_done,
};

bk_game_t *bk_snake_create(void *storage, const bk_game_options_t *opt)
{
    bks_game_t *s = (bks_game_t *)storage;
    memset(s, 0, sizeof *s);
    bk_game_base_init(&s->base, &SNAKE_VTABLE, opt, 1);
    bk_game_restart(&s->base);
    return &s->base;
}

static void snake_mode_label(uint16_t mode, char *buf, size_t cap)
{
    (void)mode; /* one variant per game (ADR-036) */
    if (cap > 0) {
        strncpy(buf, "NORMAL", cap - 1);
        buf[cap - 1] = '\0';
    }
}

/* Menu illustration (own artwork, no reference sprite): a worm crawling towards a meal, two frames. */
static void snake_thumbnail(uint8_t frame, uint8_t cells[BK_COLS * 5])
{
    static const char *const art[2][5] = {
        {"..........", ".#####..#.", ".....#....", ".....#####", ".........."},
        {"..........", "..####..#.", ".....#....", ".....#####", ".........."},
    };
    const char *const *rows = art[frame % 2u];
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < BK_COLS; x++) {
            cells[y * BK_COLS + x] = rows[y][x] == '#' ? BK_CELL_ON : BK_CELL_OFF;
        }
    }
}

const bk_game_def_t bk_snake_def = {
    BK_SNAKE_UID,         "snake",         "SNAKE",          1, 0,  sizeof(bks_game_t),
    _Alignof(bks_game_t), bk_snake_create, snake_mode_label, 2, 30, snake_thumbnail,
};
