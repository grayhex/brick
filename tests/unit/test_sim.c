/* PC simulator driver: HAL wiring, determinism, trace record / replay, panel rate limit, storage. */
#include "bk_test.h"
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>

static bk_sim_t *new_sim(unsigned cell, unsigned w, unsigned h, uint8_t fps, uint32_t seed)
{
    bk_sim_t *s = calloc(1, sizeof *s);
    bk_sim_config_t cfg = {cell, w, h, fps, seed};
    if (s != NULL && !bk_sim_init(s, &cfg)) {
        free(s);
        return NULL;
    }
    return s;
}

static void run(bk_sim_t *s, bk_buttons_t held, unsigned ticks)
{
    bk_sim_set_buttons(s, held);
    for (unsigned i = 0; i < ticks; i++) {
        bk_sim_tick(s);
    }
    bk_sim_set_buttons(s, 0);
}

static void tap(bk_sim_t *s, bk_buttons_t b)
{
    run(s, b, 3);
    run(s, 0, 3);
}

/* splash -> home -> game, then a few moves */
static void play_script(bk_sim_t *s)
{
    run(s, 0, 70);
    tap(s, BK_BTN_START);
    run(s, 0, 100);
    tap(s, BK_BTN_LEFT);
    tap(s, BK_BTN_LEFT);
    tap(s, BK_BTN_ACTION);
    run(s, BK_BTN_DOWN, 40);
    tap(s, BK_BTN_RIGHT);
    run(s, 0, 120);
}

static void test_init_validates_the_config(void)
{
    bk_sim_t *s = new_sim(8, 0, 0, 0, 1);
    CHECK(s != NULL);
    free(s);
    CHECK(new_sim(9, 0, 0, 0, 1) == NULL);       /* odd cell */
    CHECK(new_sim(8, 4000, 4000, 0, 1) == NULL); /* bigger than the frame buffer */
    CHECK(new_sim(8, 20, 20, 0, 1) == NULL);     /* too small for 10 x 20 cells */
    s = new_sim(0, 336, 536, 10, 1);             /* the Sharp candidate, fitted */
    CHECK(s != NULL);
    if (s != NULL) {
        run(s, 0, 2);
        const bk_frame_t *f = bk_sim_frame(s);
        CHECK(f != NULL);
        if (f != NULL) {
            CHECK_EQ(f->width, 336);
            CHECK_EQ(f->height, 536);
        }
    }
    free(s);
}

static void test_boots_and_shows_a_picture(void)
{
    bk_sim_t *s = new_sim(8, 0, 0, 0, 1);
    CHECK(bk_sim_frame(s) == NULL);
    CHECK_EQ(bk_sim_frame_hash(s), 0);
    run(s, 0, 1);
    CHECK(bk_sim_frame(s) != NULL);
    CHECK(bk_sim_frame_hash(s) != 0);
    CHECK_EQ(s->console.screen, BK_SCREEN_SPLASH);
    uint32_t splash = bk_sim_frame_hash(s);
    run(s, 0, 70);
    CHECK_EQ(s->console.screen, BK_SCREEN_HOME);
    CHECK(bk_sim_frame_hash(s) != splash);
    tap(s, BK_BTN_START);
    run(s, 0, 5);
    CHECK_EQ(s->console.screen, BK_SCREEN_GAME);
    free(s);
}

static void test_same_seed_and_input_give_the_same_pictures(void)
{
    bk_sim_t *a = new_sim(8, 0, 0, 0, 77);
    bk_sim_t *b = new_sim(8, 0, 0, 0, 77);
    play_script(a);
    play_script(b);
    CHECK_EQ(bk_sim_frame_hash(a), bk_sim_frame_hash(b));
    CHECK_EQ(a->frame_seq, b->frame_seq);
    CHECK_EQ(a->console.ticks, b->console.ticks);
    free(a);
    free(b);
}

static void test_the_seed_reaches_the_games(void)
{
    uint32_t hashes[8];
    unsigned distinct = 0;
    for (unsigned i = 0; i < 8; i++) {
        bk_sim_t *s = new_sim(8, 0, 0, 0, 1000 + i * 7919u);
        run(s, 0, 70);
        tap(s, BK_BTN_START);
        run(s, 0, 200);
        hashes[i] = bk_sim_frame_hash(s);
        free(s);
        bool seen = false;
        for (unsigned j = 0; j < i; j++) {
            seen = seen || hashes[j] == hashes[i];
        }
        distinct += seen ? 0 : 1;
    }
    CHECK(distinct >= 2); /* the first piece depends on the seed */
}

static void test_record_replay_reproduces_the_session(void)
{
    bk_sim_t *a = new_sim(8, 0, 0, 0, 5);
    CHECK(bk_sim_record_start(a));
    play_script(a);
    bk_sim_record_stop(a);
    CHECK(a->trace_len == a->console.ticks);
    CHECK(!a->trace_overflow);
    uint32_t want = bk_sim_frame_hash(a);
    uint32_t presents = a->console.presents;

    uint8_t *file = malloc(BK_SIM_TRACE_HEADER + BK_SIM_TRACE_MAX);
    size_t n = bk_sim_trace_save(a, file, BK_SIM_TRACE_HEADER + BK_SIM_TRACE_MAX);
    CHECK_EQ(n, BK_SIM_TRACE_HEADER + a->trace_len);
    CHECK(bk_sim_trace_save(a, file, n - 1) == 0); /* too small */

    bk_sim_t *b = new_sim(16, 100, 100, 1, 999); /* a different config: the trace brings its own */
    CHECK(b == NULL);
    b = new_sim(10, 0, 0, 30, 999);
    CHECK(bk_sim_trace_load(b, file, n));
    CHECK_EQ(b->cfg.cell, 8);
    CHECK_EQ(b->cfg.max_fps, 0);
    CHECK_EQ(b->cfg.seed, 5);
    CHECK(b->replaying);
    while (!bk_sim_replay_done(b)) {
        bk_sim_tick(b);
    }
    CHECK_EQ(bk_sim_frame_hash(b), want);
    CHECK_EQ(b->console.presents, presents);
    CHECK_EQ(b->console.ticks, a->console.ticks);
    bk_sim_replay_stop(b);
    CHECK(!b->replaying);
    free(file);
    free(a);
    free(b);
}

static void test_a_bad_trace_changes_nothing(void)
{
    bk_sim_t *a = new_sim(8, 0, 0, 0, 5);
    bk_sim_record_start(a);
    run(a, 0, 100);
    uint8_t good[BK_SIM_TRACE_HEADER + 200];
    size_t n = bk_sim_trace_save(a, good, sizeof good);
    CHECK_EQ(n, BK_SIM_TRACE_HEADER + 100);

    bk_sim_t *b = new_sim(8, 0, 0, 0, 9);
    run(b, 0, 10);
    uint32_t ticks = b->console.ticks;
    uint8_t bad[sizeof good];

    memcpy(bad, good, n);
    bad[0] = 'X';
    CHECK(!bk_sim_trace_load(b, bad, n));
    CHECK(!bk_sim_trace_load(b, good, BK_SIM_TRACE_HEADER - 1));
    CHECK(!bk_sim_trace_load(b, good, n - 1)); /* length does not match the tick count */
    CHECK(!bk_sim_trace_load(b, good, n + 1));
    memcpy(bad, good, n);
    bad[8] = 9; /* odd cell */
    CHECK(!bk_sim_trace_load(b, bad, n));
    memcpy(bad, good, n);
    bad[17] = 0xff; /* tick count far above BK_SIM_TRACE_MAX */
    CHECK(!bk_sim_trace_load(b, bad, n));
    CHECK_EQ(b->console.ticks, ticks);
    CHECK_EQ(b->cfg.seed, 9);
    CHECK(!b->replaying);
    free(a);
    free(b);
}

static void test_recording_stops_at_the_limit(void)
{
    bk_sim_t *s = new_sim(8, 0, 0, 0, 1);
    bk_sim_record_start(s);
    run(s, 0, BK_SIM_TRACE_MAX);
    CHECK(!s->trace_overflow);
    CHECK_EQ(s->trace_len, BK_SIM_TRACE_MAX);
    run(s, 0, 1);
    CHECK(s->trace_overflow);
    CHECK_EQ(s->trace_len, BK_SIM_TRACE_MAX);
    free(s);
}

static void test_no_recording_while_replaying(void)
{
    bk_sim_t *a = new_sim(8, 0, 0, 0, 5);
    bk_sim_record_start(a);
    run(a, 0, 20);
    uint8_t file[BK_SIM_TRACE_HEADER + 20];
    CHECK_EQ(bk_sim_trace_save(a, file, sizeof file), sizeof file);
    CHECK(bk_sim_trace_load(a, file, sizeof file));
    CHECK(!bk_sim_record_start(a));
    free(a);
}

/* Presents and waiting pictures while a piece is soft-dropped for 60 ticks, at the given panel limit. */
static void soft_drop(uint8_t fps, bool change_at_run_time, unsigned *presents, unsigned *dropped)
{
    bk_sim_t *s = new_sim(8, 0, 0, change_at_run_time ? 0 : fps, 3);
    run(s, 0, 70);
    tap(s, BK_BTN_START);
    run(s, 0, 20);
    if (change_at_run_time) {
        bk_sim_set_max_fps(s, fps);
        CHECK_EQ(s->hal.display.max_fps, fps);
    }
    uint32_t p0 = s->console.presents;
    uint32_t d0 = s->console.dropped_frames;
    run(s, BK_BTN_DOWN, 60);
    *presents = s->console.presents - p0;
    *dropped = s->console.dropped_frames - d0;
    free(s);
}

static void test_panel_rate_limit_can_change_at_run_time(void)
{
    unsigned fast, fast_dropped, slow, slow_dropped, live, live_dropped;
    soft_drop(0, false, &fast, &fast_dropped);
    soft_drop(10, false, &slow, &slow_dropped);
    soft_drop(10, true, &live, &live_dropped);
    CHECK_EQ(fast_dropped, 0);
    CHECK(fast > 20);          /* the picture changes on most ticks while the piece falls */
    CHECK(slow <= 60 / 6 + 1); /* 10 pictures per second at most */
    CHECK(slow >= 5);
    CHECK(slow_dropped > 0); /* intermediate pictures waited and were replaced */
    CHECK_EQ(live, slow);    /* changing the limit at run time is the same as booting with it */
    CHECK_EQ(live_dropped, slow_dropped);
}

static void test_storage_survives_a_reboot_but_not_a_wipe(void)
{
    bk_sim_t *s = new_sim(8, 0, 0, 0, 3);
    run(s, 0, 70);
    uint32_t writes = s->storage_writes;
    tap(s, BK_BTN_MODE);   /* settings */
    tap(s, BK_BTN_ACTION); /* SOUND: toggles and is saved */
    CHECK(s->storage_writes > writes);
    CHECK(s->slot_len[0] > 0 || s->slot_len[1] > 0);
    bool sound = s->console.profile.settings.sound_on;

    CHECK(bk_sim_reset(s, false));
    CHECK_EQ(s->console.ticks, 0);
    CHECK_EQ(s->frame_seq, 0);
    CHECK_EQ(s->console.profile.settings.sound_on, sound); /* loaded back from the surviving slots */

    bk_sim_config_t cfg = s->cfg;
    cfg.cell = 10;
    CHECK(bk_sim_configure(s, &cfg)); /* another "device", same flash */
    CHECK_EQ(s->console.profile.settings.sound_on, sound);

    CHECK(bk_sim_reset(s, true));
    CHECK_EQ(s->slot_len[0], 0);
    CHECK_EQ(s->slot_len[1], 0);
    CHECK(s->console.profile.settings.sound_on != sound); /* factory default again */
    free(s);
}

static void test_ble_request_reaches_the_front_end_and_survives_a_reboot(void)
{
    bk_sim_t *s = new_sim(8, 0, 0, 0, 3);
    run(s, 0, 70);
    CHECK(!s->ble); /* off until the owner asks */
    tap(s, BK_BTN_MODE);
    tap(s, BK_BTN_DOWN);
    tap(s, BK_BTN_DOWN);
    CHECK_EQ(s->console.cursor, BK_SET_BLE);
    tap(s, BK_BTN_ACTION);
    CHECK(s->ble);
    CHECK_EQ(s->console.profile.settings.ble_on, 1);
    CHECK(bk_sim_reset(s, false)); /* a reboot with the same flash applies the saved request at start */
    CHECK(s->ble);
    CHECK(bk_sim_reset(s, true)); /* factory defaults */
    CHECK(!s->ble);
    free(s);
}

static void test_speaker_and_front_light_reach_the_front_end(void)
{
    bk_sim_t *s = new_sim(8, 0, 0, 0, 3);
    run(s, 0, 70);
    CHECK_EQ(s->tone_seq, 0);
    CHECK_EQ(s->light, 0);
    tap(s, BK_BTN_SELECT); /* focus change: a cue */
    CHECK(s->tone_seq > 0);
    run(s, 0, 40);
    CHECK_EQ(s->tone_hz, 0); /* the cue ended */
    tap(s, BK_BTN_START);
    run(s, 0, 20);
    tap(s, BK_BTN_SELECT); /* in a game: front light */
    CHECK_EQ(s->light, 1);
    free(s);
}

/* Recorded sessions (tests/fixtures/sim, written by tools/sim/make_traces.py) must give the recorded result. The same
 * files and numbers are checked for WebAssembly by tests/web/test_wasm.mjs. */
static void test_recorded_sessions_reproduce(const char *dir)
{
    char path[512];
    snprintf(path, sizeof path, "%s/expected.txt", dir);
    FILE *list = fopen(path, "r");
    CHECK(list != NULL);
    if (list == NULL) {
        return;
    }
    char line[256];
    unsigned sessions = 0;
    bk_sim_t *s = calloc(1, sizeof *s);
    while (fgets(line, sizeof line, list) != NULL) {
        char name[128];
        unsigned ticks, presents, dropped, hash;
        if (line[0] == '#' || sscanf(line, "%127s %u %u %u %x", name, &ticks, &presents, &dropped, &hash) != 5) {
            continue;
        }
        sessions++;
        snprintf(path, sizeof path, "%s/%s", dir, name);
        FILE *fp = fopen(path, "rb");
        CHECK(fp != NULL);
        if (fp == NULL) {
            continue;
        }
        size_t len = fread(s->io, 1, sizeof s->io, fp);
        fclose(fp);
        CHECK(bk_sim_trace_load(s, s->io, len));
        while (!bk_sim_replay_done(s)) {
            bk_sim_tick(s);
        }
        CHECK_EQ(s->console.ticks, ticks);
        CHECK_EQ(s->console.presents, presents);
        CHECK_EQ(s->console.dropped_frames, dropped);
        CHECK_EQ((long long)bk_sim_frame_hash(s), (long long)hash);
    }
    CHECK(sessions >= 3);
    free(s);
    fclose(list);
}

int main(int argc, char **argv)
{
    TEST_RUN(test_init_validates_the_config);
    TEST_RUN(test_boots_and_shows_a_picture);
    TEST_RUN(test_same_seed_and_input_give_the_same_pictures);
    TEST_RUN(test_the_seed_reaches_the_games);
    TEST_RUN(test_record_replay_reproduces_the_session);
    TEST_RUN(test_a_bad_trace_changes_nothing);
    TEST_RUN(test_recording_stops_at_the_limit);
    TEST_RUN(test_no_recording_while_replaying);
    TEST_RUN(test_panel_rate_limit_can_change_at_run_time);
    TEST_RUN(test_storage_survives_a_reboot_but_not_a_wipe);
    TEST_RUN(test_speaker_and_front_light_reach_the_front_end);
    TEST_RUN(test_ble_request_reaches_the_front_end_and_survives_a_reboot);
    if (argc > 1) {
        bk_test_current = "test_recorded_sessions_reproduce";
        test_recorded_sessions_reproduce(argv[1]);
    }
    return bk_test_finish("sim");
}
