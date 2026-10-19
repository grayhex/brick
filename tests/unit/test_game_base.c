/* Game base class (phase machine, score, flood animation, cutscenes, options) through the stub game; input helpers. */
#include <string.h>

#include "bk_test.h"
#include "stub_game.h"

static stub_game_t mem;

static bk_game_t *make(unsigned mode, unsigned speed, unsigned level, uint32_t hi)
{
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = 7;
    o.speed = (uint8_t)speed;
    o.level = (uint8_t)level;
    o.hi_score = hi;
    o.mode = (uint16_t)mode;
    return stub_def_multi.create(&mem, &o);
}

static void step(bk_game_t *g, uint8_t pressed, uint8_t held, bk_event_queue_t *q)
{
    bk_input_t in = bk_input_make(pressed, held);
    bk_game_step(g, &in, q);
}

static void test_options_are_normalised(void)
{
    bk_game_t *g = make(7, 99, 0, 0xFFFFFFFFu);
    CHECK_EQ(g->mode, 7 % 3);
    CHECK_EQ(g->speed, BK_MAX_SPEED);
    CHECK_EQ(g->level, BK_MIN_LEVEL);
    CHECK_EQ(g->hi_score, BK_MAX_SCORE);
    CHECK_EQ(g->lives, -1);
    g = make(0, 0, 55, 0);
    CHECK_EQ(g->speed, BK_MIN_SPEED);
    CHECK_EQ(g->level, BK_MAX_LEVEL);
}

static void test_phase_machine(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make(0, 1, 1, 0);
    CHECK_EQ(g->phase, BK_PHASE_READY);

    bk_events_clear(&q);
    step(g, 0, 0, &q); /* READY: a step does nothing */
    CHECK_EQ(g->tick, 0);
    bk_game_pause(g, &q); /* pausing a game that is not playing does nothing */
    CHECK_EQ(g->phase, BK_PHASE_READY);
    CHECK_EQ(q.count, 0);

    bk_game_start(g, &q);
    CHECK_EQ(g->phase, BK_PHASE_PLAYING);
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].type, BK_EV_STARTED);
    bk_events_clear(&q);
    bk_game_start(g, &q); /* starting a running game is ignored */
    CHECK_EQ(q.count, 0);

    step(g, 0, 0, &q);
    CHECK_EQ(g->tick, 1);
    bk_game_pause(g, &q);
    CHECK_EQ(g->phase, BK_PHASE_PAUSED);
    step(g, 0, 0, &q);
    CHECK_EQ(g->tick, 1); /* frozen */
    bk_game_pause(g, &q); /* no double pause event */
    CHECK_EQ(q.count, 1);
    bk_game_resume(g, &q);
    CHECK_EQ(g->phase, BK_PHASE_PLAYING);
    CHECK_EQ(q.items[1].type, BK_EV_RESUMED);

    bk_game_restart(g); /* back to READY with a fresh party */
    CHECK_EQ(g->phase, BK_PHASE_READY);
    CHECK_EQ(g->tick, 0);
    CHECK_EQ(g->score, 0);
}

static void test_score_and_record(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make(0, 1, 1, 50);
    bk_game_start(g, &q);
    bk_game_set_score(g, 20);
    CHECK_EQ(g->hi_score, 50); /* the record only moves when beaten */
    bk_game_add_score(g, 40);
    CHECK_EQ(g->score, 60);
    CHECK_EQ(g->hi_score, 60);
    bk_game_add_score(g, 0xFFFFFFFFu); /* saturates instead of wrapping */
    CHECK_EQ(g->score, BK_MAX_SCORE);
    bk_game_set_score(g, 0xFFFFFFFFu);
    CHECK_EQ(g->score, BK_MAX_SCORE);
    bk_game_restart(g);
    CHECK_EQ(g->score, 0);
    CHECK_EQ(g->hi_score, BK_MAX_SCORE); /* a new party keeps the record */
}

static void test_speed_wraps_like_the_toy(void)
{
    bk_game_t *g = make(0, 1, 1, 0);
    bk_game_set_speed(g, 11);
    CHECK_EQ(g->speed, 1);
    bk_game_set_speed(g, 0);
    CHECK_EQ(g->speed, 10);
    bk_game_set_speed(g, 5);
    CHECK_EQ(g->speed, 5);
}

static void test_game_over_flood_timing(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make(0, 1, 1, 0);
    bk_game_start(g, &q);
    bk_game_set_score(g, STUB_GOAL);
    bk_events_clear(&q);
    step(g, 0, 0, &q); /* the stub notices the goal and ends the game */
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].type, BK_EV_GAME_OVER);
    CHECK(bk_game_busy(g));
    CHECK_EQ(g->phase, BK_PHASE_PLAYING); /* the flood animation runs in PLAYING */

    /* 127 ms per row at 60 Hz is 7.62 ticks: row n is flooded at tick ceil(n * 7.62) of the animation */
    unsigned ticks = 0;
    unsigned first_row_at = 0;
    unsigned last_fill = 0;
    while (g->phase == BK_PHASE_PLAYING && ticks < 1000) {
        step(g, 0, 0, &q);
        ticks++;
        CHECK(g->over_fill >= last_fill && g->over_fill <= last_fill + 1); /* never skips a row */
        last_fill = g->over_fill;
        if (first_row_at == 0 && g->over_fill == 1) {
            first_row_at = ticks;
        }
    }
    CHECK_EQ(first_row_at, 8); /* floor(8 / 7.62) = 1 */
    CHECK_EQ(g->phase, BK_PHASE_OVER);
    CHECK_EQ(ticks, 183); /* 20 rows * 7.62 + 30 hold = 182.4 ticks, first tick at or past it */
    CHECK_EQ(g->over_fill, BK_ROWS);

    bk_game_start(g, &q); /* play again from OVER: a fresh party */
    CHECK_EQ(g->phase, BK_PHASE_PLAYING);
    CHECK_EQ(g->score, 0);
    CHECK_EQ(g->over_fill, 0);
    CHECK(!bk_game_busy(g));
}

static void test_preview_and_lives(void)
{
    bk_game_t *g = make(0, 1, 1, 0);
    uint8_t pv[BK_PREVIEW_CELLS];
    CHECK(!bk_game_preview(g, pv)); /* the stub has no preview and no lives */
    g->lives = 3;
    CHECK(bk_game_preview(g, pv)); /* lives indicator, as the donor: pairs of cells in columns 1 and 2 */
    CHECK_EQ(pv[0 * 4 + 1], 1);
    CHECK_EQ(pv[2 * 4 + 2], 1);
    CHECK_EQ(pv[3 * 4 + 1], 0);
    CHECK_EQ(pv[0], 0);
}

static void test_event_queue_overflow_is_counted(void)
{
    bk_event_queue_t q;
    bk_events_clear(&q);
    for (int i = 0; i < BK_EVENT_CAP + 3; i++) {
        bk_events_push(&q, BK_EV_MOVE, 0, 0, 0);
    }
    CHECK_EQ(q.count, BK_EVENT_CAP);
    CHECK_EQ(q.dropped, 3);
}

static void test_edge_detector_and_repeat(void)
{
    bk_edge_t e = {0};
    CHECK_EQ(bk_edge_update(&e, BK_BTN_LEFT), BK_BTN_LEFT);
    CHECK_EQ(bk_edge_update(&e, BK_BTN_LEFT), 0);
    CHECK_EQ(bk_edge_update(&e, BK_BTN_LEFT | BK_BTN_UP), BK_BTN_UP);
    CHECK_EQ(bk_edge_update(&e, 0), 0);
    CHECK_EQ(bk_edge_update(&e, BK_BTN_LEFT), BK_BTN_LEFT);

    /* repeat: edge, then after `delay` ticks every `interval` ticks, reset on release */
    uint32_t c = 0;
    bk_input_t press = bk_input_make(BK_KEY_LEFT, BK_KEY_LEFT);
    bk_input_t hold = bk_input_make(0, BK_KEY_LEFT);
    bk_input_t none = bk_input_make(0, 0);
    CHECK(bk_repeat_fire(&c, BK_KEY_LEFT, &press, 5, 2));
    int fires[12];
    for (int i = 0; i < 12; i++) {
        fires[i] = bk_repeat_fire(&c, BK_KEY_LEFT, &hold, 5, 2);
    }
    /* counts 1..12: fires at 5, 7, 9, 11 */
    static const int expect[12] = {0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    for (int i = 0; i < 12; i++) {
        CHECK_EQ(fires[i], expect[i]);
    }
    CHECK(!bk_repeat_fire(&c, BK_KEY_LEFT, &none, 5, 2));
    CHECK_EQ(c, 0);
    /* pressed during a cutscene (no edge call): the counter starts at 1 on the first held tick, like the reference */
    CHECK(!bk_repeat_fire(&c, BK_KEY_LEFT, &hold, 5, 2));
    CHECK_EQ(c, 1);
}

static void test_input_make_normalises(void)
{
    bk_input_t in = bk_input_make(BK_KEY_FIRE, 0);
    CHECK_EQ(in.held, BK_KEY_FIRE); /* pressed implies held */
}

int main(void)
{
    TEST_RUN(test_options_are_normalised);
    TEST_RUN(test_phase_machine);
    TEST_RUN(test_score_and_record);
    TEST_RUN(test_speed_wraps_like_the_toy);
    TEST_RUN(test_game_over_flood_timing);
    TEST_RUN(test_preview_and_lives);
    TEST_RUN(test_event_queue_overflow_is_counted);
    TEST_RUN(test_edge_detector_and_repeat);
    TEST_RUN(test_input_make_normalises);
    return bk_test_finish("game_base");
}
