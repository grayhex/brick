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
    e.type = BK_EV_NONE;
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_NONE);
    e.type = BK_EV_TONE; /* a raw tone request: no game sends it yet */
    CHECK_EQ(bk_audio_cue_for_event(&e, false), BK_CUE_NONE);
    /* every cue has at least one note and a positive duration */
    for (int c = BK_CUE_SELECT; c < BK_CUE_COUNT; c++) {
        CHECK(bk_audio_cue_note_count((bk_cue_t)c) > 0);
        CHECK(bk_audio_cue_ticks((bk_cue_t)c) > 0);
    }
}

/* Every event the four games send makes a sound (Tank, Racing and Snake were silent for these before ADR-045). */
static void test_game_events_have_cues(void)
{
    static const struct {
        bk_event_type_t event;
        bk_cue_t cue;
    } MAP[] = {
        {BK_EV_SHOT, BK_CUE_SHOT},
        {BK_EV_HIT, BK_CUE_HIT},
        {BK_EV_EXPLODE, BK_CUE_EXPLODE},
        {BK_EV_CRASH, BK_CUE_CRASH},
        {BK_EV_LIFE_LOST, BK_CUE_LIFE_LOST},
        {BK_EV_BONUS, BK_CUE_BONUS},
        {BK_EV_WIN, BK_CUE_WIN},
    };
    for (unsigned i = 0; i < sizeof MAP / sizeof MAP[0]; i++) {
        bk_event_t e = {(uint8_t)MAP[i].event, 0, 0, 0};
        CHECK_EQ(bk_audio_cue_for_event(&e, false), MAP[i].cue);
        CHECK_EQ(bk_audio_cue_for_event(&e, true), MAP[i].cue); /* the same in the menu: games never send them there */
    }
}

/* The placeholder tones stay where a small speaker still speaks, and the cues that repeat stay short. */
static void test_every_cue_is_in_the_speaker_range(void)
{
    for (int c = BK_CUE_SELECT; c < BK_CUE_COUNT; c++) {
        setup(true);
        bk_audio_play(&audio, (bk_cue_t)c);
        unsigned guard = 0;
        while (bk_audio_busy(&audio) && guard++ < 1000) {
            bk_audio_tick(&audio);
        }
        CHECK(guard < 1000);
        CHECK_EQ(hal.tone_now, 0); /* every cue ends in silence */
        for (unsigned i = 0; i + 1 < hal.tone_count; i++) {
            CHECK(hal.tones[i] >= 200 && hal.tones[i] <= 4000);
        }
        CHECK(bk_audio_cue_ticks((bk_cue_t)c) <= 60); /* no cue longer than a second */
    }
    CHECK(bk_audio_cue_ticks(BK_CUE_SHOT) <
          14); /* a held fire key shoots every 14 ticks: the shot is over before the next */
    CHECK(bk_audio_cue_ticks(BK_CUE_MOVE) < 14);
}

/* Several events in one tick play in order; the more important sound is what is left. */
static void test_the_more_important_sound_wins_within_a_tick(void)
{
    CHECK(bk_audio_cue_priority(BK_CUE_GAME_OVER) > bk_audio_cue_priority(BK_CUE_CRASH));
    CHECK(bk_audio_cue_priority(BK_CUE_CRASH) > bk_audio_cue_priority(BK_CUE_EXPLODE));
    CHECK(bk_audio_cue_priority(BK_CUE_EXPLODE) > bk_audio_cue_priority(BK_CUE_HIT));
    CHECK(bk_audio_cue_priority(BK_CUE_HIT) > bk_audio_cue_priority(BK_CUE_SHOT));
    CHECK(bk_audio_cue_priority(BK_CUE_BONUS) > bk_audio_cue_priority(BK_CUE_EXPLODE));
    /* a bullet kills an enemy: HIT, then EXPLODE (and BONUS when it is the 500th point) in the same tick */
    setup(true);
    bk_audio_play(&audio, BK_CUE_HIT);
    bk_audio_play(&audio, BK_CUE_EXPLODE);
    CHECK_EQ(audio.cue, BK_CUE_EXPLODE);
    bk_audio_play(&audio, BK_CUE_BONUS);
    CHECK_EQ(audio.cue, BK_CUE_BONUS);
    /* the player is hit: HIT, CRASH; a shot at the same moment is not heard over the explosion */
    setup(true);
    bk_audio_play(&audio, BK_CUE_HIT);
    bk_audio_play(&audio, BK_CUE_CRASH);
    bk_audio_play(&audio, BK_CUE_SHOT);
    CHECK_EQ(audio.cue, BK_CUE_CRASH);
    /* the last life: GAME_OVER follows LIFE_LOST in the same tick and is the one that remains */
    bk_audio_play(&audio, BK_CUE_LIFE_LOST);
    bk_audio_play(&audio, BK_CUE_GAME_OVER);
    CHECK_EQ(audio.cue, BK_CUE_GAME_OVER);
}

int main(void)
{
    TEST_RUN(test_cue_plays_notes_in_order_with_exact_duration);
    TEST_RUN(test_priority);
    TEST_RUN(test_mute);
    TEST_RUN(test_no_redundant_port_calls);
    TEST_RUN(test_null_port_is_safe);
    TEST_RUN(test_event_mapping);
    TEST_RUN(test_game_events_have_cues);
    TEST_RUN(test_every_cue_is_in_the_speaker_range);
    TEST_RUN(test_the_more_important_sound_wins_within_a_tick);
    return bk_test_finish("audio");
}
