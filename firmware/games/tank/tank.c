/*
 * Tank: port of ApolloBrickGames src/brick/games/tanks.ts (TanksGame of the donor n0Live/BrickGame, MIT) on top of the
 * shared game base. Verified tick by tick against the reference, see tests/conformance and docs/CONFORMANCE.md.
 *
 * The player's tank (7 cells) fights three enemy tanks (6 cells) that enter one by one at the corners and the middles of
 * the side edges, drive towards the player and shoot. A bullet is one cell and destroys one obstacle cell it hits. 25
 * enemies destroyed win the level; a hit on the player costs one of four lives. The direction keys turn the tank on the spot
 * or step forward; ACTION (ROTATE) fires. One variant only (ADR-036): three enemies and the ready-made fields of
 * levels 2..10; the donor's other seven variants (four to six enemies, random fields) are not ported. NOT verified against
 * the ROM of a real device: the rules are the donor's.
 */
#include "brick/games/tank.h"

#include <string.h>

#include "tank_internal.h"

static inline bkk_game_t *self(bk_game_t *g)
{
    return (bkk_game_t *)g;
}

static inline const bkk_game_t *cself(const bk_game_t *g)
{
    return (const bkk_game_t *)g;
}

static inline bool in_board(int x, int y)
{
    return x >= 0 && x < BKK_W && y >= 0 && y < BKK_H;
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

unsigned bkk_enemy_ticks(int speed)
{
    int clamped = speed < BK_MIN_SPEED ? BK_MIN_SPEED : (speed > BK_MAX_SPEED ? BK_MAX_SPEED : speed);
    double step = (double)(BKK_SPEED_FIRST_MS - BKK_SPEED_TENTH_MS) / (double)(BK_MAX_SPEED - BK_MIN_SPEED);
    return ms_to_ticks((double)js_round((double)BKK_SPEED_FIRST_MS - step * (double)(clamped - 1)));
}

/* ---------------------------------------------------------------- the field */

/* Obstacles, tanks (except `skip`, compared by address) and bullets, in donor coordinates. */
void bkk_build_board(const bkk_game_t *s, const bkk_tank_t *skip, uint8_t board[BK_CELLS])
{
    memcpy(board, s->obstacles, BK_CELLS);
    if (&s->player != skip) {
        bkk_put_tank(board, BKK_PLAYER, &s->player);
    }
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (s->enemy_live[i] && &s->enemy[i] != skip) {
            bkk_put_tank(board, BKK_ENEMY, &s->enemy[i]);
        }
    }
    for (int i = 0; i < BKK_PLAYER_BULLETS; i++) {
        if (s->player_bullet[i].live && in_board(s->player_bullet[i].x, s->player_bullet[i].y)) {
            board[s->player_bullet[i].y * BKK_W + s->player_bullet[i].x] = 1;
        }
    }
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (s->enemy_bullet[i].live && in_board(s->enemy_bullet[i].x, s->enemy_bullet[i].y)) {
            board[s->enemy_bullet[i].y * BKK_W + s->enemy_bullet[i].x] = 1;
        }
    }
}

/* Every lit cell burns steadily (the donor has no blinking cells in Tank); y is turned over to the screen. */
static void tank_draw(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    uint8_t board[BK_CELLS];
    bkk_build_board(cself(g), NULL, board);
    memset(cells, BK_CELL_OFF, BK_CELLS);
    for (int y = 0; y < BKK_H; y++) {
        for (int x = 0; x < BKK_W; x++) {
            if (board[y * BKK_W + x] != 0) {
                cells[(BKK_H - 1 - y) * BKK_W + x] = BK_CELL_ON;
            }
        }
    }
}

/* ---------------------------------------------------------------- bullets */

static int bullet_at(const bkk_bullet_t *list, int count, int x, int y)
{
    for (int i = 0; i < count; i++) {
        if (list[i].live && list[i].x == x && list[i].y == y) {
            return i;
        }
    }
    return -1;
}

static int enemy_covering(const bkk_game_t *s, int x, int y)
{
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (s->enemy_live[i] && bkk_covers(BKK_ENEMY, &s->enemy[i], x, y)) {
            return i;
        }
    }
    return -1;
}

/* An enemy dies to the player's bullet: +1 point, +1 to the kills of the level (destroyEnemyTank). */
static void destroy_enemy(bkk_game_t *s, int index)
{
    s->enemy_live[index] = false;
    s->killed++;
    s->on_board--;
    bk_game_emit(&s->base, BK_EV_EXPLODE, 0, 0, 0);
    bk_game_add_score(&s->base, 1);
}

/*
 * One step of a bullet by one cell (flightOfBullet). Flying out of the field removes it. An occupied cell is a hit (event
 * HIT) and the bullet is gone; then, in the donor's order: a meeting with an enemy bullet (and, for an enemy bullet, with
 * a player's bullet as well) removes both; an enemy bullet in the player's tank hits the player (returns false); a player's
 * bullet in an enemy destroys it; otherwise the obstacle cell is erased. A player's bullet in a player's bullet and an
 * enemy bullet in an enemy tank just vanish.
 */
static bool flight_of_bullet(bkk_game_t *s, bool by_player, int index)
{
    bkk_bullet_t *slots = by_player ? s->player_bullet : s->enemy_bullet;
    bkk_bullet_t *bullet = &slots[index];
    if (!bullet->live) {
        return true;
    }
    int x = bullet->x + BKK_DX[bullet->dir];
    int y = bullet->y + BKK_DY[bullet->dir];
    if (!in_board(x, y)) {
        bullet->live = false;
        return true;
    }
    int enemy_shot = bullet_at(s->enemy_bullet, BKK_ENEMIES, x, y);
    int player_shot = bullet_at(s->player_bullet, BKK_PLAYER_BULLETS, x, y);
    bool hit_player = bkk_covers(BKK_PLAYER, &s->player, x, y);
    int hit_enemy = enemy_covering(s, x, y);
    int cell = y * BKK_W + x;
    if (enemy_shot < 0 && player_shot < 0 && !hit_player && hit_enemy < 0 && s->obstacles[cell] == 0) {
        bullet->x = (int8_t)x;
        bullet->y = (int8_t)y;
        return true;
    }

    bk_game_emit(&s->base, BK_EV_HIT, 0, 0, 0);
    bullet->live = false;
    if (enemy_shot >= 0) {
        s->enemy_bullet[enemy_shot].live = false;
    } else if (player_shot >= 0 && !by_player) {
        s->player_bullet[player_shot].live = false;
    } else if (player_shot < 0) {
        if (hit_player && !by_player) {
            s->dead = true;
            return false;
        }
        if (hit_enemy >= 0) {
            if (by_player) {
                destroy_enemy(s, hit_enemy);
            }
        } else {
            s->obstacles[cell] = 0;
        }
    }
    return true;
}

/* A bullet leaves the barrel on the cell in front of it and at once makes its first step (fire). No free slot: no shot. */
static bool fire(bkk_game_t *s, const bkk_tank_t *tank, bool by_player)
{
    bkk_bullet_t *slots = by_player ? s->player_bullet : s->enemy_bullet;
    int count = by_player ? BKK_PLAYER_BULLETS : BKK_ENEMIES;
    int index = -1;
    for (int i = 0; i < count; i++) {
        if (!slots[i].live) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        return false;
    }
    if (by_player) { /* only the player's shots are heard: the donor's enemy shots are silent */
        bk_game_emit(&s->base, BK_EV_SHOT, 0, 0, 0);
    }
    slots[index].x = (int8_t)(tank->x + BKK_DX[tank->dir]);
    slots[index].y = (int8_t)(tank->y + BKK_DY[tank->dir]);
    slots[index].dir = tank->dir;
    slots[index].live = true;
    flight_of_bullet(s, by_player, index);
    return true;
}

/* All bullets move on together: the enemies' first, then the player's; after a hit on the player the rest stand still. */
static void flight_of_bullets(bkk_game_t *s)
{
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (!flight_of_bullet(s, false, i)) {
            return;
        }
    }
    for (int i = 0; i < BKK_PLAYER_BULLETS; i++) {
        if (!flight_of_bullet(s, true, i)) {
            return;
        }
    }
}

/* ---------------------------------------------------------------- the enemies */

/* Where enemies appear: the lower corners, the middles of the side edges, the upper corners (before the shift inwards). */
static const int8_t SPAWN_POINTS[6][2] = {
    {0, 0}, {BKK_W - 1, 0}, {0, BKK_H / 2}, {BKK_W - 1, BKK_H / 2}, {0, BKK_H - 1}, {BKK_W - 1, BKK_H - 1},
};

/*
 * An enemy appears (respawn): up to three attempts to pick one of the six places (the centre is moved inwards) with a free
 * 3 x 3 square; the tank looks at the player at once. As in the donor, a success on the third attempt does not count, so in
 * fact there are two attempts.
 */
static bool respawn(bkk_game_t *s, int index)
{
    uint8_t board[BK_CELLS];
    bkk_build_board(s, NULL, board);
    int8_t cells[9][2];
    int n = bkk_mask(BKK_ENEMY, BKK_UP, cells);
    int min_x = cells[0][0], max_x = cells[0][0], min_y = cells[0][1], max_y = cells[0][1];
    for (int i = 1; i < n; i++) {
        min_x = cells[i][0] < min_x ? cells[i][0] : min_x;
        max_x = cells[i][0] > max_x ? cells[i][0] : max_x;
        min_y = cells[i][1] < min_y ? cells[i][1] : min_y;
        max_y = cells[i][1] > max_y ? cells[i][1] : max_y;
    }
    int x, y;
    int tries = 3;
    do {
        uint32_t p = bk_rng_int(&s->base.rng, 6);
        x = SPAWN_POINTS[p][0];
        y = SPAWN_POINTS[p][1];
        if (x + min_x < 0) {
            x -= min_x;
        } else if (x + max_x >= BKK_W) {
            x -= max_x;
        }
        if (y + min_y < 0) {
            y -= min_y;
        } else if (y + max_y >= BKK_H) {
            y -= max_y;
        }
        tries--;
    } while (bkk_hits_board(board, BKK_BOX, BKK_UP, x, y) && tries > 0);
    if (tries <= 0) {
        return false;
    }
    s->enemy[index].x = (int8_t)x;
    s->enemy[index].y = (int8_t)y;
    s->enemy[index].dir = (uint8_t)bkk_shoot_direction(x, y, &s->player);
    s->enemy_live[index] = true;
    return true;
}

/*
 * The move of one enemy cell (aiTurn). An empty cell: unless the kills and the enemies on the field already make 25, a tank
 * tries to appear with probability 3/4; a newcomer shoots at once with probability 1 in (21 - level), otherwise its move
 * ends there. A live tank picks a direction (chooseMove) and decides whether to shoot (wantsToShoot): it shoots towards the
 * player, and if that is also the direction of the move it fires on the move, otherwise it only turns and fires. Returns
 * whether the tank made a move. The donor's 60 ms pause between the newcomer's appearing and its shot is not modelled.
 */
static bool ai_turn(bkk_game_t *s, int index)
{
    bool born_with_gun = false;
    if (!s->enemy_live[index]) {
        if (s->killed + s->on_board >= BKK_KILLS_PER_LEVEL) {
            return false;
        }
        if (bk_rng_int(&s->base.rng, 4) == 0 || !respawn(s, index)) {
            return false;
        }
        s->on_board++;
        if (bk_rng_int(&s->base.rng, (uint32_t)(10 + 11 - s->base.level)) != 0) {
            return true;
        }
        born_with_gun = true;
    }
    const bkk_tank_t *tank = &s->enemy[index];
    uint8_t board[BK_CELLS];
    bkk_build_board(s, tank, board);
    int move = bkk_choose_move(board, tank, &s->player, &s->base.rng);
    if (born_with_gun || bkk_wants_to_shoot(board, s, index, &s->base.rng)) {
        int shot = bkk_shoot_direction(tank->x, tank->y, &s->player);
        bkk_tank_t next = (shot == move && !born_with_gun) ? bkk_move_tank(board, BKK_ENEMY, *tank, move, false)
                                                           : bkk_move_tank(board, BKK_ENEMY, *tank, shot, true);
        s->enemy[index] = next;
        fire(s, &s->enemy[index], false);
    } else {
        s->enemy[index] = bkk_move_tank(board, BKK_ENEMY, *tank, move, false);
    }
    return true;
}

/*
 * A move of the enemies: ONE tank acts, the first one in a circle for which the move succeeds (aiRound); when none does
 * the move is skipped. One move happens every bkk_enemy_ticks(speed) ticks.
 */
static void ai_round(bkk_game_t *s)
{
    int tries = BKK_ENEMIES;
    bool done;
    do {
        if (s->enemy_index >= BKK_ENEMIES) {
            s->enemy_index = 0;
        }
        tries--;
        done = ai_turn(s, s->enemy_index++);
    } while (!done && tries > 0);
}

/* ---------------------------------------------------------------- the level */

static void init_level(bkk_game_t *s)
{
    s->dead = false;
    memset(s->player_bullet, 0, sizeof s->player_bullet);
    memset(s->enemy_bullet, 0, sizeof s->enemy_bullet);
    memset(s->enemy_live, 0, sizeof s->enemy_live);
    s->on_board = 0;
    s->player.x = BKK_PLAYER_START_X;
    s->player.y = BKK_PLAYER_START_Y;
    s->player.dir = BKK_UP;
    /* The donor has obstacles from level 2 on only. It draws the player's tank over random obstacles, so cells under the
     * tank merge with it; the ready-made fields never touch the start place of the tank (tests/unit/test_tank_rules.c),
     * which is why that case of the donor does not exist here and needs no code. */
    bkk_preset_obstacles(s->base.level, s->obstacles);
    s->bullet_left = BKK_BULLET_TICKS;
    memset(s->key_delay, 0, sizeof s->key_delay);
    s->start_left = BKK_START_TICKS; /* the enemies wait in the intro, the player may drive at once */
    s->ai_left = 1;
}

static void load_new_level(bkk_game_t *s)
{
    s->killed = 0;
    init_level(s);
}

/* ---------------------------------------------------------------- the round */

static void lose_life(bkk_game_t *s)
{
    bk_game_t *g = &s->base;
    bk_game_emit(g, BK_EV_CRASH, 0, 0, 0);
    tank_draw(g, s->kaboom_base); /* the explosion paints over the field as it is now */
    s->kaboom_x = s->player.x;
    s->kaboom_y = (int8_t)(BKK_H - 1 - s->player.y);
    bk_game_cutscene(g, BKK_SCENE_KABOOM, BK_KABOOM_TICKS);
}

static void win_level(bkk_game_t *s)
{
    bk_game_emit(&s->base, BK_EV_WIN, 0, 0, 0);
    bk_game_cutscene(&s->base, BKK_SCENE_CLEAR_WIN, BKK_CLEAR_WIN_TICKS);
}

/* The player's tank: a turn on the spot or a step; the board is the field without the player's own tank. */
static void move_player(bkk_game_t *s, int dir)
{
    uint8_t board[BK_CELLS];
    bkk_build_board(s, &s->player, board);
    s->player = bkk_move_tank(board, BKK_PLAYER, s->player, dir, false);
}

/* Direction keys are polled in the donor's order: left, right, up, down; the first one that is not paused acts. */
static const struct {
    uint8_t key;
    uint8_t bit;
    uint8_t dir;
} MOVE_KEYS[4] = {
    {BKK_KEY_LEFT, BK_KEY_LEFT, BKK_LEFT},
    {BKK_KEY_RIGHT, BK_KEY_RIGHT, BKK_RIGHT},
    {BKK_KEY_UP, BK_KEY_UP, BKK_UP},
    {BKK_KEY_DOWN, BK_KEY_DOWN, BKK_DOWN},
};

/*
 * Fresh presses of a direction respond on the next tick; a held key repeats every BKK_MOVE_TICKS. Moving and firing are
 * independent; the gun is limited to one shot per BKK_FIRE_TICKS and four of the player's bullets in flight. These
 * adjustments of responsiveness intentionally differ from the Java donor (docs/GAME_TANK.md).
 */
static void process_keys(bkk_game_t *s, const bk_input_t *in)
{
    for (int i = 0; i < 4; i++) {
        uint8_t key = MOVE_KEYS[i].key;
        if ((in->held & MOVE_KEYS[i].bit) != 0u && s->key_delay[key] == 0) {
            bool same = s->player.dir == MOVE_KEYS[i].dir;
            s->key_delay[key] = same ? BKK_MOVE_TICKS : BKK_ROTATE_TICKS;
            move_player(s, MOVE_KEYS[i].dir);
            break;
        }
    }
    /* ROTATE is the one big action button of the console: it fires; FIRE (engine level) does the same */
    bool fire_held = (in->held & (BK_KEY_ROTATE | BK_KEY_FIRE)) != 0u;
    if (s->start_left == 0 && fire_held && s->key_delay[BKK_KEY_FIRE] == 0 && fire(s, &s->player, true)) {
        s->key_delay[BKK_KEY_FIRE] = BKK_FIRE_TICKS;
    }
}

/* ---------------------------------------------------------------- game */

static void tank_reset(bk_game_t *g)
{
    bkk_game_t *s = self(g);
    bk_game_set_lives(g, BK_MAX_LIVES);
    g->scores_to_extra_life = BKK_EXTRA_LIFE_AT;
    s->enemy_index = 0;
    load_new_level(s);
}

static void tank_update(bk_game_t *g, const bk_input_t *in)
{
    bkk_game_t *s = self(g);
    for (int i = 0; i < BKK_KEYS; i++) {
        if (s->key_delay[i] > 0) {
            s->key_delay[i]--;
        }
    }
    for (int i = 0; i < 4; i++) {
        if ((in->pressed & MOVE_KEYS[i].bit) != 0u) {
            s->key_delay[MOVE_KEYS[i].key] = 0;
        }
    }
    process_keys(s, in);
    if (s->start_left > 0) {
        s->start_left--;
        return;
    }
    if (--s->bullet_left == 0) { /* the shared bullet timer */
        s->bullet_left = BKK_BULLET_TICKS;
        flight_of_bullets(s);
    }
    if (s->dead) {
        lose_life(s);
        return;
    }
    if (s->killed >= BKK_KILLS_PER_LEVEL) {
        win_level(s);
        return;
    }
    s->ai_left--;
    if (s->ai_left > 0) {
        return;
    }
    s->ai_left = (int16_t)bkk_enemy_ticks(g->speed);
    ai_round(s);
    if (s->dead) {
        lose_life(s);
    }
}

static void tank_scene_frame(bk_game_t *g, uint8_t kind, uint16_t elapsed)
{
    const bkk_game_t *s = cself(g);
    if (kind == BKK_SCENE_KABOOM) {
        bk_game_paint_kaboom(g, s->kaboom_base, s->kaboom_x, s->kaboom_y, elapsed);
    } else if (kind == BKK_SCENE_CLEAR_LOSE) {
        bk_game_paint_clear(g, BKK_CLEAR_LOSE_TICKS, elapsed);
    } else if (kind == BKK_SCENE_CLEAR_WIN) {
        bk_game_paint_clear(g, BKK_CLEAR_WIN_TICKS, elapsed);
    }
}

static void tank_scene_done(bk_game_t *g, uint8_t kind)
{
    bkk_game_t *s = self(g);
    if (kind == BKK_SCENE_KABOOM) {
        bk_game_lose_life_ticks(g, BKK_SCENE_CLEAR_LOSE, BKK_CLEAR_LOSE_TICKS);
    } else if (kind == BKK_SCENE_CLEAR_LOSE) {
        init_level(s); /* the same level again: the kills and the score are kept (reloadLevel) */
    } else if (kind == BKK_SCENE_CLEAR_WIN) {
        bk_game_next_level(g);
        load_new_level(s);
    }
}

static const bk_game_vtable_t TANK_VTABLE = {
    tank_reset, tank_update, tank_draw, NULL, NULL, tank_scene_frame, tank_scene_done,
};

bk_game_t *bk_tank_create(void *storage, const bk_game_options_t *opt)
{
    bkk_game_t *s = (bkk_game_t *)storage;
    memset(s, 0, sizeof *s);
    bk_game_base_init(&s->base, &TANK_VTABLE, opt, 1);
    bk_game_restart(&s->base);
    return &s->base;
}

/*
 * The menu clip (ADR-041, own artwork): the bottom five rows of the field seen from the side. The player's tank (left)
 * shoots a three-cell wall and an enemy (right); bullets of both sides meet and vanish, a shot through the gap destroys
 * the enemy, a second enemy drives in from above, turns to the player and meets the same end; the wall is rebuilt at the
 * loop point. Tanks are the 7-cell player's and the 6-cell enemy shapes of the game turned to the side.
 */
// clang-format off
static const char TANK_CLIP_FRAMES[][BK_CLIP_CELLS + 1] = {
    "..........""##...#....""###..#....""##...#....""..........", /* 0: the player and a wall */
    "..........""##...#....""####.#....""##...#....""..........", /* 1: the player fires at the wall */
    "..........""##...#..##""###.##.##.""##...#..##""..........", /* 2: an enemy rolls in */
    "..........""##...#..##""###....##.""##...#..##""..........", /* 3: the shot took the middle of the wall */
    "..........""##...#..##""###...###.""##...#..##""..........", /* 4: the enemy fires back through the gap */
    "..........""##...#..##""###..#.##.""##...#..##""..........",
    "..........""##...#..##""#####..##.""##...#..##""..........", /* 6: the player fires: the two bullets are about to meet */
    "..........""##...#..##""###....##.""##...#..##""..........", /* 7: both bullets are gone */
    "..........""##...#..##""####...##.""##...#..##""..........", /* 8: the player fires again */
    "..........""##...#..##""###.#..##.""##...#..##""..........",
    "..........""##...#..##""###..#.##.""##...#..##""..........", /* 10: through the gap */
    "..........""##...#..##""###...###.""##...#..##""..........", /* 11: poster: the duel */
    "..........""##...#..#.""###....#.#""##...#..#.""..........", /* 12: the enemy is destroyed */
    "..........""##...#....""###.....#.""##...#....""..........",
    "..........""##...#....""###.......""##...#....""..........",
    ".......#.#""##...#.###""###.....#.""##...#....""..........", /* 15: a second enemy drives in from above */
    "..........""##...#.#.#""###....###""##...#..#.""..........",
    "..........""##...#..##""###....##.""##...#..##""..........", /* 17: it turns to the player */
    "..........""##...#..##""###...###.""##...#..##""..........", /* 18: it fires */
    "..........""##...#..##""###..#.##.""##...#..##""..........",
    "..........""##...#..##""#####..##.""##...#..##""..........", /* 20: the player answers: the bullets meet */
    "..........""##...#..##""###....##.""##...#..##""..........",
    "..........""##...#..##""####...##.""##...#..##""..........", /* 22: the player fires */
    "..........""##...#..##""###.#..##.""##...#..##""..........",
    "..........""##...#..##""###..#.##.""##...#..##""..........",
    "..........""##...#..##""###...###.""##...#..##""..........",
    "..........""##...#..#.""###....#.#""##...#..#.""..........", /* 26: the second enemy is destroyed */
    "..........""##...#....""###.....#.""##...#....""..........",
    "..........""##...#....""###.......""##...#....""..........", /* 28: the wall is rebuilt at the loop point (next frame) */
};
// clang-format on

static const bk_clip_t TANK_CLIP = {
    (uint16_t)BK_ARRAY_LEN(TANK_CLIP_FRAMES),
    10,
    11, /* poster: the duel, a bullet in flight through the gap in the wall */
    TANK_CLIP_FRAMES,
};

const bk_game_def_t bk_tank_def = {
    BK_TANK_UID, "tank", "TANK", 1, 0, sizeof(bkk_game_t), _Alignof(bkk_game_t), bk_tank_create, &TANK_CLIP,
};
