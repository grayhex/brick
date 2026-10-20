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

/* ---------------------------------------------------------------- lives and clear scenes */

enum { SCENE_LOSE = 7, SCENE_WIN = 8 };

typedef struct {
    bk_game_t base;
    unsigned lose_done;
    unsigned win_done;
} life_game_t;

static life_game_t life_mem;

static void life_reset(bk_game_t *g)
{
    bk_game_set_lives(g, 3);
}

static void life_update(bk_game_t *g, const bk_input_t *in)
{
    if (in->pressed & BK_KEY_ROTATE) {
        bk_game_lose_life(g, SCENE_LOSE);
    } else if (in->pressed & BK_KEY_UP) {
        bk_game_win_level(g, SCENE_WIN);
    } else if (in->pressed & BK_KEY_DOWN) {
        g->lives = -1; /* a game without lives */
        bk_game_lose_life(g, SCENE_LOSE);
    }
}

static void life_draw(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    (void)g;
    memset(cells, 0, BK_CELLS);
}

static void life_scene_frame(bk_game_t *g, uint8_t kind, uint16_t elapsed)
{
    (void)kind;
    bk_game_paint_clear(g, BK_CLEAR_SCENE_TICKS, elapsed);
}

static void life_scene_done(bk_game_t *g, uint8_t kind)
{
    life_game_t *l = (life_game_t *)g;
    if (kind == SCENE_WIN) {
        bk_game_next_level(g);
        l->win_done++;
    } else {
        l->lose_done++;
    }
}

static const bk_game_vtable_t LIFE_VT = {life_reset, life_update, life_draw, NULL, NULL, life_scene_frame, life_scene_done};

static bk_game_t *make_life(unsigned speed, unsigned level)
{
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = 3;
    o.speed = (uint8_t)speed;
    o.level = (uint8_t)level;
    memset(&life_mem, 0, sizeof life_mem);
    bk_game_base_init(&life_mem.base, &LIFE_VT, &o, 1);
    bk_game_restart(&life_mem.base);
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_start(&life_mem.base, &q);
    return &life_mem.base;
}

static unsigned lit_rows(const bk_game_t *g, int *first_lit)
{
    uint8_t cells[BK_CELLS];
    bk_game_cells(g, cells);
    unsigned rows = 0;
    *first_lit = -1;
    for (int r = 0; r < BK_ROWS; r++) {
        bool lit = true;
        for (int c = 0; c < BK_COLS; c++) {
            lit = lit && cells[r * BK_COLS + c] != 0;
        }
        if (lit) {
            rows++;
            if (*first_lit < 0) {
                *first_lit = r;
            }
        }
    }
    return rows;
}

static void test_set_lives_is_clamped(void)
{
    bk_game_t *g = make_life(1, 1);
    CHECK_EQ(g->lives, 3);
    bk_game_set_lives(g, 9);
    CHECK_EQ(g->lives, BK_MAX_LIVES);
    bk_game_set_lives(g, -3);
    CHECK_EQ(g->lives, 0);
}

static void test_a_lost_life_runs_the_clear_scene_and_restarts_the_round(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make_life(1, 1);
    bk_events_clear(&q);
    step(g, BK_KEY_ROTATE, BK_KEY_ROTATE, &q);
    CHECK_EQ(g->lives, 2);
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].type, BK_EV_LIFE_LOST);
    CHECK(bk_game_busy(g));
    CHECK_EQ(life_mem.lose_done, 0);

    int first;
    /* the first frame is visible at once: one row flooded from the bottom (ceil(1/30 * 20) = 1) */
    CHECK_EQ(lit_rows(g, &first), 1);
    CHECK_EQ(first, BK_ROWS - 1);
    /* the keys are ignored while the scene runs */
    for (int t = 1; t < BK_CLEAR_SCENE_TICKS; t++) {
        bk_events_clear(&q);
        step(g, BK_KEY_ROTATE, BK_KEY_ROTATE, &q);
        CHECK_EQ(q.count, 0);
        CHECK_EQ(g->lives, 2);
        unsigned rows = lit_rows(g, &first);
        if (t == 29) {
            CHECK_EQ(rows, BK_ROWS); /* fully flooded at the end of the first half */
        }
        if (t == 30) {
            CHECK_EQ(rows, BK_ROWS - 1); /* then erased from the top: the top row goes out first */
            CHECK_EQ(first, 1);
        }
        if (t == 59) {
            CHECK_EQ(rows, 0);
        }
        CHECK_EQ(life_mem.lose_done, 0);
    }
    step(g, 0, 0, &q); /* the 60th tick after the loss runs scene_done */
    CHECK_EQ(life_mem.lose_done, 1);
    CHECK(!bk_game_busy(g));
}

static void test_the_last_life_ends_the_game(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make_life(1, 1);
    bk_game_set_lives(g, 1);
    bk_events_clear(&q);
    step(g, BK_KEY_ROTATE, BK_KEY_ROTATE, &q);
    CHECK_EQ(g->lives, 0);
    CHECK_EQ(q.count, 2);
    CHECK_EQ(q.items[0].type, BK_EV_LIFE_LOST);
    CHECK_EQ(q.items[1].type, BK_EV_GAME_OVER);
    CHECK(g->dying);
    CHECK(!g->scene_on);
    /* a game without lives counts as having one */
    g = make_life(1, 1);
    bk_events_clear(&q);
    step(g, BK_KEY_DOWN, BK_KEY_DOWN, &q);
    CHECK_EQ(g->lives, 0);
    CHECK(g->dying);
}

static void test_a_won_level_scores_and_advances(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make_life(3, 4);
    bk_events_clear(&q);
    step(g, BK_KEY_UP, BK_KEY_UP, &q);
    CHECK_EQ(g->score, 100);
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].type, BK_EV_WIN);
    CHECK(bk_game_busy(g));
    for (int t = 1; t < BK_CLEAR_SCENE_TICKS; t++) {
        step(g, 0, 0, &q);
    }
    bk_events_clear(&q);
    step(g, 0, 0, &q);
    CHECK_EQ(life_mem.win_done, 1);
    CHECK_EQ(g->level, 5);
    CHECK_EQ(g->speed, 3);
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].type, BK_EV_LEVEL_UP);
    CHECK_EQ(q.items[0].a, 3);
}

static void test_level_overflow_raises_the_speed_and_wraps_it(void)
{
    bk_event_queue_t q;
    bk_game_t *g = make_life(9, BK_MAX_LEVEL);
    bk_events_clear(&q);
    bk_game_next_level(g); /* outside of a step the event has nowhere to go: nothing must break */
    CHECK_EQ(g->level, BK_MIN_LEVEL);
    CHECK_EQ(g->speed, 10);
    g->out = &q;
    g->level = BK_MAX_LEVEL;
    bk_game_next_level(g);
    g->out = NULL;
    CHECK_EQ(g->level, BK_MIN_LEVEL);
    CHECK_EQ(g->speed, BK_MIN_SPEED); /* 10 + 1 wraps to 1 like the toy */
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].a, 1);
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
    TEST_RUN(test_set_lives_is_clamped);
    TEST_RUN(test_a_lost_life_runs_the_clear_scene_and_restarts_the_round);
    TEST_RUN(test_the_last_life_ends_the_game);
    TEST_RUN(test_a_won_level_scores_and_advances);
    TEST_RUN(test_level_overflow_raises_the_speed_and_wraps_it);
    return bk_test_finish("game_base");
}
