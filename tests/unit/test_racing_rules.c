/* Racing rules, readable cases (the tick-by-tick comparison with the reference is test_racing_conformance). */
#include <string.h>

#include "bk_test.h"
#include "racing_lab.h"

static racing_lab_t lab;
static unsigned seen[64]; /* events of every type since start() */

static bk_game_t *start(uint32_t seed, unsigned speed, unsigned level)
{
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = seed;
    o.speed = (uint8_t)speed;
    o.level = (uint8_t)level;
    racing_lab_init(&lab, &o);
    memset(seen, 0, sizeof seen);
    return &lab.game.base;
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

static unsigned events(bk_event_type_t t)
{
    return seen[t];
}

/* The start melody has run; the road is empty and stays empty (the last group is far away). */
static void clear_road(void)
{
    idle(BKR_INTRO_TICKS);
    lab.game.opp_count = 0;
    lab.game.tail_valid = true;
    lab.game.tail_y = 60;
    memset(seen, 0, sizeof seen);
}

static void put_cars(const int *xy, unsigned count)
{
    racing_lab_set_cars(&lab, xy, count);
}

static void test_the_car_the_lanes_and_the_start(void)
{
    start(5, 1, 1);
    CHECK_EQ(BKR_LANE_X[0], 3);
    CHECK_EQ(BKR_LANE_X[1], 6);
    CHECK_EQ(lab.game.lane, 1);
    CHECK_EQ(lab.game.car_x, 6);
    CHECK_EQ(lab.game.base.lives, 4);
    CHECK_EQ(lab.game.base.scores_to_extra_life, 2000);
    CHECK(lab.game.opp_count >= 1);
    uint8_t cells[BK_CELLS];
    bk_game_cells(&lab.game.base, cells);
    /* the car: nose, bar, stem, wheels - seven cells with the wheels on the bottom row (row 19), centre column 6 */
    CHECK_EQ(cells[16 * 10 + 6], BK_CELL_ON);
    CHECK_EQ(cells[17 * 10 + 5], BK_CELL_ON);
    CHECK_EQ(cells[17 * 10 + 6], BK_CELL_ON);
    CHECK_EQ(cells[17 * 10 + 7], BK_CELL_ON);
    CHECK_EQ(cells[18 * 10 + 6], BK_CELL_ON);
    CHECK_EQ(cells[19 * 10 + 5], BK_CELL_ON);
    CHECK_EQ(cells[19 * 10 + 7], BK_CELL_ON);
    CHECK_EQ(cells[19 * 10 + 6], BK_CELL_OFF);
    CHECK_EQ(cells[18 * 10 + 5], BK_CELL_OFF);
}

static void test_two_cars_overlap_by_their_lit_cells(void)
{
    CHECK(bkr_cars_overlap(6, 1, 6, 1));
    CHECK(bkr_cars_overlap(6, 1, 6, 3));  /* wheels over the bar: share cells */
    CHECK(!bkr_cars_overlap(6, 1, 6, 4)); /* the nose fits between the wheels above it */
    CHECK(!bkr_cars_overlap(6, 1, 6, 5)); /* too far apart */
    CHECK(!bkr_cars_overlap(6, 1, 3, 1)); /* the two lanes side by side */
    CHECK(bkr_cars_overlap(6, 1, 4, 1));  /* one cell closer */
}

static void test_the_start_melody_freezes_the_road_and_the_keys(void)
{
    start(5, 1, 1);
    int before_x = lab.game.car_x;
    unsigned before_phase = lab.game.border_phase;
    for (unsigned i = 0; i < BKR_INTRO_TICKS - 1; i++) {
        step(BK_KEY_LEFT, BK_KEY_LEFT);
    }
    CHECK_EQ(lab.game.car_x, before_x);
    CHECK_EQ(lab.game.border_phase, before_phase);
    CHECK_EQ(events(BK_EV_MOVE), 0);
    idle(2);
    tap(BK_KEY_LEFT);
    CHECK_EQ(lab.game.car_x, 3);
}

static void test_a_lane_change_moves_the_car_and_the_edges_stop_it(void)
{
    start(5, 1, 1);
    clear_road();
    tap(BK_KEY_LEFT);
    CHECK_EQ(lab.game.lane, 0);
    CHECK_EQ(lab.game.car_x, 3);
    CHECK_EQ(events(BK_EV_MOVE), 1);
    tap(BK_KEY_LEFT);
    CHECK_EQ(lab.game.lane, 0);
    CHECK_EQ(events(BK_EV_MOVE), 1);
    tap(BK_KEY_RIGHT);
    tap(BK_KEY_RIGHT);
    CHECK_EQ(lab.game.car_x, 6);
    CHECK_EQ(events(BK_EV_MOVE), 2);
}

static void test_a_held_lane_key_changes_the_lane_once_at_the_edge(void)
{
    /* the repeat timing of a held key (5 ticks) is checked tick by tick by the conformance test; with two lanes it shows
     * only as extra, harmless tries at the edge */
    start(5, 1, 1);
    clear_road();
    for (unsigned i = 0; i < 31; i++) { /* RIGHT held: the car already is in the right lane */
        step(i == 0 ? BK_KEY_RIGHT : 0, BK_KEY_RIGHT);
    }
    CHECK_EQ(events(BK_EV_MOVE), 0);
    step(0, 0);
    for (unsigned i = 0; i < 31; i++) { /* LEFT held: one change, then the edge */
        step(i == 0 ? BK_KEY_LEFT : 0, BK_KEY_LEFT);
    }
    CHECK_EQ(events(BK_EV_MOVE), 1);
    CHECK_EQ(lab.game.lane, 0);
}

static void test_changing_into_a_lane_with_a_car_is_a_crash(void)
{
    start(5, 1, 1);
    clear_road();
    int cars[] = {3, 2};
    put_cars(cars, 1);
    tap(BK_KEY_LEFT);
    CHECK_EQ(events(BK_EV_CRASH), 1);
    CHECK_EQ(lab.game.car_x, 3); /* the car did move, then hit */
    idle(BK_KABOOM_TICKS);
    CHECK_EQ(events(BK_EV_LIFE_LOST), 1);
    CHECK_EQ(lab.game.base.lives, 3);
    idle(BK_CLEAR_LOSE_TICKS + BKR_INTRO_TICKS);
    CHECK_EQ(lab.game.car_x, 6); /* a fresh round in the start lane */
    CHECK_EQ(lab.game.base.phase, BK_PHASE_PLAYING);
}

static void test_a_car_further_up_is_no_danger(void)
{
    start(5, 1, 1);
    clear_road();
    int cars[] = {3, 12};
    put_cars(cars, 1);
    tap(BK_KEY_LEFT);
    CHECK_EQ(events(BK_EV_CRASH), 0);
    CHECK_EQ(events(BK_EV_MOVE), 1);
}

static void test_a_car_driving_into_the_player_is_a_crash(void)
{
    start(5, 1, 1);
    clear_road();
    int cars[] = {6, 3};
    put_cars(cars, 1);
    idle(BKR_ACCEL_TICKS * 4);
    CHECK_EQ(events(BK_EV_CRASH), 1);
    idle(BK_KABOOM_TICKS);
    CHECK_EQ(events(BK_EV_LIFE_LOST), 1);
}

static void test_acceleration_makes_extra_road_steps(void)
{
    start(5, 1, 1);
    clear_road();
    int cars[] = {3, 14};
    put_cars(cars, 1);
    step(BK_KEY_UP, BK_KEY_UP);
    int after_one = lab.game.opp[0].y;
    step(0, 0);
    step(BK_KEY_ROTATE, BK_KEY_ROTATE);
    step(0, 0);
    step(BK_KEY_FIRE, BK_KEY_FIRE);
    int after_three = lab.game.opp[0].y;
    /* the border moves on every extra step, the cars on every second one */
    CHECK(after_one <= 14);
    CHECK(after_three < 14);
    int y = lab.game.opp[0].y;
    for (int i = 0; i < 10; i++) {
        step(0, BK_KEY_DOWN); /* DOWN does nothing on the normal field */
    }
    CHECK(lab.game.opp[0].y >= y - 1);
}

static void test_a_passed_car_scores_and_every_fifty_points_speed_up(void)
{
    start(5, 4, 1);
    clear_road();
    bkr_set_score(&lab.game, 49);
    CHECK_EQ(lab.game.base.speed, 4);
    int cars[] = {3, -1};
    put_cars(cars, 1);
    lab.game.odd_move = false;
    for (unsigned i = 0; i < 40 && lab.game.base.score < 50; i++) {
        step(i < 12 ? BK_KEY_UP : 0, i < 12 ? BK_KEY_UP : 0);
    }
    CHECK_EQ(lab.game.base.score, 50);
    CHECK_EQ(lab.game.base.speed, 5);
    CHECK_EQ(events(BK_EV_LEVEL_UP), 1);
}

static void test_speed_ten_wraps_and_the_level_rises(void)
{
    start(5, 10, 3);
    clear_road();
    bkr_set_score(&lab.game, 49);
    bkr_set_score(&lab.game, 50);
    CHECK_EQ(lab.game.base.speed, 1);
    CHECK_EQ(lab.game.base.level, 4);
    bk_game_set_speed(&lab.game.base, 10);
    lab.game.base.level = 10;
    bkr_set_score(&lab.game, 100);
    CHECK_EQ(lab.game.base.speed, 1);
    CHECK_EQ(lab.game.base.level, 1); /* after level 10 comes level 1 */
}

static void test_an_extra_life_every_two_thousand_points(void)
{
    start(5, 1, 1);
    clear_road();
    bk_game_set_lives(&lab.game.base, 2);
    bkr_set_score(&lab.game, 1999);
    CHECK_EQ(lab.game.base.lives, 2);
    bkr_set_score(&lab.game, 2000);
    CHECK_EQ(lab.game.base.lives, 3);
    CHECK_EQ(lab.game.base.extra_lives, 1);
    bkr_set_score(&lab.game, 4000);
    CHECK_EQ(lab.game.base.lives, 4);
    bkr_set_score(&lab.game, 6000); /* four lives are full: 200 points instead */
    CHECK_EQ(lab.game.base.lives, 4);
    CHECK_EQ(lab.game.base.score, 6200);
}

static void test_four_losses_end_the_game(void)
{
    start(5, 1, 1);
    clear_road();
    bk_game_set_lives(&lab.game.base, 1);
    int cars[] = {6, 3};
    put_cars(cars, 1);
    idle(BKR_ACCEL_TICKS * 4 + BK_KABOOM_TICKS + 5);
    CHECK_EQ(events(BK_EV_GAME_OVER), 1);
    CHECK_EQ(lab.game.base.lives, 0);
    idle(300);
    CHECK_EQ(lab.game.base.phase, BK_PHASE_OVER);
}

static void test_the_road_step_follows_speed_and_level(void)
{
    CHECK_EQ(bkr_half_step_ticks(1, 1), 6);
    CHECK_EQ(bkr_half_step_ticks(10, 1), 2);
    CHECK_EQ(bkr_half_step_ticks(10, 10), 1);
    for (int level = 1; level <= 10; level++) {
        for (int speed = 1; speed < 10; speed++) {
            CHECK(bkr_half_step_ticks(speed, level) >= bkr_half_step_ticks(speed + 1, level)); /* faster never slows down */
        }
    }
}

static void test_the_preview_shows_the_lives(void)
{
    start(5, 1, 1);
    uint8_t pv[BK_PREVIEW_CELLS];
    CHECK(bk_game_preview(&lab.game.base, pv));
    for (int r = 0; r < 4; r++) {
        CHECK_EQ(pv[r * 4 + 1], BK_CELL_ON);
    }
    bk_game_set_lives(&lab.game.base, 2);
    bk_game_preview(&lab.game.base, pv);
    CHECK_EQ(pv[1 * 4 + 1], BK_CELL_ON);
    CHECK_EQ(pv[2 * 4 + 1], BK_CELL_OFF);
}

static void test_the_definition_and_menu_texts(void)
{
    CHECK_EQ(bk_racing_def.uid, BK_RACING_UID);
    CHECK_STR(bk_racing_def.id, "racing");
    CHECK_STR(bk_racing_def.title, "RACING");
    CHECK_EQ(bk_racing_def.mode_count, 1); /* one variant per game (ADR-036) */
    CHECK(bk_racing_def.state_size <= 2048);
    CHECK(bk_racing_def.preview != NULL); /* the menu clip itself is checked for every game in test_clips */
}

int main(void)
{
    TEST_RUN(test_the_car_the_lanes_and_the_start);
    TEST_RUN(test_two_cars_overlap_by_their_lit_cells);
    TEST_RUN(test_the_start_melody_freezes_the_road_and_the_keys);
    TEST_RUN(test_a_lane_change_moves_the_car_and_the_edges_stop_it);
    TEST_RUN(test_a_held_lane_key_changes_the_lane_once_at_the_edge);
    TEST_RUN(test_changing_into_a_lane_with_a_car_is_a_crash);
    TEST_RUN(test_a_car_further_up_is_no_danger);
    TEST_RUN(test_a_car_driving_into_the_player_is_a_crash);
    TEST_RUN(test_acceleration_makes_extra_road_steps);
    TEST_RUN(test_a_passed_car_scores_and_every_fifty_points_speed_up);
    TEST_RUN(test_speed_ten_wraps_and_the_level_rises);
    TEST_RUN(test_an_extra_life_every_two_thousand_points);
    TEST_RUN(test_four_losses_end_the_game);
    TEST_RUN(test_the_road_step_follows_speed_and_level);
    TEST_RUN(test_the_preview_shows_the_lives);
    TEST_RUN(test_the_definition_and_menu_texts);
    return bk_test_finish("racing_rules");
}
