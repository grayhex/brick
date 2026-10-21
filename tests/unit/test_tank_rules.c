/* Tank rules, readable cases (the tick-by-tick comparison with the reference is test_tank_conformance). */
#include <string.h>

#include "bk_test.h"
#include "tank_lab.h"

static tank_lab_t lab;
static unsigned seen[64]; /* events of every type since start() */

static bkk_game_t *start(uint32_t seed, unsigned speed, unsigned level)
{
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = seed;
    o.speed = (uint8_t)speed;
    o.level = (uint8_t)level;
    tank_lab_init(&lab, &o);
    memset(seen, 0, sizeof seen);
    return &lab.game;
}

static void step(uint8_t pressed, uint8_t held)
{
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_input_t in = bk_input_make(pressed, held);
    bk_game_step(&lab.game.base, &in, &q);
    for (unsigned i = 0; i < q.count; i++) {
        seen[q.items[i].type & 63u]++;
    }
}

static void idle(unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        step(0, 0);
    }
}

static void tap(uint8_t key)
{
    step(key, key);
    step(0, 0);
}

static void hold(uint8_t key, unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        step(i == 0 ? key : 0, key);
    }
}

static unsigned events(bk_event_type_t t)
{
    return seen[t];
}

/* The start melody has run; no enemy is on the field and none is going to come. */
static void quiet(void)
{
    lab.game.start_left = 0;
    lab.game.ai_left = 30000;
    memset(seen, 0, sizeof seen);
}

static void put_enemy(int i, int x, int y, int dir)
{
    lab.game.enemy[i].x = (int8_t)x;
    lab.game.enemy[i].y = (int8_t)y;
    lab.game.enemy[i].dir = (uint8_t)dir;
    lab.game.enemy_live[i] = true;
    lab.game.on_board++;
}

static void put_bullet(bkk_bullet_t *b, int x, int y, int dir)
{
    b->x = (int8_t)x;
    b->y = (int8_t)y;
    b->dir = (uint8_t)dir;
    b->live = true;
}

static unsigned live_bullets(const bkk_bullet_t *list, int n)
{
    unsigned c = 0;
    for (int i = 0; i < n; i++) {
        c += list[i].live ? 1u : 0u;
    }
    return c;
}

static unsigned lit(const uint8_t *cells)
{
    unsigned n = 0;
    for (int i = 0; i < BK_CELLS; i++) {
        n += cells[i] != 0 ? 1u : 0u;
    }
    return n;
}

static double dabs(double v)
{
    return v < 0 ? -v : v;
}

/* ---------------------------------------------------------------- the start, shapes, fields */

static void test_the_start(void)
{
    start(5, 1, 1);
    CHECK_EQ(lab.game.base.lives, 4);
    CHECK_EQ(lab.game.base.scores_to_extra_life, 500);
    CHECK_EQ(lab.game.player.x, 4);
    CHECK_EQ(lab.game.player.y, 7);
    CHECK_EQ(lab.game.player.dir, BKK_UP);
    CHECK_EQ(lab.game.start_left, BKK_START_TICKS);
    CHECK_EQ(lab.game.ai_left, 1);
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 0);
    CHECK_EQ(lit(lab.game.obstacles), 0); /* no obstacles on level 1 */

    /* the drawing turns y over: donor y 8 (the barrel) is screen row 11, y 7 is row 12, y 6 is row 13 */
    uint8_t cells[BK_CELLS];
    bk_game_cells(&lab.game.base, cells);
    CHECK_EQ(lit(cells), 7);
    CHECK_EQ(cells[11 * 10 + 4], BK_CELL_ON);
    for (int x = 3; x <= 5; x++) {
        CHECK_EQ(cells[12 * 10 + x], BK_CELL_ON);
        CHECK_EQ(cells[13 * 10 + x], BK_CELL_ON);
    }
    CHECK_EQ(cells[11 * 10 + 3], BK_CELL_OFF);
}

static void test_the_tank_shapes(void)
{
    int8_t cells[9][2];
    CHECK_EQ(bkk_mask(BKK_PLAYER, BKK_UP, cells), 7);
    CHECK_EQ(bkk_mask(BKK_ENEMY, BKK_UP, cells), 6);
    CHECK_EQ(bkk_mask(BKK_BOX, BKK_UP, cells), 9);

    /* the enemy has no cell behind its centre, the player's tank has */
    bkk_tank_t t = {5, 5, BKK_UP};
    CHECK(bkk_covers(BKK_PLAYER, &t, 5, 4));
    CHECK(!bkk_covers(BKK_ENEMY, &t, 5, 4));
    CHECK(bkk_covers(BKK_ENEMY, &t, 5, 6)); /* the barrel */
    /* turned to the right the barrel is on the right, the missing cell is on the left of the centre */
    t.dir = BKK_RIGHT;
    CHECK(bkk_covers(BKK_PLAYER, &t, 6, 5));
    CHECK(!bkk_covers(BKK_ENEMY, &t, 4, 5));
    t.dir = BKK_DOWN;
    CHECK(bkk_covers(BKK_PLAYER, &t, 5, 4));
    CHECK(!bkk_covers(BKK_ENEMY, &t, 5, 6));
    t.dir = BKK_LEFT;
    CHECK(bkk_covers(BKK_PLAYER, &t, 4, 5));
    CHECK(!bkk_covers(BKK_ENEMY, &t, 6, 5));

    /* every shape stays in a 3 x 3 square around the centre, so the field is left alone when the centre is 1 cell from the
     * edge */
    for (int dir = 0; dir < 4; dir++) {
        CHECK(!bkk_out_of_board(BKK_PLAYER, dir, 1, 1));
        CHECK(!bkk_out_of_board(BKK_PLAYER, dir, 8, 18));
        CHECK(bkk_out_of_board(BKK_PLAYER, dir, 0, 5));
        CHECK(bkk_out_of_board(BKK_PLAYER, dir, 9, 5));
        CHECK(bkk_out_of_board(BKK_PLAYER, dir, 5, 0));
        CHECK(bkk_out_of_board(BKK_PLAYER, dir, 5, 19));
    }
}

static void test_the_ready_made_fields(void)
{
    /* cells on levels 1..10: none, then 12, 12, 12, 6, 18, 21, 15, 27, 24 (the donor's table, docs/GAME_TANK.md) */
    static const unsigned CELLS[11] = {0, 0, 12, 12, 12, 6, 18, 21, 15, 27, 24};
    for (int level = 1; level <= 10; level++) {
        uint8_t board[BK_CELLS];
        bkk_preset_obstacles(level, board);
        CHECK_EQ(lit(board), CELLS[level]);
        /* no field touches the place of the player's tank: the donor's overlap with random obstacles does not exist here */
        for (int y = 6; y <= 8; y++) {
            for (int x = 3; x <= 5; x++) {
                CHECK_EQ(board[y * BKK_W + x], 0);
            }
        }
    }
    start(5, 1, 7);
    CHECK_EQ(lit(lab.game.obstacles), 21); /* a start level above the first has obstacles from the very beginning */
}

static void test_the_enemy_pace(void)
{
    static const unsigned PACE[10] = {90, 82, 75, 67, 59, 52, 44, 36, 29, 21};
    for (int speed = 1; speed <= 10; speed++) {
        CHECK_EQ(bkk_enemy_ticks(speed), PACE[speed - 1]);
    }
    CHECK_EQ(bkk_enemy_ticks(0), 90);
    CHECK_EQ(bkk_enemy_ticks(99), 21);
}

/* ---------------------------------------------------------------- the player's tank */

static void test_the_intro_lets_the_player_drive_but_not_shoot(void)
{
    start(5, 1, 1);
    hold(BK_KEY_RIGHT, 12);
    CHECK_EQ(lab.game.player.dir, BKK_RIGHT);
    CHECK_EQ(lab.game.player.x, 6); /* turn at tick 1, steps at ticks 6 and 11 */
    hold(BK_KEY_ROTATE, 20);
    CHECK_EQ(events(BK_EV_SHOT), 0);
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 0);
    for (int i = 0; i < 20; i++) {
        step(0, 0);
    }
    CHECK_EQ(lab.game.start_left, BKK_START_TICKS - 52);
    idle(BKK_START_TICKS - 52 - 1);
    CHECK_EQ(lab.game.start_left, 1);
    CHECK_EQ(lab.game.on_board, 0); /* the enemies wait for the end of the intro */
    idle(1);
    CHECK_EQ(lab.game.start_left, 0);
    tap(BK_KEY_ROTATE);
    CHECK_EQ(events(BK_EV_SHOT), 1);
}

static void test_a_key_turns_on_the_spot_or_steps(void)
{
    start(5, 1, 1);
    quiet();
    tap(BK_KEY_RIGHT);
    CHECK_EQ(lab.game.player.dir, BKK_RIGHT);
    CHECK_EQ(lab.game.player.x, 4);
    tap(BK_KEY_RIGHT); /* a fresh press does not wait for the pause of the previous one */
    CHECK_EQ(lab.game.player.x, 5);
    tap(BK_KEY_DOWN);
    CHECK_EQ(lab.game.player.dir, BKK_DOWN);
    tap(BK_KEY_DOWN);
    CHECK_EQ(lab.game.player.y, 6);
    tap(BK_KEY_LEFT);
    tap(BK_KEY_LEFT);
    CHECK_EQ(lab.game.player.x, 4);
    tap(BK_KEY_UP);
    tap(BK_KEY_UP);
    CHECK_EQ(lab.game.player.y, 7);
    CHECK_EQ(lab.game.player.dir, BKK_UP);
}

static void test_a_held_key_repeats_every_five_ticks(void)
{
    start(5, 1, 1);
    quiet();
    hold(BK_KEY_UP, 1);
    CHECK_EQ(lab.game.player.y, 8); /* the tank already looks up: a step at once */
    for (int i = 0; i < 4; i++) {
        step(0, BK_KEY_UP);
    }
    CHECK_EQ(lab.game.player.y, 8);
    step(0, BK_KEY_UP);
    CHECK_EQ(lab.game.player.y, 9); /* tick 6 */
    for (int i = 0; i < 5; i++) {
        step(0, BK_KEY_UP);
    }
    CHECK_EQ(lab.game.player.y, 10); /* tick 11 */
}

static void test_the_first_free_direction_key_acts(void)
{
    start(5, 1, 1);
    quiet();
    step(BK_KEY_LEFT | BK_KEY_RIGHT, BK_KEY_LEFT | BK_KEY_RIGHT);
    CHECK_EQ(lab.game.player.dir, BKK_LEFT); /* the order is left, right, up, down */
    idle(10);
    step(BK_KEY_UP | BK_KEY_DOWN, BK_KEY_UP | BK_KEY_DOWN);
    CHECK_EQ(lab.game.player.dir, BKK_UP);
}

static void test_the_edges_and_obstacles_stop_the_tank(void)
{
    start(5, 1, 1);
    quiet();
    hold(BK_KEY_LEFT, 80);
    CHECK_EQ(lab.game.player.x, 1);
    CHECK_EQ(lab.game.player.dir, BKK_LEFT);
    hold(BK_KEY_UP, 150);
    CHECK_EQ(lab.game.player.y, 18);
    hold(BK_KEY_RIGHT, 150);
    CHECK_EQ(lab.game.player.x, 8);
    hold(BK_KEY_DOWN, 150);
    CHECK_EQ(lab.game.player.y, 1);

    start(5, 1, 1);
    quiet();
    for (int y = 6; y <= 9; y++) {
        lab.game.obstacles[y * BKK_W + 7] = 1;
    }
    hold(BK_KEY_RIGHT, 60);
    CHECK_EQ(lab.game.player.x, 5); /* the barrel stops at the obstacle */
}

static void test_a_turn_that_would_touch_something_is_made_with_a_step(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.obstacles[8 * BKK_W + 3] = 1; /* a cell only the tank turned to the right occupies */
    tap(BK_KEY_RIGHT);
    CHECK_EQ(lab.game.player.dir, BKK_RIGHT);
    CHECK_EQ(lab.game.player.x, 5); /* one cell forward with the turn */

    start(5, 1, 1);
    quiet();
    lab.game.player.x = 1;
    lab.game.obstacles[8 * BKK_W + 2] = 1; /* the same at the left edge: the step would leave the field, so no turn */
    tap(BK_KEY_LEFT);
    CHECK_EQ(lab.game.player.dir, BKK_UP);
    CHECK_EQ(lab.game.player.x, 1);
}

static void test_tanks_and_own_bullets_block_the_player(void)
{
    start(5, 1, 1);
    quiet();
    put_enemy(0, 4, 11, BKK_DOWN);
    hold(BK_KEY_UP, 30);
    CHECK_EQ(lab.game.player.y, 8); /* the barrel stops under the enemy's tank */

    start(5, 1, 1);
    quiet();
    lab.game.bullet_left = 255;
    put_bullet(&lab.game.player_bullet[0], 4, 9, BKK_UP);
    hold(BK_KEY_UP, 12);
    CHECK_EQ(lab.game.player.y, 7); /* even the player's own bullet in front blocks the step */
}

/* ---------------------------------------------------------------- the gun and the bullets */

static void test_a_shot_and_the_flight_of_the_bullet(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.bullet_left = 255;
    tap(BK_KEY_ROTATE);
    CHECK_EQ(events(BK_EV_SHOT), 1);
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 1);
    CHECK_EQ(lab.game.player_bullet[0].x, 4);
    CHECK_EQ(lab.game.player_bullet[0].y, 9); /* it appears on the cell of the barrel (4, 8) and at once steps on */

    start(5, 1, 1);
    quiet();
    tap(BK_KEY_ROTATE); /* the shared timer flies every bullet one cell per 7 ticks */
    int y0 = lab.game.player_bullet[0].y;
    idle(4);
    CHECK_EQ(lab.game.player_bullet[0].y, y0);
    idle(1);
    CHECK_EQ(lab.game.player_bullet[0].y, y0 + 1);
    idle(7);
    CHECK_EQ(lab.game.player_bullet[0].y, y0 + 2);
    idle(200);
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 0); /* gone beyond the edge */
}

static void test_the_gun_pauses_and_four_bullets_at_most(void)
{
    start(5, 1, 1);
    quiet();
    hold(BK_KEY_ROTATE, 1);
    CHECK_EQ(events(BK_EV_SHOT), 1);
    for (int i = 0; i < 13; i++) {
        step(0, BK_KEY_ROTATE);
    }
    CHECK_EQ(events(BK_EV_SHOT), 1);
    step(0, BK_KEY_ROTATE);
    CHECK_EQ(events(BK_EV_SHOT), 2); /* one shot per 14 ticks */
    /* a bullet needs 7 ticks per cell and the field is 12 cells to the edge: the first one is still in the air at tick 60 */
    for (int i = 0; i < 45; i++) {
        step(0, BK_KEY_ROTATE);
    }
    CHECK_EQ(events(BK_EV_SHOT), 4); /* four bullets in the air and no free slot: the fifth shot has to wait */
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 4);
    for (int i = 0; i < 60; i++) {
        step(0, BK_KEY_ROTATE);
    }
    CHECK(events(BK_EV_SHOT) > 4); /* the first bullet has left the field: a slot is free again */
}

static void test_fire_and_rotate_are_the_same_button(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.bullet_left = 255;
    tap(BK_KEY_FIRE);
    CHECK_EQ(events(BK_EV_SHOT), 1);
    idle(14);
    tap(BK_KEY_ROTATE);
    CHECK_EQ(events(BK_EV_SHOT), 2);
}

static void test_driving_and_firing_are_independent(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.bullet_left = 255;
    step(BK_KEY_RIGHT | BK_KEY_ROTATE, BK_KEY_RIGHT | BK_KEY_ROTATE);
    CHECK_EQ(lab.game.player.dir, BKK_RIGHT); /* the turn and the shot in the same tick */
    CHECK_EQ(events(BK_EV_SHOT), 1);
}

static void test_a_bullet_erases_the_obstacle_cell_it_hits(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.obstacles[11 * BKK_W + 4] = 1;
    lab.game.obstacles[12 * BKK_W + 4] = 1;
    tap(BK_KEY_ROTATE);
    idle(40);
    CHECK_EQ(events(BK_EV_HIT), 1);
    CHECK_EQ(lab.game.obstacles[11 * BKK_W + 4], 0);
    CHECK_EQ(lab.game.obstacles[12 * BKK_W + 4], 1);
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 0);
}

static void test_a_player_bullet_destroys_an_enemy(void)
{
    start(5, 1, 1);
    quiet();
    put_enemy(0, 4, 14, BKK_DOWN);
    tap(BK_KEY_ROTATE);
    idle(60);
    CHECK_EQ(events(BK_EV_EXPLODE), 1);
    CHECK_EQ(lab.game.enemy_live[0], false);
    CHECK_EQ(lab.game.killed, 1);
    CHECK_EQ(lab.game.on_board, 0);
    CHECK_EQ(lab.game.base.score, 1);

    /* the hit is tested on the cells of the drawing: the missing cell behind an enemy's centre lets the bullet through */
    start(5, 1, 1);
    quiet();
    put_enemy(0, 4, 14, BKK_UP);
    tap(BK_KEY_ROTATE);
    idle(60);
    CHECK_EQ(events(BK_EV_EXPLODE), 1); /* it passed the hole and hit the centre */
}

static void test_bullets_meeting_each_other(void)
{
    start(5, 1, 1);
    quiet();
    put_bullet(&lab.game.player_bullet[0], 4, 12, BKK_UP);
    put_bullet(&lab.game.enemy_bullet[0], 4, 13, BKK_DOWN);
    lab.game.bullet_left = 1;
    idle(1);
    CHECK_EQ(live_bullets(lab.game.player_bullet, BKK_PLAYER_BULLETS), 0); /* the enemy bullet stepped into the player's */
    CHECK_EQ(live_bullets(lab.game.enemy_bullet, BKK_ENEMIES), 0);
    CHECK_EQ(events(BK_EV_HIT), 1);

    start(5, 1, 1);
    quiet();
    put_bullet(&lab.game.player_bullet[0], 2, 12, BKK_UP);
    put_bullet(&lab.game.enemy_bullet[0], 2, 13, BKK_DOWN);
    put_bullet(&lab.game.enemy_bullet[1], 5, 12, BKK_RIGHT);
    put_bullet(&lab.game.enemy_bullet[2], 7, 12, BKK_LEFT);
    lab.game.bullet_left = 1;
    idle(1);
    CHECK_EQ(live_bullets(lab.game.enemy_bullet, BKK_ENEMIES), 0); /* two enemy bullets meet and both vanish too */
}

static void test_an_enemy_bullet_in_an_enemy_just_vanishes(void)
{
    start(5, 1, 1);
    quiet();
    put_enemy(0, 4, 14, BKK_DOWN);
    put_bullet(&lab.game.enemy_bullet[0], 4, 16, BKK_DOWN);
    lab.game.bullet_left = 1;
    idle(1);
    idle(7);
    CHECK_EQ(live_bullets(lab.game.enemy_bullet, BKK_ENEMIES), 0);
    CHECK_EQ(lab.game.enemy_live[0], true);
    CHECK_EQ(lab.game.killed, 0);
}

/* ---------------------------------------------------------------- the player is hit, lives, levels */

static void test_a_hit_costs_a_life_and_the_same_level_starts_again(void)
{
    start(5, 1, 4);
    quiet();
    lab.game.killed = 10;
    bk_game_set_score(&lab.game.base, 10);
    put_bullet(&lab.game.enemy_bullet[0], 4, 11, BKK_DOWN);
    lab.game.bullet_left = 1;
    idle(14); /* the bullet steps on at ticks 1, 8 and 15: (4,10), (4,9), and then into the barrel cell of the tank */
    CHECK_EQ(events(BK_EV_CRASH), 0);
    idle(1);
    CHECK_EQ(events(BK_EV_CRASH), 1);
    CHECK(bk_game_busy(&lab.game.base));
    unsigned scene = 0;
    while (bk_game_busy(&lab.game.base) && scene < 1000u) {
        step(0, 0);
        scene++;
    }
    CHECK_EQ(scene, BK_KABOOM_TICKS + BKK_CLEAR_LOSE_TICKS);
    CHECK_EQ(events(BK_EV_LIFE_LOST), 1);
    CHECK_EQ(lab.game.base.lives, 3);
    CHECK_EQ(lab.game.base.level, 4); /* the same level ... */
    CHECK_EQ(lab.game.killed, 10);    /* ... with the kills and the score kept */
    CHECK_EQ(lab.game.base.score, 10);
    CHECK_EQ(lab.game.start_left, BKK_START_TICKS); /* and the intro again */
    CHECK_EQ(lab.game.player.x, 4);
    CHECK_EQ(lab.game.player.y, 7);
    CHECK_EQ(live_bullets(lab.game.enemy_bullet, BKK_ENEMIES), 0);
}

static void test_the_last_life_ends_the_game(void)
{
    start(5, 1, 1);
    quiet();
    bk_game_set_lives(&lab.game.base, 1);
    put_bullet(&lab.game.enemy_bullet[0], 4, 11, BKK_DOWN);
    lab.game.bullet_left = 1;
    idle(BK_KABOOM_TICKS + 40);
    CHECK_EQ(events(BK_EV_GAME_OVER), 1);
    CHECK_EQ(lab.game.base.lives, 0);
    idle(400);
    CHECK_EQ(lab.game.base.phase, BK_PHASE_OVER);
}

static void test_25_kills_win_the_level(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.killed = 24;
    put_enemy(0, 4, 14, BKK_DOWN);
    tap(BK_KEY_ROTATE);
    idle(40);
    CHECK_EQ(events(BK_EV_WIN), 1);
    CHECK(bk_game_busy(&lab.game.base));
    CHECK_EQ(lab.game.base.level, 1);
    unsigned scene = 0;
    while (bk_game_busy(&lab.game.base) && scene < 1000u) {
        CHECK_EQ(lab.game.base.level, 1); /* a clear scene first ... */
        step(0, 0);
        scene++;
    }
    CHECK(scene > BKK_CLEAR_WIN_TICKS - 45u &&
          scene <= BKK_CLEAR_WIN_TICKS); /* ... of 314 ticks, the first of them already gone */
    CHECK_EQ(lab.game.base.level, 2);
    CHECK_EQ(events(BK_EV_LEVEL_UP), 1);
    CHECK_EQ(lab.game.killed, 0);
    CHECK_EQ(lit(lab.game.obstacles), 12); /* the field of level 2 */
    CHECK_EQ(lab.game.base.score, 1);      /* no bonus for a won level, only the points of the kills */
}

static void test_the_level_wraps_and_raises_the_speed(void)
{
    start(5, 10, 10);
    quiet();
    memset(lab.game.obstacles, 0, sizeof lab.game.obstacles);
    lab.game.killed = 24;
    put_enemy(0, 4, 14, BKK_DOWN);
    tap(BK_KEY_ROTATE);
    idle(40 + BKK_CLEAR_WIN_TICKS);
    CHECK_EQ(lab.game.base.level, 1);
    CHECK_EQ(lab.game.base.speed, 1); /* speed 10 wraps to 1 */

    start(5, 4, 10);
    quiet();
    memset(lab.game.obstacles, 0, sizeof lab.game.obstacles);
    lab.game.killed = 24;
    put_enemy(0, 4, 14, BKK_DOWN);
    tap(BK_KEY_ROTATE);
    idle(40 + BKK_CLEAR_WIN_TICKS);
    CHECK_EQ(lab.game.base.level, 1);
    CHECK_EQ(lab.game.base.speed, 5);
}

static void test_a_hit_is_handled_before_a_win(void)
{
    start(5, 1, 1);
    quiet();
    lab.game.killed = 25;
    lab.game.dead = true;
    idle(1);
    CHECK_EQ(events(BK_EV_CRASH), 1);
    CHECK_EQ(events(BK_EV_WIN), 0);
}

static void test_an_extra_life_every_500_points(void)
{
    start(5, 1, 1);
    quiet();
    bk_game_set_lives(&lab.game.base, 2);
    bk_game_set_score(&lab.game.base, 499);
    memset(seen, 0, sizeof seen);
    put_enemy(0, 4, 14, BKK_DOWN);
    tap(BK_KEY_ROTATE);
    idle(40);
    CHECK_EQ(lab.game.base.score, 500);
    CHECK_EQ(lab.game.base.lives, 3);
    CHECK_EQ(events(BK_EV_BONUS), 1);

    start(5, 1, 1); /* with four lives the bonus is 50 points */
    quiet();
    bk_game_set_score(&lab.game.base, 499);
    memset(seen, 0, sizeof seen);
    put_enemy(0, 4, 14, BKK_DOWN);
    tap(BK_KEY_ROTATE);
    idle(40);
    CHECK_EQ(lab.game.base.lives, 4);
    CHECK_EQ(lab.game.base.score, 550);
}

static void test_a_new_party_starts_the_circle_of_enemies_again(void)
{
    start(5, 1, 1);
    lab.game.enemy_index = 2;
    bk_game_restart(&lab.game.base);
    CHECK_EQ(lab.game.enemy_index, 0);
    CHECK_EQ(lab.game.killed, 0);
    CHECK_EQ(lab.game.start_left, BKK_START_TICKS);
}

/* ---------------------------------------------------------------- the enemies' decisions */

static uint8_t board_a[BK_CELLS];

static void test_the_direction_of_a_shot(void)
{
    bkk_tank_t player = {4, 7, BKK_UP};
    CHECK_EQ(bkk_shoot_direction(4, 15, &player), BKK_DOWN); /* above: shoots down */
    CHECK_EQ(bkk_shoot_direction(4, 2, &player), BKK_UP);    /* below: shoots up */
    CHECK_EQ(bkk_shoot_direction(9, 8, &player), BKK_LEFT);  /* the larger difference is horizontal */
    CHECK_EQ(bkk_shoot_direction(0, 8, &player), BKK_RIGHT);
    CHECK_EQ(bkk_shoot_direction(8, 11, &player), BKK_LEFT); /* 4 and 4: the vertical wins only if it is larger */
    CHECK_EQ(bkk_shoot_direction(0, 3, &player), BKK_RIGHT);
    CHECK_EQ(bkk_shoot_direction(8, 10, &player), BKK_LEFT); /* 4 and 3 */
    CHECK_EQ(bkk_shoot_direction(7, 12, &player), BKK_DOWN); /* 3 and 5 */
    CHECK_EQ(bkk_shoot_direction(4, 7, &player), BKK_RIGHT); /* the same cell (cannot happen): the donor's else-branch */
    CHECK_EQ(bkk_shoot_direction(9, 11, &player), BKK_LEFT); /* 5 and 4: horizontal */
}

static void test_directions_are_sorted_by_weight_with_the_donors_ties(void)
{
    const double w[4] = {0.75, 0.5, 0.75, 0.25};
    int order[4];
    bkk_sort_by_weight(w, order);
    CHECK_EQ(order[0], BKK_UP); /* equal weights keep the order up, right, down, left */
    CHECK_EQ(order[1], BKK_DOWN);
    CHECK_EQ(order[2], BKK_RIGHT);
    CHECK_EQ(order[3], BKK_LEFT);
    const double v[4] = {0.25, 0.5, 0.25, 0.75};
    bkk_sort_by_weight(v, order);
    CHECK_EQ(order[0], BKK_LEFT);
    CHECK_EQ(order[1], BKK_RIGHT);
    CHECK_EQ(order[2], BKK_UP);
    CHECK_EQ(order[3], BKK_DOWN);
}

static void test_move_weights(void)
{
    double w[4];
    memset(board_a, 0, sizeof board_a);
    /* an open field, the player straight below and looking up: down and the current direction up share the top weight */
    bkk_tank_t tank = {4, 10, BKK_UP};
    bkk_tank_t player = {4, 3, BKK_UP};
    bkk_move_weights(board_a, &tank, &player, w);
    CHECK(dabs(w[BKK_UP] - 0.75) < 1e-12);
    CHECK(dabs(w[BKK_RIGHT] - 0.5) < 1e-12);
    CHECK(dabs(w[BKK_DOWN] - 0.75) < 1e-12);
    CHECK(dabs(w[BKK_LEFT] - 0.25) < 1e-12); /* across the player's course */

    /* against the left edge: the way left is closed, near (-0.5) and a whole tank length away (-0.25 more) */
    tank.x = 1;
    tank.y = 10;
    tank.dir = BKK_LEFT;
    bkk_move_weights(board_a, &tank, &player, w);
    CHECK(dabs(w[BKK_LEFT] - (0.25 - 0.5 - 0.25)) < 1e-12);
    CHECK(w[BKK_UP] > 0.0 && w[BKK_DOWN] > 0.0);
}

static void test_distance_to_an_obstacle(void)
{
    memset(board_a, 0, sizeof board_a);
    board_a[12 * BKK_W + 4] = 1;
    board_a[10 * BKK_W + 7] = 1;
    CHECK_EQ(bkk_distance_to_obstacle(board_a, 4, 10, BKK_UP), 2);
    CHECK_EQ(bkk_distance_to_obstacle(board_a, 4, 10, BKK_DOWN), -1);
    CHECK_EQ(bkk_distance_to_obstacle(board_a, 4, 10, BKK_RIGHT), 3);
    CHECK_EQ(bkk_distance_to_obstacle(board_a, 4, 10, BKK_LEFT), -1);
}

static void test_the_chance_of_a_shot(void)
{
    start(5, 1, 1);
    bkk_game_t *g = &lab.game;
    memset(board_a, 0, sizeof board_a);
    const double rate = 1.0 / 13.0;

    /* in line and close: point blank is capped at 12/13 */
    g->player.x = 4;
    g->player.y = 3;
    g->player.dir = BKK_UP;
    put_enemy(0, 4, 12, BKK_DOWN);
    CHECK(dabs(bkk_shot_chance(board_a, g, 0) - 12.0 * rate) < 1e-12);

    /* diagonal, far away, the player looking up (towards an enemy above): 2/13 + 1/13 */
    g->player.x = 2;
    g->player.y = 4;
    g->enemy[0].x = 8;
    g->enemy[0].y = 10;
    g->enemy[0].dir = BKK_LEFT;
    CHECK(dabs(bkk_shot_chance(board_a, g, 0) - 3.0 * rate) < 1e-12);

    /* an obstacle three cells away on the line of fire adds (9 - 3) / 13 */
    board_a[10 * BKK_W + 5] = 1;
    CHECK(dabs(bkk_shot_chance(board_a, g, 0) - 9.0 * rate) < 1e-12);

    /* a friend in the line of fire takes 2/13 and the result cannot go below 1/13 */
    put_enemy(1, 6, 10, BKK_LEFT);
    CHECK(dabs(bkk_shot_chance(board_a, g, 0) - 1.0 * rate) < 1e-12);
    g->enemy_live[1] = false;
    CHECK(dabs(bkk_shot_chance(board_a, g, 0) - 9.0 * rate) < 1e-12); /* a dead friend does not count */
}

static void test_the_dice_of_the_move_use_a_closed_interval(void)
{
    /* The best direction has weight exactly 0.25, and the first number of the generator is exactly 0.25 too:
     * "roll <= chance" takes it with ONE roll; with "<" three more rolls would be thrown. */
    static const char *BOARD = "00000000000000011001101010000100100111100001010010001100101101010000111011010000110100000001"
                               "101001010001010000001011101001001100"
                               "000110110001000011001000000000000000011001000011001000101100100100000000";
    uint8_t board[BK_CELLS];
    for (int i = 0; i < BK_CELLS; i++) {
        board[i] = BOARD[i] == '1' ? 1u : 0u;
    }
    bkk_tank_t tank = {1, 17, BKK_UP};
    bkk_tank_t player = {6, 17, BKK_LEFT};
    double w[4];
    bkk_move_weights(board, &tank, &player, w);
    CHECK(dabs(w[BKK_DOWN] - 0.25) < 1e-12);
    bk_rng_t rng;
    bk_rng_seed(&rng, 951241320u); /* its first output is 0x40000000 = 0.25 * 2^32 */
    bk_rng_t twin = rng;
    CHECK_EQ(bk_rng_next_u32(&twin), 0x40000000u);
    CHECK_EQ(bkk_choose_move(board, &tank, &player, &rng), BKK_DOWN);
    CHECK_EQ(rng.state, twin.state); /* exactly one roll */
}

/* ---------------------------------------------------------------- appearing enemies */

static bool enemy_standing(int *x, int *y, int *dir)
{
    for (int i = 0; i < BKK_ENEMIES; i++) {
        if (lab.game.enemy_live[i]) {
            *x = lab.game.enemy[i].x;
            *y = lab.game.enemy[i].y;
            *dir = lab.game.enemy[i].dir;
            return true;
        }
    }
    return false;
}

static void test_enemies_appear_at_the_six_places(void)
{
    static const int PLACES[6][2] = {{1, 1}, {8, 1}, {1, 10}, {8, 10}, {1, 18}, {8, 18}}; /* shifted inwards */
    bool found[6] = {false, false, false, false, false, false};
    unsigned appeared = 0;
    for (uint32_t seed = 1; seed <= 400; seed++) {
        start(seed, 10, 1);
        lab.game.start_left = 0;
        lab.game.ai_left = 1;
        idle(1);
        int x, y, dir;
        if (!enemy_standing(&x, &y, &dir)) {
            CHECK_EQ(lab.game.on_board, 0);
            continue;
        }
        appeared++;
        CHECK_EQ(lab.game.on_board, 1);
        int place = -1;
        for (int p = 0; p < 6; p++) {
            if (PLACES[p][0] == x && PLACES[p][1] == y) {
                place = p;
            }
        }
        CHECK(place >= 0);
        if (place >= 0) {
            found[place] = true;
        }
        /* a newcomer looks at the player ... unless it fired at once and has already made its move */
        (void)dir;
    }
    for (int p = 0; p < 6; p++) {
        CHECK(found[p]);
    }
    /* one move of the enemies tries up to three cells of the circle in a row and each cell lets a tank appear with
     * probability 3/4: a newcomer is there in 1 - (1/4)^3 = 98 % of the first moves (not in all of them, not in 3 of 4) */
    CHECK(appeared > 400 * 95 / 100 && appeared < 400);
}

static void test_a_blocked_place_is_not_used(void)
{
    static const int PLACES[6][2] = {{1, 1}, {8, 1}, {1, 10}, {8, 10}, {1, 18}, {8, 18}};
    unsigned appeared = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        start(seed, 10, 1);
        lab.game.start_left = 0;
        lab.game.ai_left = 1;
        for (int p = 0; p < 6; p++) { /* one obstacle cell in the middle of every 3 x 3 place */
            lab.game.obstacles[PLACES[p][1] * BKK_W + PLACES[p][0]] = 1;
        }
        idle(1);
        int x, y, dir;
        appeared += enemy_standing(&x, &y, &dir) ? 1u : 0u;
    }
    CHECK_EQ(appeared, 0);
}

static void test_no_newcomer_when_kills_and_enemies_make_25(void)
{
    unsigned appeared = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        start(seed, 10, 1);
        lab.game.start_left = 0;
        lab.game.ai_left = 1;
        lab.game.killed = 23;
        lab.game.on_board = 2;
        idle(1);
        int x, y, dir;
        appeared += enemy_standing(&x, &y, &dir) ? 1u : 0u;
    }
    CHECK_EQ(appeared, 0);
    unsigned with_room = 0;
    for (uint32_t seed = 1; seed <= 200; seed++) {
        start(seed, 10, 1);
        lab.game.start_left = 0;
        lab.game.ai_left = 1;
        lab.game.killed = 22;
        lab.game.on_board = 2;
        idle(1);
        int x, y, dir;
        with_room += enemy_standing(&x, &y, &dir) ? 1u : 0u;
    }
    CHECK(with_room > 50);
}

static void test_the_definition(void)
{
    CHECK_EQ(bk_tank_def.uid, 4);
    CHECK_STR(bk_tank_def.id, "tank");
    CHECK_STR(bk_tank_def.title, "TANK");
    CHECK_EQ(bk_tank_def.mode_count, 1);
    CHECK_EQ(bk_tank_def.state_size, sizeof(bkk_game_t));
    CHECK(bk_tank_def.state_size <= 2048); /* BK_GAME_STATE_MAX */
    CHECK(bk_tank_def.preview != NULL);
}

int main(void)
{
    TEST_RUN(test_the_start);
    TEST_RUN(test_the_tank_shapes);
    TEST_RUN(test_the_ready_made_fields);
    TEST_RUN(test_the_enemy_pace);
    TEST_RUN(test_the_intro_lets_the_player_drive_but_not_shoot);
    TEST_RUN(test_a_key_turns_on_the_spot_or_steps);
    TEST_RUN(test_a_held_key_repeats_every_five_ticks);
    TEST_RUN(test_the_first_free_direction_key_acts);
    TEST_RUN(test_the_edges_and_obstacles_stop_the_tank);
    TEST_RUN(test_a_turn_that_would_touch_something_is_made_with_a_step);
    TEST_RUN(test_tanks_and_own_bullets_block_the_player);
    TEST_RUN(test_a_shot_and_the_flight_of_the_bullet);
    TEST_RUN(test_the_gun_pauses_and_four_bullets_at_most);
    TEST_RUN(test_fire_and_rotate_are_the_same_button);
    TEST_RUN(test_driving_and_firing_are_independent);
    TEST_RUN(test_a_bullet_erases_the_obstacle_cell_it_hits);
    TEST_RUN(test_a_player_bullet_destroys_an_enemy);
    TEST_RUN(test_bullets_meeting_each_other);
    TEST_RUN(test_an_enemy_bullet_in_an_enemy_just_vanishes);
    TEST_RUN(test_a_hit_costs_a_life_and_the_same_level_starts_again);
    TEST_RUN(test_the_last_life_ends_the_game);
    TEST_RUN(test_25_kills_win_the_level);
    TEST_RUN(test_the_level_wraps_and_raises_the_speed);
    TEST_RUN(test_a_hit_is_handled_before_a_win);
    TEST_RUN(test_an_extra_life_every_500_points);
    TEST_RUN(test_a_new_party_starts_the_circle_of_enemies_again);
    TEST_RUN(test_the_direction_of_a_shot);
    TEST_RUN(test_directions_are_sorted_by_weight_with_the_donors_ties);
    TEST_RUN(test_move_weights);
    TEST_RUN(test_distance_to_an_obstacle);
    TEST_RUN(test_the_chance_of_a_shot);
    TEST_RUN(test_the_dice_of_the_move_use_a_closed_interval);
    TEST_RUN(test_enemies_appear_at_the_six_places);
    TEST_RUN(test_a_blocked_place_is_not_used);
    TEST_RUN(test_no_newcomer_when_kills_and_enemies_make_25);
    TEST_RUN(test_the_definition);
    return bk_test_finish("tank_rules");
}
