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
    bk_console_step(&con, BK_BTN_FN2); /* any button skips the splash and is not otherwise used */
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

    tap(BK_BTN_FN2); /* SPEED -> LEVEL */
    CHECK_EQ(con.focus, BK_MENU_LEVEL);
    tap(BK_BTN_UP);
    tap(BK_BTN_UP);
    CHECK_EQ(con.level, 2);
    tap(BK_BTN_FN2); /* LEVEL -> MODE (this game has 3 modes) */
    CHECK_EQ(con.focus, BK_MENU_MODE);
    tap(BK_BTN_UP);
    CHECK_EQ(con.mode, 1);
    tap(BK_BTN_DOWN);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.mode, 2); /* wraps 0 -> 2 */
    tap(BK_BTN_FN2);
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
    tap(BK_BTN_FN2);
    CHECK_EQ(con.focus, BK_MENU_LEVEL);
    tap(BK_BTN_FN2);
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
    tap(BK_BTN_FN2); /* Tetris has one mode: the focus cycles between SPEED and LEVEL only */
    CHECK_EQ(con.focus, BK_MENU_LEVEL);
    tap(BK_BTN_FN2);
    CHECK_EQ(con.focus, BK_MENU_SPEED);
}

static void test_mode_focus_dropped_when_game_has_one_mode(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_FN2);
    tap(BK_BTN_FN2);
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
    tap(BK_BTN_FN2);
    for (int i = 0; i < 2; i++) {
        tap(BK_BTN_UP); /* level 2 */
    }
    tap(BK_BTN_FN2);
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
    tap(BK_BTN_FN1);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
}

static void test_controls_mapping_in_game(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    stub_game_t *s = (stub_game_t *)con.game;
    bk_console_step(&con, BK_BTN_LEFT | BK_BTN_FN2); /* the function key in the same tick is not part of the game input */
    CHECK_EQ(s->last_held, BK_KEY_LEFT);
    bk_console_step(&con, 0);
    bk_console_step(&con, BK_BTN_ACTION); /* the one big button is ROTATE for the game */
    CHECK_EQ(s->rotate_presses, 1);
    CHECK_EQ(s->last_held, BK_KEY_ROTATE);
    bk_console_step(&con, 0);
    bk_console_step(&con, BK_BTN_UP | BK_BTN_DOWN | BK_BTN_RIGHT);
    CHECK_EQ(s->last_held, BK_KEY_UP | BK_KEY_DOWN | BK_KEY_RIGHT);
    CHECK_EQ(bk_buttons_to_keys(BK_BTN_FN1 | BK_BTN_FN2 | BK_BTN_FN3), 0); /* function keys never reach a game */
}

static void test_pause_resume_and_two_step_exit(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    stub_game_t *s = (stub_game_t *)con.game;
    idle(5);
    uint32_t steps = s->steps;

    bk_console_step(&con, BK_BTN_FN1);
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

    /* FN3 while playing only pauses: a stray press cannot lose the game */
    bk_console_step(&con, BK_BTN_FN3);
    CHECK_EQ(con.game->phase, BK_PHASE_PAUSED);
    CHECK_EQ(con.screen, BK_SCREEN_GAME);
    bk_console_step(&con, 0);
    bk_console_step(&con, BK_BTN_FN3); /* the second press leaves */
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
    tap(BK_BTN_FN3);
    tap(BK_BTN_FN3);
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
    tap(BK_BTN_FN2);
    tap(BK_BTN_FN2);
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
    tap(BK_BTN_FN3);
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

    tap(BK_BTN_FN3);
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
    tap(BK_BTN_FN3);
    tap(BK_BTN_DOWN);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.cursor, BK_SET_RECORDS);
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
    tap(BK_BTN_FN3);
    for (int i = 0; i < 3; i++) {
        tap(BK_BTN_DOWN);
    }
    CHECK_EQ(con.cursor, BK_SET_BUTTONS);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_BUTTON_TEST);
    bk_console_step(&con, BK_BTN_LEFT | BK_BTN_ACTION | BK_BTN_FN3); /* pressing keys only lights them up */
    CHECK_EQ(con.screen, BK_SCREEN_BUTTON_TEST);
    bk_console_step(&con, BK_BTN_FN1 | BK_BTN_FN2 | BK_BTN_FN3); /* the three function keys together leave */
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
    bk_console_step(&con, 0);
    tap(BK_BTN_DOWN);
    CHECK_EQ(con.cursor, BK_SET_ABOUT);
    tap(BK_BTN_ACTION);
    CHECK_EQ(con.screen, BK_SCREEN_ABOUT);
    tap(BK_BTN_FN3);
    CHECK_EQ(con.screen, BK_SCREEN_SETTINGS);
}

static void test_front_light_in_game(void)
{
    fresh(CAT_STUBS, 2);
    to_home();
    tap(BK_BTN_ACTION);
    CHECK_EQ(hal.light, 0);
    tap(BK_BTN_FN2);
    CHECK_EQ(hal.light, 1);
    CHECK_EQ(con.game->phase, BK_PHASE_PLAYING); /* the light key never pauses or otherwise disturbs the game */
    tap(BK_BTN_FN2);
    tap(BK_BTN_FN2);
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

    hal.buttons = BK_BTN_FN3; /* to the settings page */
    bk_console_tick(&con);
    hal.buttons = 0;
    for (int i = 0; i < 2; i++) {
        bk_console_tick(&con);
    }
    hal.buttons = BK_BTN_FN3;
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
    hal.buttons = BK_BTN_FN3;
    bk_console_tick(&con);
    hal.buttons = 0;
    bk_console_tick(&con);
    CHECK_EQ(con.screen, BK_SCREEN_HOME);
    before = hal.presents;
    for (unsigned i = 0; i < 4 * BK_BLINK_HALF_TICKS; i++) {
        bk_console_tick(&con);
    }
    unsigned flips = hal.presents - before;
    CHECK(flips >= 3 && flips <= 5);
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
    tap(BK_BTN_FN3);
    tap(BK_BTN_FN3);
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
    TEST_RUN(test_front_light_in_game);
    TEST_RUN(test_present_only_on_change);
    TEST_RUN(test_storage_failure_does_not_stop_the_console);
    TEST_RUN(test_tetris_in_the_console);
    return bk_test_finish("console");
}
