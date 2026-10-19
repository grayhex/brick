/* Sound sequencer: timing, priority, mute, event mapping. */
#include "bk_test.h"
#include "brick/audio.h"
#include "fake_hal.h"

static fake_hal_t hal;
static bk_audio_t audio;

static void setup(bool enabled)
{
    fake_hal_init(&hal);
    bk_audio_init(&audio, &hal.hal.audio, enabled);
}

static void test_cue_plays_notes_in_order_with_exact_duration(void)
{
    setup(true);
    bk_audio_play(&audio, BK_CUE_CONFIRM);
    CHECK_EQ(hal.tones[0], 660); /* the first note sounds at once */
    unsigned ticks = 0;
    while (bk_audio_busy(&audio) && ticks < 1000) {
        bk_audio_tick(&audio);
        ticks++;
    }
    CHECK_EQ(ticks, bk_audio_cue_ticks(BK_CUE_CONFIRM));
    CHECK_EQ(hal.tone_count, bk_audio_cue_note_count(BK_CUE_CONFIRM) + 1); /* notes + final silence */
    CHECK_EQ(hal.tones[1], 880);
    CHECK_EQ(hal.tones[2], 1320);
    CHECK_EQ(hal.tone_now, 0);
}

static void test_priority(void)
{
    setup(true);
    bk_audio_play(&audio, BK_CUE_GAME_OVER);
    bk_audio_play(&audio, BK_CUE_MOVE); /* lower priority: ignored */
    CHECK_EQ(audio.cue, BK_CUE_GAME_OVER);
    bk_audio_play(&audio, BK_CUE_GAME_OVER); /* equal priority restarts */
    CHECK_EQ(audio.note, 0);
    setup(true);
    bk_audio_play(&audio, BK_CUE_MOVE);
    bk_audio_play(&audio, BK_CUE_LOCK); /* higher priority interrupts */
    CHECK_EQ(audio.cue, BK_CUE_LOCK);
    bk_audio_play(&audio, BK_CUE_TETRIS);
    CHECK_EQ(audio.cue, BK_CUE_TETRIS);
    CHECK(bk_audio_cue_priority(BK_CUE_TETRIS) > bk_audio_cue_priority(BK_CUE_LINE));
    CHECK(bk_audio_cue_priority(BK_CUE_GAME_OVER) > bk_audio_cue_priority(BK_CUE_TETRIS));
}

static void test_mute(void)
{
    setup(false);
    bk_audio_play(&audio, BK_CUE_CONFIRM);
    CHECK_EQ(hal.tone_count, 0);
    CHECK(!bk_audio_busy(&audio));
    bk_audio_set_enabled(&audio, true);
    bk_audio_play(&audio, BK_CUE_CONFIRM);
    CHECK(bk_audio_busy(&audio));
    bk_audio_set_enabled(&audio, false); /* muting cuts the running cue at once */
    CHECK(!bk_audio_busy(&audio));
    CHECK_EQ(hal.tone_now, 0);
}

static void test_no_redundant_port_calls(void)
{
    setup(true);
    bk_audio_tick(&audio); /* idle ticks never touch the port */
    CHECK_EQ(hal.tone_count, 0);
    bk_audio_play(&audio, BK_CUE_MOVE);
    while (bk_audio_busy(&audio)) {
        bk_audio_tick(&audio);
    }
    CHECK_EQ(hal.tone_count, 2); /* 600 Hz, then silence */
}

static void test_null_port_is_safe(void)
{
    bk_audio_t a;
    bk_audio_init(&a, NULL, true);
    bk_audio_play(&a, BK_CUE_CONFIRM);
    for (int i = 0; i < 100; i++) {
        bk_audio_tick(&a);
    }
    CHECK(!bk_audio_busy(&a));
}

static void test_event_mapping(void)
{
    bk_event_t e = {0, 0, 0, 0};
    e.type = BK_EV_MOVE;
    CHECK_EQ(bk_audio_cue_for_event(&e, true), BK_CUE_SELECT);
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_MOVE);
    e.type = BK_EV_LINES;
    e.a = 1;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_LINE);
    e.a = 3;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_LINE);
    e.a = 4;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_TETRIS);
    e.type = BK_EV_GAME_OVER;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_GAME_OVER);
    e.type = BK_EV_ROTATE;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_ROTATE);
    e.type = BK_EV_LOCK;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_LOCK);
    e.type = BK_EV_LEVEL_UP;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_LEVEL_UP);
    e.type = BK_EV_SHOT; /* not used by Tetris: silent until a game that needs it defines a cue */
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_NONE);
    /* every cue has at least one note and a positive duration */
    for (int c = BK_CUE_SELECT; c < BK_CUE_COUNT; c++) {
        CHECK(bk_audio_cue_note_count((bk_cue_t)c) > 0);
        CHECK(bk_audio_cue_ticks((bk_cue_t)c) > 0);
    }
}

int main(void)
{
    TEST_RUN(test_cue_plays_notes_in_order_with_exact_duration);
    TEST_RUN(test_priority);
    TEST_RUN(test_mute);
    TEST_RUN(test_no_redundant_port_calls);
    TEST_RUN(test_null_port_is_safe);
    TEST_RUN(test_event_mapping);
    return bk_test_finish("audio");
}
