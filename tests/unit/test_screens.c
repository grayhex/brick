/*
 * Golden screens: the logical 1-bit frame of every console screen, one character per dot ('#' ink, '.' light).
 * The files in tests/golden are reviewable pictures of the UI and fail the build when the picture changes.
 *   BK_UPDATE_GOLDEN=1   rewrite the golden files (review the diff!)
 *   BK_DUMP_DIR=dir      additionally write PBM files (tools/frames/pbm_to_png.py turns them into PNG)
 */
#include <stdlib.h>
#include <string.h>

#include "bk_test.h"
#include "brick/console.h"
#include "brick/games/racing.h"
#include "brick/games/snake.h"
#include "brick/games/tetris.h"
#include "fake_hal.h"
#include "frame_dump.h"
#include "mini_json.h"
#include "stub_game.h"

static const char *golden_dir;

static fake_hal_t hal;
static bk_console_t con;
static char ascii[BK_FRAME_MAX_W * BK_FRAME_MAX_H * 2];

static void check_golden(const char *name)
{
    const bk_frame_t *f = bk_console_render(&con);
    bk_frame_to_ascii(f, ascii, sizeof ascii);

    char path[512];
    snprintf(path, sizeof path, "%s/%s.txt", golden_dir, name);
    if (getenv("BK_DUMP_DIR") != NULL) {
        char pbm[512];
        snprintf(pbm, sizeof pbm, "%s/%s.pbm", getenv("BK_DUMP_DIR"), name);
        bk_frame_write_pbm(f, pbm);
    }
    if (getenv("BK_UPDATE_GOLDEN") != NULL) {
        FILE *fp = fopen(path, "wb");
        CHECK(fp != NULL);
        if (fp != NULL) {
            fputs(ascii, fp);
            fclose(fp);
        }
        return;
    }
    char *want = jv_read_file(path);
    if (want == NULL) {
        fprintf(stderr, "FAIL [%s] missing golden file %s (run with BK_UPDATE_GOLDEN=1)\n", name, path);
        bk_test_failures++;
        return;
    }
    bool same = strcmp(want, ascii) == 0;
    if (!same) {
        fprintf(stderr, "FAIL [%s] frame differs from %s\n", name, path);
        bk_test_failures++;
        /* first differing dot, to make the report actionable */
        size_t i = 0;
        while (want[i] != '\0' && want[i] == ascii[i]) {
            i++;
        }
        fprintf(stderr, "  first difference at byte %zu (row %zu, column %zu)\n", i, i / (f->width + 1u),
                i % (f->width + 1u));
    }
    bk_test_checks++;
    free(want);
}

/* Native-panel frames are checked by a hash of their bits (an ASCII picture of 336 x 536 dots would be 180 KB); the
 * hashes live in tests/golden/fit.hashes, one "name hex" line per frame. BK_DUMP_DIR still writes the PBM to look at. */
static void check_hash(const char *name, const bk_frame_t *f)
{
    uint32_t h = 0x811c9dc5u;
    h = (h ^ f->width) * 0x01000193u;
    h = (h ^ f->height) * 0x01000193u;
    for (size_t i = 0; i < bk_frame_bytes(f); i++) {
        h = (h ^ f->bits[i]) * 0x01000193u;
    }
    char line[64];
    snprintf(line, sizeof line, "%s %08x\n", name, h);

    char path[512];
    snprintf(path, sizeof path, "%s/fit.hashes", golden_dir);
    if (getenv("BK_DUMP_DIR") != NULL) {
        char pbm[512];
        snprintf(pbm, sizeof pbm, "%s/%s.pbm", getenv("BK_DUMP_DIR"), name);
        bk_frame_write_pbm(f, pbm);
    }
    if (getenv("BK_UPDATE_GOLDEN") != NULL) {
        static int first = 1;
        FILE *fp = fopen(path, first ? "wb" : "ab");
        first = 0;
        CHECK(fp != NULL);
        if (fp != NULL) {
            fputs(line, fp);
            fclose(fp);
        }
        return;
    }
    char *all = jv_read_file(path);
    bool ok = all != NULL && strstr(all, line) != NULL;
    if (!ok) {
        fprintf(stderr, "FAIL [%s] hash differs from %s, expected line: %s", name, path, line);
        bk_test_failures++;
    }
    bk_test_checks++;
    free(all);
}

static void boot(const bk_catalog_entry_t *cat, size_t n)
{
    CHECK(bk_console_init(&con, cat, n, &hal.hal, 8));
}

static void run(unsigned ticks, bk_buttons_t held)
{
    for (unsigned i = 0; i < ticks; i++) {
        bk_console_step(&con, held);
    }
}

static void tap(bk_buttons_t b)
{
    bk_console_step(&con, b);
    bk_console_step(&con, 0);
}

/* Show the menu picture at frame `frame` of the clip of the selected game, in the lit phase of the blinking value. */
static void show_clip_frame(unsigned frame)
{
    while ((con.ticks / BK_BLINK_HALF_TICKS) % 2u != 0u) {
        run(1, 0);
    }
    con.preview_ticks = frame * con.catalog[con.game_index].def->preview->frame_ticks;
}

/* Move the settings cursor down to `item` (the list wraps). */
static void goto_setting(bk_setting_item_t item)
{
    for (int guard = 0; con.cursor != item && guard < BK_SET_COUNT; guard++) {
        tap(BK_BTN_DOWN);
    }
    CHECK_EQ(con.cursor, item);
}

static const bk_catalog_entry_t CAT_TETRIS[] = {{&bk_tetris_def, "1"}};
static const bk_catalog_entry_t CAT_TWO[] = {{&bk_tetris_def, "1"}, {&bk_snake_def, "2"}};
static const bk_catalog_entry_t CAT_THREE[] = {{&bk_tetris_def, "1"}, {&bk_snake_def, "2"}, {&bk_racing_def, "3"}};
static const bk_catalog_entry_t CAT_MULTI[] = {{&stub_def_multi, "12"}, {&stub_def_single, "A"}};

static void seed_profile(uint16_t uid, uint16_t mode, uint32_t score, uint8_t speed, uint8_t level)
{
    bk_profile_t p;
    bk_profile_defaults(&p);
    bk_profile_set_record(&p, uid, mode, score);
    p.settings.last_speed = speed;
    p.settings.last_level = level;
    bk_save_store(&hal.hal.storage, &p);
}

static void test_splash_and_home(void)
{
    fake_hal_init(&hal);
    seed_profile(BK_TETRIS_UID, 0, 12345, 3, 1);
    boot(CAT_TETRIS, 1);
    run(1, 0);
    check_golden("splash");
    run(BK_SPLASH_TICKS, 0);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    show_clip_frame(22); /* the poster of the Tetris clip: the S above the well */
    check_golden("home_tetris");
}

static void test_home_two_digit_glyph_and_mode_focus(void)
{
    fake_hal_init(&hal);
    boot(CAT_MULTI, 2);
    run(BK_SPLASH_TICKS, 0);
    tap(BK_BTN_SELECT); /* LEVEL */
    tap(BK_BTN_SELECT); /* MODE: the mode number blinks */
    tap(BK_BTN_UP);
    tap(BK_BTN_UP); /* mode 3 */
    run(30, 0);     /* back into the lit blink phase: 2 * BK_BLINK_HALF_TICKS later */
    check_golden("home_multi_mode_focus");
}

static void play_tetris_to(unsigned ticks)
{
    for (unsigned i = 0; i < ticks; i++) {
        bk_buttons_t b = 0;
        unsigned phase = i % 90;
        if (phase < 8) {
            b = BK_BTN_LEFT;
        } else if (phase >= 20 && phase < 22) {
            b = BK_BTN_ACTION;
        } else if (phase >= 30 && phase < 70) {
            b = BK_BTN_DOWN;
        }
        bk_console_step(&con, b);
    }
}

static void test_game_screens(void)
{
    fake_hal_init(&hal);
    seed_profile(BK_TETRIS_UID, 0, 12345, 3, 4);
    boot(CAT_TETRIS, 1);
    run(BK_SPLASH_TICKS, 0);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    play_tetris_to(600);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    run(1, 0);
    check_golden("game_tetris");
    bk_console_step(&con, BK_BTN_START);
    run(1, 0);
    check_golden("game_tetris_paused");
    tap(BK_BTN_START);

    unsigned guard = 0;
    while (con.game->phase != BK_PHASE_OVER && guard++ < 40000) {
        bk_console_step(&con, (guard % 25) < 15 ? BK_BTN_DOWN : 0);
    }
    CHECK_EQ(con.game->phase, BK_PHASE_OVER);
    check_golden("game_tetris_over");
}

static void test_snake_screens(void)
{
    fake_hal_init(&hal);
    seed_profile(BK_SNAKE_UID, 0, 230, 4, 1);
    boot(CAT_TWO, 2);
    run(BK_SPLASH_TICKS, 0);
    tap(BK_BTN_RIGHT);   /* game 2 */
    show_clip_frame(22); /* the poster of the Snake clip */
    check_golden("home_snake");
    tap(BK_BTN_START);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    run(20, 0);
    tap(BK_BTN_UP);
    run(30, 0);
    tap(BK_BTN_LEFT);
    run(15, 0);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    CHECK_EQ(con.game->lives, 4);
    run(1, 0);
    while ((con.ticks / BK_BLINK_HALF_TICKS) % 2u != 0u) { /* the lit blink phase: the head and the meal show */
        run(1, 0);
    }
    CHECK_EQ(con.game->lives, 4);
    check_golden("game_snake");
    /* the clear scene after a lost life: the worm runs into the wall */
    unsigned guard = 0;
    while (con.game->lives == 4 && guard++ < 2000) {
        bk_console_step(&con, 0);
    }
    CHECK_EQ(con.game->lives, 3);
    run(20, 0);
    check_golden("game_snake_clear");
}

static void test_racing_screens(void)
{
    fake_hal_init(&hal);
    seed_profile(BK_RACING_UID, 0, 1234, 3, 1);
    boot(CAT_THREE, 3);
    run(BK_SPLASH_TICKS, 0);
    tap(BK_BTN_RIGHT);  /* game 2 */
    tap(BK_BTN_RIGHT);  /* game 3 */
    show_clip_frame(9); /* the poster of the Racing clip: the cars side by side */
    check_golden("home_racing");
    tap(BK_BTN_START);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    run(130, 0); /* the start melody (90 ticks) is over and the road runs */
    tap(BK_BTN_LEFT);
    run(15, 0);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    CHECK_EQ(con.game->lives, 4);
    check_golden("game_racing");
    /* the explosion of the first crash */
    unsigned guard = 0;
    while (!bk_game_busy(con.game) && guard++ < 5000) {
        bk_console_step(&con, 0);
    }
    CHECK(bk_game_busy(con.game));
    run(20, 0);
    check_golden("game_racing_kaboom");
}

static void test_text_screens(void)
{
    fake_hal_init(&hal);
    bk_profile_t p;
    bk_profile_defaults(&p);
    p.settings.light_level = 1; /* LOW, as in the mock-up */
    bk_save_store(&hal.hal.storage, &p);
    boot(CAT_TETRIS, 1);
    run(BK_SPLASH_TICKS, 0);
    tap(BK_BTN_MODE);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
    check_golden("settings"); /* the cursor on LIGHT: the picture of design/renders/ui-settings */
    goto_setting(BK_SET_ABOUT);
    CHECK_EQ(con.top, 3);
    check_golden("settings_end"); /* the list has scrolled: an arrow shows that it goes on above */
    goto_setting(BK_SET_BUTTONS);
    tap(BK_BTN_ACTION);
    run(1, BK_BTN_LEFT | BK_BTN_ACTION | BK_BTN_SELECT);
    CHECK_EQ(con.screen, BK_SCREEN_BUTTON_TEST);
    check_golden("buttons");
    run(1, BK_BTN_START | BK_BTN_SELECT | BK_BTN_MODE);
    run(1, 0);
    tap(BK_BTN_DOWN);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_ABOUT);
    check_golden("about");
}

/* A muted console shows the crossed speaker in the status column, and the pause box shows on the Racing field. */
static void test_muted_and_paused_pictures(void)
{
    fake_hal_init(&hal);
    bk_profile_t p;
    bk_profile_defaults(&p);
    p.settings.sound_on = 0;
    bk_save_store(&hal.hal.storage, &p);
    boot(CAT_THREE, 3);
    run(BK_SPLASH_TICKS, 0);
    show_clip_frame(9);
    tap(BK_BTN_RIGHT);
    tap(BK_BTN_RIGHT);
    show_clip_frame(9);
    check_golden("home_racing_muted");
}

/* The same console, fitted to the native panels of docs/DISPLAY_CANDIDATES.md (candidates only, none approved). */
static void test_native_panels(void)
{
    static const struct {
        const char *name;
        unsigned w, h;
    } panels[] = {{"lcd316", 336, 536}, {"lcd44", 240, 320}, {"lcd27", 240, 400}};
    for (unsigned i = 0; i < sizeof panels / sizeof panels[0]; i++) {
        fake_hal_init(&hal);
        seed_profile(BK_TETRIS_UID, 0, 123456, 3, 4);
        bk_layout_t l;
        CHECK(bk_layout_fit(&l, panels[i].w, panels[i].h));
        CHECK(bk_console_init_layout(&con, CAT_TETRIS, 1, &hal.hal, &l));
        run(BK_SPLASH_TICKS, 0);
        show_clip_frame(con.catalog[0].def->preview->poster);
        char name[64];
        snprintf(name, sizeof name, "%s_home", panels[i].name);
        check_hash(name, bk_console_render(&con));
        tap(BK_BTN_START);
        play_tetris_to(600);
        CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
        run(1, 0);
        snprintf(name, sizeof name, "%s_game", panels[i].name);
        const bk_frame_t *f = bk_console_render(&con);
        CHECK_EQ(f->width, panels[i].w);
        CHECK_EQ(f->height, panels[i].h);
        check_hash(name, f);
    }
}

int main(int argc, char **argv)
{
    golden_dir = argc > 1 ? argv[1] : "tests/golden";
    TEST_RUN(test_splash_and_home);
    TEST_RUN(test_home_two_digit_glyph_and_mode_focus);
    TEST_RUN(test_game_screens);
    TEST_RUN(test_snake_screens);
    TEST_RUN(test_racing_screens);
    TEST_RUN(test_text_screens);
    TEST_RUN(test_muted_and_paused_pictures);
    TEST_RUN(test_native_panels);
    return bk_test_finish("screens");
}
