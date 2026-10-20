/* Snake rules, readable cases (the tick-by-tick comparison with the reference is test_snake_conformance). */
#include <string.h>

#include "bk_test.h"
#include "snake_lab.h"

static snake_lab_t lab;
static bk_event_t log_[64];
static unsigned log_n;

static bk_game_options_t options(uint32_t seed, unsigned speed, unsigned level)
{
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = seed;
    o.speed = (uint8_t)speed;
    o.level = (uint8_t)level;
    return o;
}

static bk_game_t *start(uint32_t seed, unsigned speed, unsigned level)
{
    bk_game_options_t o = options(seed, speed, level);
    snake_lab_init(&lab, &o);
    log_n = 0;
    return &lab.game.base;
}

static void step(uint8_t pressed, uint8_t held)
{
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_input_t in = bk_input_make(pressed, held);
    bk_game_step(&lab.game.base, &in, &q);
    log_n = 0;
    for (unsigned i = 0; i < q.count && log_n < 64; i++) {
        log_[log_n++] = q.items[i];
    }
}

static void idle(unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        step(0, 0);
    }
}

static unsigned count_events(bk_event_type_t t)
{
    unsigned n = 0;
    for (unsigned i = 0; i < log_n; i++) {
        n += log_[i].type == t;
    }
    return n;
}

static int head_x(void)
{
    return lab.game.body[0] % 10;
}

static int head_y(void)
{
    return lab.game.body[0] / 10;
}

static void cells_of(uint8_t cells[BK_CELLS])
{
    bk_game_cells(&lab.game.base, cells);
}

static void test_the_start_position_is_the_one_of_the_rom(void)
{
    start(5, 1, 1);
    CHECK_EQ(lab.game.len, 3);
    CHECK_EQ(lab.game.body[0], 19 * 10 + 5); /* head (5,0) counting y from the bottom */
    CHECK_EQ(lab.game.body[2], 19 * 10 + 3); /* tail (3,0) */
    CHECK_EQ(lab.game.dir, 1);
    CHECK_EQ(lab.game.base.lives, 4);
    uint8_t cells[BK_CELLS];
    cells_of(cells);
    CHECK_EQ(cells[19 * 10 + 5], BK_CELL_BLINK); /* the head blinks */
    CHECK_EQ(cells[19 * 10 + 4], BK_CELL_ON);
    CHECK_EQ(cells[19 * 10 + 3], BK_CELL_ON);
    CHECK_EQ(cells[lab.game.food], BK_CELL_BLINK);
    CHECK(lab.game.food != 19 * 10 + 5 && lab.game.food != 19 * 10 + 4 && lab.game.food != 19 * 10 + 3);
    int lit = 0;
    for (int i = 0; i < BK_CELLS; i++) {
        lit += cells[i] != 0;
    }
    CHECK_EQ(lit, 4); /* the three cells of the worm and one meal */
}

static void test_the_step_period_is_fractional_and_exact(void)
{
    const double want[10] = {19.94148, 17.96004, 15.97896, 14.02584, 12.04428, 10.09068, 8.10936, 6.12768, 4.20336, 2.22228};
    for (int s = 1; s <= 10; s++) {
        double got = bks_step_ticks(s);
        CHECK(got > want[s - 1] - 1e-9 && got < want[s - 1] + 1e-9);
        if (s > 1) {
            CHECK(got < bks_step_ticks(s - 1));
        }
    }
    /* speed 1: the first step is on the 20th tick (19.94 ticks), not on the 19th */
    start(5, 1, 1);
    unsigned first = 0;
    for (unsigned t = 1; t <= 40 && first == 0; t++) {
        step(0, 0);
        if (count_events(BK_EV_MOVE) > 0) {
            first = t;
        }
    }
    CHECK_EQ(first, 20);
    CHECK_EQ(head_x(), 6);
}

static void test_a_direction_key_moves_at_once_and_restarts_the_timer(void)
{
    start(5, 1, 1);
    idle(10);
    step(BK_KEY_UP, BK_KEY_UP);
    CHECK_EQ(head_x(), 5);
    CHECK_EQ(head_y(), 18);
    CHECK_EQ(count_events(BK_EV_MOVE), 1);
    step(0, 0);
    /* the next automatic step comes a whole period later: nothing for ~19 ticks */
    unsigned quiet = 0;
    while (quiet < 40) {
        step(0, 0);
        if (count_events(BK_EV_MOVE) > 0) {
            break;
        }
        quiet++;
    }
    CHECK_EQ(quiet, 18);
}

static void test_the_opposite_key_turns_the_worm_round_without_a_step(void)
{
    start(5, 1, 1);
    step(BK_KEY_LEFT, BK_KEY_LEFT);
    CHECK_EQ(count_events(BK_EV_MOVE), 0);
    CHECK_EQ(lab.game.body[0], 19 * 10 + 3); /* the old tail is the head now */
    CHECK_EQ(lab.game.body[2], 19 * 10 + 5);
    CHECK_EQ(lab.game.dir, 3);
    /* a bent worm: the direction comes from the new head and its neighbour */
    int bent[] = {5 * 10 + 5, 5 * 10 + 4, 6 * 10 + 4, 7 * 10 + 4};
    snake_lab_set_worm(&lab, 1, bent, 4);
    step(BK_KEY_LEFT, BK_KEY_LEFT);
    CHECK_EQ(lab.game.body[0], 7 * 10 + 4);
    CHECK_EQ(lab.game.dir, 2); /* the new head (4,7) continues away from its neighbour (4,6): down along column 4 */
}

static void test_rotate_is_up_and_holding_it_is_not_a_turbo(void)
{
    start(5, 1, 1);
    idle(3);
    step(BK_KEY_ROTATE, BK_KEY_ROTATE);
    CHECK_EQ(head_y(), 18);
    CHECK_EQ(lab.game.dir, 0);
    unsigned moves = 0;
    for (int i = 0; i < 30; i++) {
        step(0, BK_KEY_ROTATE);
        moves += count_events(BK_EV_MOVE);
    }
    CHECK_EQ(moves, 1); /* one automatic step in 30 ticks at speed 1, no repeat */
    start(5, 1, 1);
    step(BK_KEY_FIRE, BK_KEY_FIRE); /* the engine alias of the action key */
    CHECK_EQ(lab.game.dir, 0);
}

static void test_a_meal_scores_grows_and_a_new_one_appears_on_a_free_cell(void)
{
    for (uint32_t seed = 1; seed <= 200; seed++) {
        start(seed, 1, 1);
        int worm[] = {10 * 10 + 4, 10 * 10 + 3, 10 * 10 + 2};
        snake_lab_set_worm(&lab, 1, worm, 3);
        lab.game.food = 10 * 10 + 5;
        step(BK_KEY_RIGHT, BK_KEY_RIGHT);
        CHECK_EQ(lab.game.base.score, 10);
        CHECK_EQ(lab.game.len, 4);
        CHECK_EQ(count_events(BK_EV_BONUS), 1);
        bool on_body = false;
        for (unsigned i = 0; i < lab.game.len; i++) {
            on_body = on_body || lab.game.body[i] == lab.game.food;
        }
        CHECK(!on_body);
    }
}

static void test_leaving_the_field_or_hitting_the_body_costs_a_life(void)
{
    start(5, 1, 1);
    int worm[] = {5, 15, 25};
    snake_lab_set_worm(&lab, 0, worm, 3);
    step(BK_KEY_UP, BK_KEY_UP);
    CHECK_EQ(lab.game.base.lives, 3);
    CHECK_EQ(count_events(BK_EV_LIFE_LOST), 1);
    CHECK(bk_game_busy(&lab.game.base));
    idle(BK_CLEAR_SCENE_TICKS); /* scene runs, then the round restarts with a fresh worm */
    CHECK(!bk_game_busy(&lab.game.base));
    CHECK_EQ(lab.game.len, 3);
    CHECK_EQ(lab.game.body[0], 19 * 10 + 5);

    int coil[] = {44, 54, 55, 45,
                  35, 34, 24, 25}; /* head (4,4); the cell to its right, (5,4), is the fourth cell of the body */
    snake_lab_set_worm(&lab, 1, coil, 8);
    lab.game.food = 0;
    step(BK_KEY_RIGHT, BK_KEY_RIGHT);
    CHECK_EQ(lab.game.base.lives, 2);

    /* the cell the tail leaves is free */
    start(5, 1, 1);
    int square[] = {44, 45, 55, 54}; /* a 2 x 2 worm: the head (4,4) is right above its own tail (4,5) */
    snake_lab_set_worm(&lab, 3, square, 4);
    lab.game.food = 0;
    step(BK_KEY_DOWN, BK_KEY_DOWN);
    CHECK_EQ(lab.game.base.lives, 4); /* the tail moves away, so the cell is free */
    CHECK_EQ(lab.game.body[0], 54);
}

static void test_four_losses_end_the_game(void)
{
    start(5, 1, 1);
    for (int life = 4; life > 0; life--) {
        int worm[] = {5, 15, 25};
        snake_lab_set_worm(&lab, 0, worm, 3);
        step(BK_KEY_UP, BK_KEY_UP);
        CHECK_EQ(lab.game.base.lives, life - 1);
        if (life > 1) {
            idle(BK_CLEAR_SCENE_TICKS);
        }
    }
    CHECK(lab.game.base.dying);
    CHECK_EQ(count_events(BK_EV_GAME_OVER), 1);
    unsigned guard = 0;
    while (lab.game.base.phase != BK_PHASE_OVER && guard++ < 400) {
        step(0, 0);
    }
    CHECK_EQ(lab.game.base.phase, BK_PHASE_OVER);
    CHECK(guard > 150 && guard < 200); /* flood: 20 rows at 7.62 ticks plus the 30 tick hold = 183 ticks */
}

static void test_the_thirteenth_meal_wins_the_level(void)
{
    start(5, 3, 4);
    int worm[] = {10 * 10 + 4, 10 * 10 + 3, 10 * 10 + 2};
    snake_lab_set_worm(&lab, 1, worm, 3);
    lab.game.eaten = 12;
    lab.game.food = 10 * 10 + 5;
    step(BK_KEY_RIGHT, BK_KEY_RIGHT);
    CHECK_EQ(lab.game.base.score, 110); /* 10 for the meal, 100 for the level */
    CHECK_EQ(count_events(BK_EV_BONUS), 1);
    CHECK_EQ(count_events(BK_EV_WIN), 1);
    idle(BK_CLEAR_SCENE_TICKS - 1);
    step(0, 0);
    CHECK_EQ(count_events(BK_EV_LEVEL_UP), 1);
    CHECK_EQ(lab.game.base.level, 5);
    CHECK_EQ(lab.game.base.speed, 3);
    CHECK_EQ(lab.game.len, 3); /* a fresh round */
    CHECK_EQ(lab.game.eaten, 0);
}

static void test_the_preview_shows_the_lives(void)
{
    start(5, 1, 1);
    uint8_t pv[BK_PREVIEW_CELLS];
    CHECK(bk_game_preview(&lab.game.base, pv));
    for (int r = 0; r < 4; r++) {
        CHECK_EQ(pv[r * 4 + 1], BK_CELL_ON);
        CHECK_EQ(pv[r * 4 + 2], BK_CELL_ON);
    }
    bk_game_set_lives(&lab.game.base, 2);
    bk_game_preview(&lab.game.base, pv);
    CHECK_EQ(pv[1 * 4 + 1], BK_CELL_ON);
    CHECK_EQ(pv[2 * 4 + 1], BK_CELL_OFF);
}

static void test_the_definition_and_menu_texts(void)
{
    CHECK_EQ(bk_snake_def.uid, BK_SNAKE_UID);
    CHECK_STR(bk_snake_def.id, "snake");
    CHECK_STR(bk_snake_def.title, "SNAKE");
    CHECK_EQ(bk_snake_def.mode_count, 1); /* one variant per game (ADR-036) */
    CHECK(bk_snake_def.state_size <= 2048);
    CHECK(bk_snake_def.preview != NULL); /* the menu clip itself is checked for every game in test_clips */
}

int main(void)
{
    TEST_RUN(test_the_start_position_is_the_one_of_the_rom);
    TEST_RUN(test_the_step_period_is_fractional_and_exact);
    TEST_RUN(test_a_direction_key_moves_at_once_and_restarts_the_timer);
    TEST_RUN(test_the_opposite_key_turns_the_worm_round_without_a_step);
    TEST_RUN(test_rotate_is_up_and_holding_it_is_not_a_turbo);
    TEST_RUN(test_a_meal_scores_grows_and_a_new_one_appears_on_a_free_cell);
    TEST_RUN(test_leaving_the_field_or_hitting_the_body_costs_a_life);
    TEST_RUN(test_four_losses_end_the_game);
    TEST_RUN(test_the_thirteenth_meal_wins_the_level);
    TEST_RUN(test_the_preview_shows_the_lives);
    TEST_RUN(test_the_definition_and_menu_texts);
    return bk_test_finish("snake_rules");
}
