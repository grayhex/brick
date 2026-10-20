/*
 * Console state machine: boot, menu navigation (games, modes, speed, level), start, controls mapping, pause/exit,
 * records, settings, front light, presenting only on change. Uses stub games to prove the console is game-agnostic.
 */
#include <string.h>

#include "bk_test.h"
#include "brick/console.h"
#include "brick/games/tetris.h"
#include "fake_hal.h"
#include "stub_game.h"
#include "tetris_internal.h"

static const bk_catalog_entry_t CAT_STUBS[] = {{&stub_def_multi, "2"}, {&stub_def_single, "A"}};
static const bk_catalog_entry_t CAT_TETRIS[] = {{&bk_tetris_def, "1"}};

static fake_hal_t hal;
static bk_console_t con;

static void boot(const bk_catalog_entry_t *cat, size_t n)
{
    CHECK(bk_console_init(&con, cat, n, &hal.hal, 8));
}

static void fresh(const bk_catalog_entry_t *cat, size_t n)
{
    fake_hal_init(&hal);
    boot(cat, n);
}

static void idle(unsigned n)
{
    for (unsigned i = 0; i < n; i++) {
        bk_console_step(&con, 0);
    }
}

static void tap(bk_buttons_t b)
{
    bk_console_step(&con, b);
    bk_console_step(&con, 0);
}

static void to_home(void)
{
    idle(BK_SPLASH_TICKS);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
}

/* Move the settings cursor down to `item` (the list wraps, so this always arrives). */
static void goto_setting(bk_setting_item_t item)
{
    for (int guard = 0; con.cursor != item && guard < BK_SET_COUNT; guard++) {
        tap(BK_BTN_DOWN);
    }
    CHECK_EQ(con.cursor, item);
}

static bool has_event(bk_event_type_t t)
{
    for (unsigned i = 0; i < con.events.count; i++) {
        if (con.events.items[i].type == t) {
            return true;
        }
    }
    return false;
}

static void test_init_validation(void)
{
    fake_hal_init(&hal);
    CHECK(!bk_console_init(&con, NULL, 1, &hal.hal, 8));
    CHECK(!bk_console_init(&con, CAT_STUBS, 0, &hal.hal, 8));
    CHECK(!bk_console_init(&con, CAT_STUBS, 2, NULL, 8));
    CHECK(!bk_console_init(&con, CAT_STUBS, 2, &hal.hal, 7));
    static const bk_catalog_entry_t no_glyph[] = {{&stub_def_multi, NULL}};
    CHECK(!bk_console_init(&con, no_glyph, 1, &hal.hal, 8));
    bk_game_def_t huge = stub_def_multi;
    huge.state_size = BK_GAME_STATE_MAX + 1; /* a game whose state does not fit the static reservation is refused up front */
    const bk_catalog_entry_t big[] = {{&huge, "1"}};
    CHECK(!bk_console_init(&con, big, 1, &hal.hal, 8));
    bk_clip_t bad_clip = stub_clip;
    bad_clip.poster = bad_clip.frame_count; /* a poster past the end of the clip */
    bk_game_def_t broken = stub_def_multi;
    broken.preview = &bad_clip;
    const bk_catalog_entry_t bad[] = {{&broken, "1"}};
    CHECK(!bk_console_init(&con, bad, 1, &hal.hal, 8)); /* a broken clip would put garbage in the menu */
    CHECK(bk_console_init(&con, CAT_STUBS, 2, &hal.hal, 8));
    CHECK(bk_console_init(&con, CAT_TETRIS, 1, &hal.hal, 8)); /* the real catalog fits */
    CHECK(sizeof(bkt_game_t) <= BK_GAME_STATE_MAX);
}

static void test_splash(void)
{
    fresh(CAT_STUBS, 2);
    CHECK_EQ(con.screen, BK_SCREEN_SPLASH);
    idle(BK_SPLASH_TICKS - 1);
    CHECK_EQ(con.screen, BK_SCREEN_SPLASH);
    idle(1);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);

    fresh(CAT_STUBS, 2);
    bk_console_step(&con, BK_BTN_SELECT); /* any button skips the splash and is not otherwise used */
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    CHECK_EQ(con.focus, BK_MENU_SPEED);
}

static void test_home_navigation(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    CHECK_EQ(con.game_index, 0);
    tap(BK_BTN_UP); /* default focus is SPEED */
    CHECK_EQ(con.speed, 1);
    tap(BK_BTN_DOWN);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.speed, 9); /* wraps 0 -> 9 */
    tap(BK_BTN_UP);
    CHECK_EQ(con.speed, 0);

    tap(BK_BTN_SELECT); /* SPEED -> LEVEL */
    CHECK_EQ(con.focus, BK_MENU_LEVEL);
    tap(BK_BTN_UP);
    tap(BK_BTN_UP);
    CHECK_EQ(con.level, 2);
    tap(BK_BTN_SELECT); /* LEVEL -> MODE (this game has 3 modes) */
    CHECK_EQ(con.focus, BK_MENU_MODE);
    tap(BK_BTN_UP);
    CHECK_EQ(con.mode, 1);
    tap(BK_BTN_DOWN);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.mode, 2); /* wraps 0 -> 2 */
    tap(BK_BTN_SELECT);
    CHECK_EQ(con.focus, BK_MENU_SPEED);

    /* the next game: the mode restarts from the first one, SPEED and LEVEL stay */
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.game_index, 1);
    CHECK_EQ(con.mode, 0);
    CHECK_EQ(con.level, 2);
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.game_index, 0); /* wraps */
    tap(BK_BTN_LEFT);
    CHECK_EQ(con.game_index, 1);

    /* a single-mode game has no MODE step in the focus cycle; UP on MODE would be silent */
    CHECK_EQ(con.focus, BK_MENU_SPEED);
    tap(BK_BTN_SELECT);
    CHECK_EQ(con.focus, BK_MENU_LEVEL);
    tap(BK_BTN_SELECT);
    CHECK_EQ(con.focus, BK_MENU_SPEED);
}

static void test_single_game_catalog_is_silent(void)
{
    fresh(CAT_TETRIS, 1);
    to_home();
    bk_console_step(&con, BK_BTN_RIGHT); /* nothing to switch to: no change and no beep */
    CHECK_EQ(con.game_index, 0);
    CHECK_EQ(con.events.count, 0);
    tap(BK_BTN_LEFT);
    CHECK_EQ(con.game_index, 0);
    CHECK_EQ(con.events.count, 0);
    tap(BK_BTN_SELECT); /* Tetris has one mode: the focus cycles between SPEED and LEVEL only */
    CHECK_EQ(con.focus, BK_MENU_LEVEL);
    tap(BK_BTN_SELECT);
    CHECK_EQ(con.focus, BK_MENU_SPEED);
}

static void test_mode_focus_dropped_when_game_has_one_mode(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_SELECT);
    tap(BK_BTN_SELECT);
    CHECK_EQ(con.focus, BK_MENU_MODE);
    tap(BK_BTN_RIGHT); /* the new game has one mode: focus must not rest on MODE */
    CHECK_EQ(con.game_index, 1);
    CHECK(con.focus != BK_MENU_MODE);
}

static void test_menu_autorepeat(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    for (unsigned i = 0; i < 25; i++) {
        bk_console_step(&con, BK_BTN_UP); /* edge + 24 held ticks */
    }
    CHECK_EQ(con.speed, 2);
    for (unsigned i = 0; i < 6; i++) {
        bk_console_step(&con, BK_BTN_UP);
    }
    CHECK_EQ(con.speed, 3); /* then every 6 ticks */
    bk_console_step(&con, 0);
    for (unsigned i = 0; i < 10; i++) {
        bk_console_step(&con, BK_BTN_UP); /* a new press starts over */
    }
    CHECK_EQ(con.speed, 4);
}

static void test_start_game_uses_menu_choices(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    for (int i = 0; i < 4; i++) {
        tap(BK_BTN_UP); /* speed 4 */
    }
    tap(BK_BTN_SELECT);
    for (int i = 0; i < 2; i++) {
        tap(BK_BTN_UP); /* level 2 */
    }
    tap(BK_BTN_SELECT);
    tap(BK_BTN_UP); /* mode 1 */
    hal.writes = 0;
    fake_hal_clear_tones(&hal);
    bk_console_step(&con, BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    CHECK(con.game != NULL);
    CHECK_EQ(con.game->speed, 5); /* the game gets the displayed value + 1 */
    CHECK_EQ(con.game->level, 3);
    CHECK_EQ(con.game->mode, 1);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    CHECK(has_event(BK_EV_STARTED));
    CHECK_EQ(((stub_game_t *)con.game)->created_seed, hal.entropy_value + 0x9e3779b1u);
    CHECK_EQ(hal.tones[0], 660); /* confirm cue */
    CHECK_EQ(hal.writes, 1);     /* the menu position is remembered in the profile */
    CHECK_EQ(con.profile.settings.last_speed, 4);
    CHECK_EQ(con.profile.settings.last_level, 2);
    CHECK_EQ(con.profile.settings.last_mode, 1);
}

static void test_fn1_also_starts(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_START);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
}

static void test_controls_mapping_in_game(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    stub_game_t *s = (stub_game_t *)con.game;
    bk_console_step(&con, BK_BTN_LEFT | BK_BTN_SELECT); /* the function key in the same tick is not part of the game input */
    CHECK_EQ(s->last_held, BK_KEY_LEFT);
    bk_console_step(&con, 0);
    bk_console_step(&con, BK_BTN_ACTION); /* the one big button is ROTATE for the game */
    CHECK_EQ(s->rotate_presses, 1);
    CHECK_EQ(s->last_held, BK_KEY_ROTATE);
    bk_console_step(&con, 0);
    bk_console_step(&con, BK_BTN_UP | BK_BTN_DOWN | BK_BTN_RIGHT);
    CHECK_EQ(s->last_held, BK_KEY_UP | BK_KEY_DOWN | BK_KEY_RIGHT);
    CHECK_EQ(bk_buttons_to_keys(BK_BTN_START | BK_BTN_SELECT | BK_BTN_MODE), 0); /* function keys never reach a game */
}

static void test_pause_resume_and_two_step_exit(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    stub_game_t *s = (stub_game_t *)con.game;
    idle(5);
    uint32_t steps = s->steps;

    bk_console_step(&con, BK_BTN_START);
    CHECK_EQ(con.game->phase, BK_PHASE_PAUSED);
    CHECK(has_event(BK_EV_PAUSED));
    idle(20);
    CHECK_EQ(s->steps, steps); /* frozen */

    /* ACTION resumes, and that press is not also delivered to the game as a rotation */
    bk_console_step(&con, BK_BTN_ACTION);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    CHECK_EQ(s->rotate_presses, 0);
    CHECK(has_event(BK_EV_RESUMED));
    bk_console_step(&con, 0);

    /* MODE while playing only pauses: a stray press cannot lose the game */
    bk_console_step(&con, BK_BTN_MODE);
    CHECK_EQ(con.game->phase, BK_PHASE_PAUSED);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    bk_console_step(&con, 0);
    bk_console_step(&con, BK_BTN_MODE); /* the second press leaves */
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    CHECK(con.game == NULL);
    CHECK(has_event(BK_EV_UI_BACK));
}

static void test_game_over_record_and_replay(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    for (int i = 0; i < 3; i++) {
        tap(BK_BTN_ACTION); /* 3 x 10 points = goal 30: game over */
    }
    unsigned guard = 0;
    while (con.game->phase != BK_PHASE_OVER && guard++ < 1000) {
        idle(1);
    }
    CHECK_EQ(con.game->phase, BK_PHASE_OVER);
    CHECK_EQ(bk_profile_get_record(&con.profile, 100, 0), 30);

    /* ACTION plays again: a fresh party, the record stays */
    uint32_t parties = con.parties;
    bk_console_step(&con, BK_BTN_ACTION);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    CHECK_EQ(con.game->score, 0);
    CHECK_EQ(con.game->hi_score, 30);
    CHECK_EQ(con.parties, parties + 1);

    /* leaving mid-party keeps a better score even without a game over */
    bk_console_step(&con, 0);
    for (int i = 0; i < 2; i++) {
        tap(BK_BTN_ACTION);
    }
    CHECK_EQ(con.game->score, 20); /* below the record: nothing to save */
    tap(BK_BTN_MODE);
    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    CHECK_EQ(bk_profile_get_record(&con.profile, 100, 0), 30);

    /* a reboot restores the record and the menu position */
    bk_console_t again;
    CHECK(bk_console_init(&again, CAT_STUBS, 2, &hal.hal, 8));
    CHECK_EQ(bk_profile_get_record(&again.profile, 100, 0), 30);
    CHECK_EQ(again.game_index, 0);
}

static void test_records_are_per_game_and_mode(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_SELECT);
    tap(BK_BTN_SELECT);
    tap(BK_BTN_UP); /* mode 1: the goal is 31 */
    tap(BK_BTN_ACTION);
    for (int i = 0; i < 4; i++) {
        tap(BK_BTN_ACTION);
    }
    unsigned guard = 0;
    while (con.game->phase != BK_PHASE_OVER && guard++ < 1000) {
        idle(1);
    }
    CHECK_EQ(bk_profile_get_record(&con.profile, 100, 1), 40);
    CHECK_EQ(bk_profile_get_record(&con.profile, 100, 0), 0);
    CHECK_EQ(bk_profile_get_record(&con.profile, 101, 0), 0);
}

static void test_settings_sound_and_light(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
    CHECK_EQ(con.cursor, BK_SET_SOUND);

    fake_hal_clear_tones(&hal);
    tap(BK_BTN_ACTION); /* SOUND off */
    CHECK_EQ(con.profile.settings.sound_on, 0);
    CHECK(!con.audio.enabled);
    CHECK_EQ(hal.tone_now, 0);
    fake_hal_clear_tones(&hal);
    tap(BK_BTN_DOWN);
    CHECK_EQ(hal.tone_count, 0); /* muted: even the menu tick is silent */

    CHECK_EQ(con.cursor, BK_SET_LIGHT);
    unsigned calls = hal.light_calls;
    tap(BK_BTN_RIGHT);
    CHECK_EQ(hal.light, 1);
    tap(BK_BTN_RIGHT);
    CHECK_EQ(hal.light, 2);
    tap(BK_BTN_RIGHT);
    CHECK_EQ(hal.light, 0);
    CHECK_EQ(hal.light_calls, calls + 3);
    tap(BK_BTN_LEFT);
    CHECK_EQ(hal.light, 2); /* LEFT goes backwards */
    CHECK_EQ(con.profile.settings.light_level, 2);

    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);

    /* settings survive a reboot and are applied at boot */
    fake_hal_t keep = hal;
    keep.hal.input.ctx = keep.hal.display.ctx = keep.hal.audio.ctx = keep.hal.storage.ctx = keep.hal.system.ctx = &keep;
    bk_console_t again;
    CHECK(bk_console_init(&again, CAT_STUBS, 2, &keep.hal, 8));
    CHECK_EQ(again.profile.settings.sound_on, 0);
    CHECK(!again.audio.enabled);
    CHECK_EQ(keep.light, 2);
}

static void test_settings_clear_records_needs_confirmation(void)
{
    fresh(CAT_STUBS, 2);
    bk_profile_defaults(&con.profile);
    bk_profile_t p;
    bk_profile_defaults(&p);
    bk_profile_set_record(&p, 100, 0, 77);
    bk_save_store(&hal.hal.storage, &p);
    boot(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_MODE);
    goto_setting(BK_SET_RECORDS);
    tap(BK_BTN_ACTION);
    CHECK(con.confirm_clear);
    CHECK_EQ(bk_profile_get_record(&con.profile, 100, 0), 77); /* one press changes nothing */
    tap(BK_BTN_DOWN);                                          /* moving away cancels */
    CHECK(!con.confirm_clear);
    tap(BK_BTN_UP);
    tap(BK_BTN_ACTION);
    tap(BK_BTN_ACTION);
    CHECK_EQ(bk_profile_get_record(&con.profile, 100, 0), 0);
    bk_console_t again;
    CHECK(bk_console_init(&again, CAT_STUBS, 2, &hal.hal, 8));
    CHECK_EQ(bk_profile_get_record(&again.profile, 100, 0), 0);
}

static void test_button_test_and_about(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_MODE);
    goto_setting(BK_SET_BUTTONS);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_BUTTON_TEST);
    bk_console_step(&con, BK_BTN_LEFT | BK_BTN_ACTION | BK_BTN_MODE); /* pressing keys only lights them up */
    CHECK_EQ(con.screen, BK_SCREEN_BUTTON_TEST);
    bk_console_step(&con, BK_BTN_START | BK_BTN_SELECT | BK_BTN_MODE); /* the three function keys together leave */
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
    bk_console_step(&con, 0);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.cursor, BK_SET_ABOUT);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_ABOUT);
    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
}

static void test_button_test_lights_each_button_on_its_own_row(void)
{
    static const char *const names[8] = {"UP", "DOWN", "LEFT", "RIGHT", "ACTION", "MODE", "SELECT", "START"};
    static const bk_button_t bits[8] = {BK_BTN_UP,     BK_BTN_DOWN, BK_BTN_LEFT,   BK_BTN_RIGHT,
                                        BK_BTN_ACTION, BK_BTN_MODE, BK_BTN_SELECT, BK_BTN_START};
    static bk_frame_t expected;
    for (int pressed = 0; pressed < 8; pressed++) {
        fresh(CAT_STUBS, 2);
        to_home();
        tap(BK_BTN_MODE);
        goto_setting(BK_SET_BUTTONS);
        tap(BK_BTN_ACTION);
        CHECK_EQ(con.screen, BK_SCREEN_BUTTON_TEST);
        bk_console_step(&con, bits[pressed]);
        const bk_frame_t *shown = bk_console_render(&con);

        bk_text_row_t rows[8];
        for (int i = 0; i < 8; i++) {
            rows[i].label = names[i];
            rows[i].value = i == pressed ? "ON" : "--";
        }
        bk_page_t page;
        memset(&page, 0, sizeof page);
        page.title = "BUTTONS";
        page.style = BK_PAGE_LINES;
        page.rows = rows;
        page.count = 8;
        page.cursor = -1;
        page.sound_on = true;
        page.footer[0] = "EXIT";
        page.footer[1] = "MODE+SELECT+START";
        page.status[0].label = "LIGHT";
        page.status[0].value = "OFF";
        page.status[1].label = "BLE";
        page.status[1].value = "OFF";
        bk_render_page(&con.layout, &page, &expected);
        CHECK(bk_frame_equal(shown, &expected)); /* exactly this button's row is lit, in the order printed on the housing */
    }
}

static void test_front_light_in_game(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    CHECK_EQ(hal.light, 0);
    tap(BK_BTN_SELECT);
    CHECK_EQ(hal.light, 1);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING); /* the light key never pauses or otherwise disturbs the game */
    tap(BK_BTN_SELECT);
    tap(BK_BTN_SELECT);
    CHECK_EQ(hal.light, 0);
}

static void test_present_only_on_change(void)
{
    fresh(CAT_STUBS, 2);
    hal.buttons = 0;
    bk_console_tick(&con);
    CHECK_EQ(hal.presents, 1);
    for (int i = 0; i < 5; i++) {
        bk_console_tick(&con);
    }
    CHECK_EQ(hal.presents, 1); /* splash is static: no redundant refresh of the panel */

    hal.buttons = BK_BTN_MODE; /* to the settings page */
    bk_console_tick(&con);
    hal.buttons = 0;
    for (int i = 0; i < 2; i++) {
        bk_console_tick(&con);
    }
    hal.buttons = BK_BTN_MODE;
    bk_console_tick(&con);
    hal.buttons = 0;
    bk_console_tick(&con);
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
    unsigned before = hal.presents;
    for (int i = 0; i < 100; i++) {
        bk_console_tick(&con);
    }
    CHECK_EQ(hal.presents, before); /* a static page costs nothing */

    /* the menu blinks the focused value: a refresh only when the blink phase flips */
    hal.buttons = BK_BTN_MODE;
    bk_console_tick(&con);
    hal.buttons = 0;
    bk_console_tick(&con);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    hal.buttons = BK_BTN_RIGHT; /* the second stub has no menu clip: only the blink of the focused value is left */
    bk_console_tick(&con);
    hal.buttons = 0;
    bk_console_tick(&con);
    CHECK_EQ(con.game_index, 1);
    before = hal.presents;
    for (unsigned i = 0; i < 4 * BK_BLINK_HALF_TICKS; i++) {
        bk_console_tick(&con);
    }
    unsigned flips = hal.presents - before;
    CHECK(flips >= 3 && flips <= 5);
}

/* The display is never assumed to run at 60 Hz (AGENTS.md, LS032B7DD02 quotes 1-10 Hz): pictures that change faster
 * than the panel allows are dropped, the latest one always goes out, and the game keeps its own 60 Hz tick. */
static void test_display_rate_limit(void)
{
    static const unsigned fps_list[] = {0, 60, 30, 20, 15, 10};
    for (unsigned k = 0; k < sizeof fps_list / sizeof fps_list[0]; k++) {
        unsigned fps = fps_list[k];
        unsigned interval = (fps == 0 || fps >= BK_TICK_HZ) ? 1 : (BK_TICK_HZ + fps - 1) / fps;
        fake_hal_init(&hal);
        hal.hal.display.max_fps = (uint8_t)fps;
        boot(CAT_STUBS, 2);

        hal.buttons = BK_BTN_START; /* skip the splash, then start the game: the stub changes its picture on every tick */
        bk_console_tick(&con);
        hal.buttons = 0;
        bk_console_tick(&con);
        hal.buttons = BK_BTN_START;
        bk_console_tick(&con);
        hal.buttons = 0;
        CHECK_EQ(con.screen, BK_SCREEN_GAME);

        unsigned last_present_tick = con.presented_tick;
        unsigned seen = 0;
        for (unsigned i = 0; i < 120; i++) {
            unsigned before = hal.presents;
            bk_console_tick(&con);
            if (hal.presents != before) {
                CHECK(con.ticks - last_present_tick >= interval); /* never faster than the panel allows */
                last_present_tick = con.ticks;
                seen++;
            }
        }
        CHECK(seen + 1 >= 120 / interval && seen <= 120 / interval + 1); /* and not slower either */
        CHECK_EQ(con.game->tick >= 100, 1);                              /* the simulation was not slowed down */
        if (interval > 1) {
            CHECK(con.dropped_frames > 0);
        }

        /* once the picture stops changing, the latest one reaches the panel within one interval */
        hal.buttons = BK_BTN_START; /* pause: the picture is now static */
        bk_console_tick(&con);
        hal.buttons = 0;
        for (unsigned i = 0; i < interval + 1; i++) {
            bk_console_tick(&con);
        }
        CHECK(bk_frame_equal(&hal.last_frame, bk_console_render(&con)));
    }
}

static void test_storage_failure_does_not_stop_the_console(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_UP); /* a changed menu position is remembered when the game starts */
    hal.fail_writes = 1;
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.save_errors, 1); /* the failed write is counted, the game starts anyway */
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    tap(BK_BTN_MODE);
    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
}

static void test_tetris_in_the_console(void)
{
    fresh(CAT_TETRIS, 1);
    to_home();
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    bkt_game_t *t = (bkt_game_t *)con.game;
    CHECK(t->has_piece);
    int x = t->piece_x;
    tap(BK_BTN_LEFT);
    CHECK_EQ(t->piece_x, x - 1);
    int angle = t->piece.angle;
    uint8_t shape = t->piece.shape;
    tap(BK_BTN_ACTION); /* the big button rotates */
    CHECK(shape == BKT_SHAPE_SQUARE || t->piece.angle != angle);
    int y = t->piece_y;
    for (int i = 0; i < 3; i++) {
        bk_console_step(&con, BK_BTN_DOWN);
    }
    CHECK(t->piece_y < y);

    /* play on with the soft drop until the stack tops out: scores and ends like the standalone game */
    unsigned guard = 0;
    while (con.game->phase != BK_PHASE_OVER && guard++ < 20000) {
        bk_console_step(&con, (guard % 40) < 20 ? BK_BTN_DOWN : BK_BTN_LEFT);
    }
    CHECK_EQ(con.game->phase, BK_PHASE_OVER);
    bk_console_render(&con);
}

/* ---------------------------------------------------------------- menu clip, DEMO, new settings, AUTO OFF (ADR-041) */

static bool region_equal(const bk_frame_t *a, const bk_frame_t *b, int x0, int y0, int x1, int y1)
{
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (bk_frame_get(a, x, y) != bk_frame_get(b, x, y)) {
                return false;
            }
        }
    }
    return true;
}

static unsigned ink_in(const bk_frame_t *f, int x0, int y0, int x1, int y1)
{
    unsigned n = 0;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            n += bk_frame_get(f, x, y) ? 1u : 0u;
        }
    }
    return n;
}

static void test_home_plays_the_clip_of_the_selected_game(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    const int cell = con.layout.cell;
    const int clip_y = BK_CLIP_FIRST_ROW * cell;
    /* the stub's dot walks along the bottom row: frame k lights the cell of column k and no other */
    for (int round = 0; round < 2; round++) { /* twice: the loop wraps */
        for (int k = 0; k < 4; k++) {
            con.preview_ticks = (uint32_t)(round * 4 + k) * STUB_CLIP_TICKS + 3;
            const bk_frame_t *f = bk_console_render(&con);
            for (int col = 0; col < BK_COLS; col++) {
                CHECK_EQ(bk_frame_get(f, col * cell, (BK_ROWS - 1) * cell), col == k);
            }
            CHECK_EQ(ink_in(f, 0, clip_y, con.layout.field_w, (BK_ROWS - 1) * cell), 0); /* only the bottom row is lit */
        }
    }

    /* switching games restarts the clip of the new one; a game without a clip leaves the area empty */
    con.preview_ticks = 3 * STUB_CLIP_TICKS;
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.game_index, 1);
    CHECK(con.preview_ticks < 3);
    CHECK_EQ(ink_in(bk_console_render(&con), 0, clip_y, con.layout.field_w, con.layout.height), 0);
    tap(BK_BTN_LEFT);
    CHECK_EQ(con.game_index, 0);
    CHECK(con.preview_ticks < 3);
    idle(STUB_CLIP_TICKS + 5);
    CHECK(bk_frame_get(bk_console_render(&con), 1 * cell, (BK_ROWS - 1) * cell)); /* frame 1 after one frame time */

    /* the number and the sign keep their rows (mock-up): sign rows 1..7, mode number rows 9..13 */
    const bk_frame_t *f = bk_console_render(&con);
    CHECK_EQ(ink_in(f, 0, 0, con.layout.field_w, cell), 0);
    CHECK(ink_in(f, 0, cell, con.layout.field_w, 8 * cell) > 0);
    CHECK_EQ(ink_in(f, 0, 8 * cell, con.layout.field_w, 9 * cell), 0);
    CHECK(ink_in(f, 0, 9 * cell, con.layout.field_w, 14 * cell) > 0);
    CHECK_EQ(ink_in(f, 0, 14 * cell, con.layout.field_w, 15 * cell), 0);
}

static void test_demo_off_shows_the_poster_and_stops_refreshing(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_MODE);
    goto_setting(BK_SET_DEMO);
    tap(BK_BTN_ACTION); /* DEMO off */
    CHECK_EQ(con.profile.settings.demo_on, 0);
    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    const int cell = con.layout.cell;
    for (int k = 0; k < 6; k++) {
        con.preview_ticks = (uint32_t)k * STUB_CLIP_TICKS;
        const bk_frame_t *f = bk_console_render(&con);
        for (int col = 0; col < BK_COLS; col++) {
            CHECK_EQ(bk_frame_get(f, col * cell, (BK_ROWS - 1) * cell), col == stub_clip.poster); /* always the poster */
        }
    }

    /* the panel is refreshed only for what still changes (the blinking value), with the animation on it is more often */
    unsigned off_presents;
    unsigned on_presents;
    for (int demo = 0; demo < 2; demo++) {
        fake_hal_init(&hal);
        bk_profile_t p;
        bk_profile_defaults(&p);
        p.settings.demo_on = (uint8_t)demo;
        bk_save_store(&hal.hal.storage, &p);
        boot(CAT_STUBS, 2);
        hal.buttons = 0;
        for (unsigned i = 0; i < BK_SPLASH_TICKS + 4 * STUB_CLIP_TICKS * 3; i++) {
            bk_console_tick(&con);
        }
        CHECK_EQ(con.screen, BK_SCREEN_HOME);
        if (demo) {
            on_presents = hal.presents;
        } else {
            off_presents = hal.presents;
        }
    }
    CHECK(on_presents >= off_presents + 6);
}

static void test_settings_ble_demo_and_auto_off_values(void)
{
    fresh(CAT_STUBS, 2);
    CHECK_EQ(hal.ble_calls, 1); /* the radio is told its state at start: off */
    CHECK(!hal.ble);
    to_home();
    tap(BK_BTN_MODE);

    goto_setting(BK_SET_BLE);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.profile.settings.ble_on, 1);
    CHECK(hal.ble);
    CHECK_EQ(hal.ble_calls, 2);
    tap(BK_BTN_LEFT); /* either direction toggles */
    CHECK_EQ(con.profile.settings.ble_on, 0);
    CHECK(!hal.ble);
    tap(BK_BTN_RIGHT);
    CHECK(hal.ble);

    goto_setting(BK_SET_DEMO);
    CHECK_EQ(con.profile.settings.demo_on, 1);
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.profile.settings.demo_on, 0);
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.profile.settings.demo_on, 1);

    goto_setting(BK_SET_SLEEP);
    CHECK_EQ(con.profile.settings.sleep_idx, BK_SLEEP_DEFAULT);
    CHECK_EQ(bk_sleep_timeout_ticks(con.profile.settings.sleep_idx), 5u * 60u * BK_TICK_HZ);
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.profile.settings.sleep_idx, 4); /* 10 minutes */
    tap(BK_BTN_RIGHT);
    CHECK_EQ(con.profile.settings.sleep_idx, 0); /* wraps to OFF */
    CHECK_EQ(bk_sleep_timeout_ticks(0), 0);
    tap(BK_BTN_LEFT);
    CHECK_EQ(con.profile.settings.sleep_idx, 4);
    CHECK_EQ(bk_sleep_timeout_ticks(1), 1u * 60u * BK_TICK_HZ);
    CHECK_EQ(bk_sleep_timeout_ticks(2), 3u * 60u * BK_TICK_HZ);
    CHECK_EQ(bk_sleep_timeout_ticks(BK_SLEEP_OPTIONS), 0); /* out of range: never */

    /* all of it survives a reboot, and BLE is applied at start */
    fake_hal_t keep = hal;
    keep.hal.input.ctx = keep.hal.display.ctx = keep.hal.audio.ctx = keep.hal.storage.ctx = keep.hal.system.ctx = &keep;
    keep.ble = false;
    bk_console_t again;
    CHECK(bk_console_init(&again, CAT_STUBS, 2, &keep.hal, 8));
    CHECK_EQ(again.profile.settings.ble_on, 1);
    CHECK_EQ(again.profile.settings.demo_on, 1);
    CHECK_EQ(again.profile.settings.sleep_idx, 4);
    CHECK(keep.ble);
}

static void test_the_settings_list_scrolls_to_keep_the_cursor_in_view(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_MODE);
    CHECK_EQ(con.cursor, 0);
    CHECK_EQ(con.top, 0);
    const size_t visible = bk_page_rows_visible(&con.layout);
    CHECK_EQ(visible, 5);
    for (int i = 1; i < BK_SET_COUNT; i++) {
        tap(BK_BTN_DOWN);
        CHECK_EQ(con.cursor, i);
        CHECK(con.cursor >= con.top && con.cursor < con.top + visible); /* never out of view */
        CHECK_EQ(con.top, i < (int)visible ? 0 : i + 1 - (int)visible); /* scrolls only when it has to */
    }
    CHECK_EQ(con.top, BK_SET_COUNT - visible);
    tap(BK_BTN_DOWN); /* wraps to the first row: the list goes back to the top */
    CHECK_EQ(con.cursor, 0);
    CHECK_EQ(con.top, 0);
    tap(BK_BTN_UP); /* and to the last one the other way */
    CHECK_EQ(con.cursor, BK_SET_COUNT - 1);
    CHECK_EQ(con.top, BK_SET_COUNT - visible);
    tap(BK_BTN_UP);
    tap(BK_BTN_UP);
    CHECK_EQ(con.top, BK_SET_COUNT - visible); /* the cursor moves inside the window, the window stays */
    for (int i = 0; i < 3; i++) {
        tap(BK_BTN_UP);
    }
    CHECK(con.top <= con.cursor);
    tap(BK_BTN_MODE);
    tap(BK_BTN_MODE); /* the list opens at the top every time */
    CHECK_EQ(con.cursor, 0);
    CHECK_EQ(con.top, 0);
}

static void test_auto_off_blanks_the_display_and_any_key_wakes_it(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_MODE);
    goto_setting(BK_SET_LIGHT);
    tap(BK_BTN_RIGHT);
    tap(BK_BTN_RIGHT); /* light HIGH */
    CHECK_EQ(hal.light, 2);
    goto_setting(BK_SET_SLEEP);
    tap(BK_BTN_LEFT);
    tap(BK_BTN_LEFT);
    CHECK_EQ(con.profile.settings.sleep_idx, 1); /* one minute */
    tap(BK_BTN_MODE);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);

    const uint32_t timeout = bk_sleep_timeout_ticks(1);
    idle(timeout - con.idle_ticks - 1);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    CHECK_EQ(con.idle_ticks, timeout - 1);
    idle(1);
    CHECK_EQ(con.screen, BK_SCREEN_SLEEP);
    CHECK_EQ(hal.light, 0);                        /* the front light goes out ... */
    CHECK_EQ(con.profile.settings.light_level, 2); /* ... the setting stays */
    const bk_frame_t *blank = bk_console_render(&con);
    CHECK_EQ(ink_in(blank, 0, 0, blank->width, blank->height), 0); /* nothing is lit */

    /* any key wakes it and does nothing else: no game starts, the menu does not move */
    uint32_t parties = con.parties;
    const uint8_t index = con.game_index;
    idle(10);
    CHECK_EQ(con.screen, BK_SCREEN_SLEEP);
    bk_console_step(&con, BK_BTN_RIGHT);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    CHECK_EQ(con.game_index, index);
    bk_console_step(&con, 0);
    CHECK_EQ(hal.light, 2);
    CHECK_EQ(con.idle_ticks, 1);
    CHECK_EQ(con.parties, parties);

    /* a key held down is activity for as long as it is held, not only at the moment it was pressed */
    con.idle_ticks = 100;
    for (int i = 0; i < 10; i++) {
        bk_console_step(&con, BK_BTN_SELECT);
    }
    CHECK_EQ(con.idle_ticks, 0);
    bk_console_step(&con, 0);

    bk_console_step(&con, BK_BTN_START);
    bk_console_step(&con, 0);
    CHECK_EQ(con.screen, BK_SCREEN_GAME); /* and the next key works as usual */
}

static void test_auto_off_never_in_a_running_game_but_in_a_paused_one(void)
{
    fresh(CAT_STUBS, 2);
    con.profile.settings.sleep_idx = 1;
    to_home();
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);
    con.idle_ticks = 1u << 20; /* as if the player had done nothing for a very long time */
    idle(1);
    CHECK_EQ(con.screen, BK_SCREEN_GAME); /* a game in play is never idle */
    CHECK_EQ(con.idle_ticks, 0);

    tap(BK_BTN_START); /* paused */
    CHECK_EQ(con.game->phase, BK_PHASE_PAUSED);
    stub_game_t *s = (stub_game_t *)con.game;
    uint32_t steps = s->steps;
    idle(bk_sleep_timeout_ticks(1) + 5);
    CHECK_EQ(con.screen, BK_SCREEN_SLEEP);
    CHECK(con.game != NULL);
    bk_console_step(&con, BK_BTN_ACTION); /* the waking key does not resume the game either */
    bk_console_step(&con, 0);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    CHECK_EQ(con.game->phase, BK_PHASE_PAUSED);
    CHECK_EQ(s->steps, steps);
    CHECK_EQ(s->rotate_presses, 0);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING);

    /* OFF never sleeps */
    con.profile.settings.sleep_idx = 0;
    tap(BK_BTN_START);
    con.idle_ticks = 1u << 20;
    idle(3);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);

    /* a finished game is idle again */
    con.profile.settings.sleep_idx = 1;
    con.idle_ticks = 0;
    unsigned guard = 0;
    tap(BK_BTN_START);
    for (int i = 0; i < 3; i++) {
        tap(BK_BTN_ACTION);
    }
    while (con.game->phase != BK_PHASE_OVER && guard++ < 1000) {
        idle(1);
    }
    CHECK_EQ(con.game->phase, BK_PHASE_OVER);
    idle(bk_sleep_timeout_ticks(1) + 5);
    CHECK_EQ(con.screen, BK_SCREEN_SLEEP);
}

static void test_a_sleeping_panel_is_left_alone(void)
{
    fresh(CAT_STUBS, 2);
    con.profile.settings.sleep_idx = 1;
    hal.buttons = 0;
    unsigned guard = 0;
    while (con.screen != BK_SCREEN_SLEEP && guard++ < 2u * bk_sleep_timeout_ticks(1)) {
        bk_console_tick(&con);
    }
    CHECK_EQ(con.screen, BK_SCREEN_SLEEP);
    bk_console_tick(&con);
    unsigned before = hal.presents;
    for (int i = 0; i < 600; i++) {
        bk_console_tick(&con);
    }
    CHECK_EQ(hal.presents, before); /* one blank picture was sent, then nothing: the driver may stop the panel */
    CHECK_EQ(ink_in(&hal.last_frame, 0, 0, hal.last_frame.width, hal.last_frame.height), 0);
}

static void test_the_status_shows_the_values_the_game_will_get(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    bk_console_render(&con);
    CHECK_EQ(con.scene.speed, 1); /* the first speed and level of the toy: 01 and 01 */
    CHECK_EQ(con.scene.level, 1);
    tap(BK_BTN_UP);
    tap(BK_BTN_UP);
    tap(BK_BTN_SELECT);
    for (int i = 0; i < 9; i++) {
        tap(BK_BTN_UP);
    }
    bk_console_render(&con);
    CHECK_EQ(con.scene.speed, 3); /* shown from 1: the same number the game starts with */
    CHECK_EQ(con.scene.level, 10);
    tap(BK_BTN_START);
    CHECK_EQ(con.game->speed, 3);
    CHECK_EQ(con.game->level, 10);
    bk_console_render(&con);
    CHECK_EQ(con.scene.speed, 3);
    CHECK_EQ(con.scene.level, 10);
}

static void test_the_pause_sign_and_lives_label_in_the_game_panel(void)
{
    fresh(CAT_TETRIS, 1);
    to_home();
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    idle(3);
    const int cell = con.layout.cell;
    static bk_frame_t playing;
    const bk_frame_t *f = bk_console_render(&con);
    playing = *f;
    CHECK_EQ(ink_in(&playing, 2 * cell + 3, 8 * cell + 3, 8 * cell - 3, 9 * cell), 0); /* nothing of a pause yet */
    tap(BK_BTN_START);
    CHECK_EQ(con.game->phase, BK_PHASE_PAUSED);
    f = bk_console_render(&con);
    CHECK(!bk_frame_equal(f, &playing));
    CHECK(ink_in(f, 2 * cell, 8 * cell, 8 * cell, 12 * cell) > 0); /* the sign in its box over the field */
    CHECK(
        region_equal(f, &playing, con.layout.field_w, 0, con.layout.width, con.layout.height)); /* the panel is as before */
}

int main(void)
{
    TEST_RUN(test_init_validation);
    TEST_RUN(test_splash);
    TEST_RUN(test_home_navigation);
    TEST_RUN(test_single_game_catalog_is_silent);
    TEST_RUN(test_mode_focus_dropped_when_game_has_one_mode);
    TEST_RUN(test_menu_autorepeat);
    TEST_RUN(test_start_game_uses_menu_choices);
    TEST_RUN(test_fn1_also_starts);
    TEST_RUN(test_controls_mapping_in_game);
    TEST_RUN(test_pause_resume_and_two_step_exit);
    TEST_RUN(test_game_over_record_and_replay);
    TEST_RUN(test_records_are_per_game_and_mode);
    TEST_RUN(test_settings_sound_and_light);
    TEST_RUN(test_settings_clear_records_needs_confirmation);
    TEST_RUN(test_button_test_and_about);
    TEST_RUN(test_button_test_lights_each_button_on_its_own_row);
    TEST_RUN(test_front_light_in_game);
    TEST_RUN(test_present_only_on_change);
    TEST_RUN(test_display_rate_limit);
    TEST_RUN(test_storage_failure_does_not_stop_the_console);
    TEST_RUN(test_tetris_in_the_console);
    TEST_RUN(test_home_plays_the_clip_of_the_selected_game);
    TEST_RUN(test_demo_off_shows_the_poster_and_stops_refreshing);
    TEST_RUN(test_settings_ble_demo_and_auto_off_values);
    TEST_RUN(test_the_settings_list_scrolls_to_keep_the_cursor_in_view);
    TEST_RUN(test_auto_off_blanks_the_display_and_any_key_wakes_it);
    TEST_RUN(test_auto_off_never_in_a_running_game_but_in_a_paused_one);
    TEST_RUN(test_a_sleeping_panel_is_left_alone);
    TEST_RUN(test_the_status_shows_the_values_the_game_will_get);
    TEST_RUN(test_the_pause_sign_and_lives_label_in_the_game_panel);
    return bk_test_finish("console");
}
