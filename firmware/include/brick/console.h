/*
 * The "firmware application" of the console as a platform-neutral state machine: boot splash, game menu, settings,
 * the running game, records, sound and front light. It talks to the outside world only through bk_hal_t.
 *
 * Driving it (one call per 60 Hz tick):
 *     bk_console_tick(&console);         // read buttons -> step -> compose -> present if the picture changed
 * or, in tests / headless runs, bk_console_step() + bk_console_render() without touching the display port.
 *
 * ---- Controls (PROPOSED, ADR-025; the roles of the function keys are OPEN in the PRD) ----
 *
 *   key      | menu (HOME)                      | settings          | playing         | paused / game over
 *   ---------+----------------------------------+-------------------+-----------------+--------------------------
 *   LEFT/RIGHT| previous / next game            | change value      | game: move      | -
 *   UP/DOWN  | value of the focused item +/-1   | move cursor       | game: UP rotate | -
 *            |  (MODE, SPEED or LEVEL)          |                   |   DOWN soft drop|
 *   ACTION   | start the game                   | activate / change | game: rotate    | resume / play again
 *   FN1      | start the game                   | activate / change | pause           | resume / play again
 *   FN2      | focus MODE -> SPEED -> LEVEL     | -                 | front light     | front light
 *   FN3      | open SETTINGS                    | back              | pause           | back to the menu
 *
 * The console is the only place that knows about FN keys: games see just the D-pad and the big ACTION button
 * (as ROTATE). Leaving a running game takes two FN3 presses (first pauses, second leaves), so a stray press cannot
 * lose a game. There is no mapping for the engine's FIRE key (hard drop): the console has one action button.
 */
#ifndef BRICK_CONSOLE_H
#define BRICK_CONSOLE_H

#include "brick/audio.h"
#include "brick/frame.h"
#include "brick/game.h"
#include "brick/hal.h"
#include "brick/render.h"
#include "brick/save.h"

BK_EXTERN_C_BEGIN

#define BK_VERSION_STRING "0.1.0-PROTO"

#ifndef BK_GAME_STATE_MAX
#define BK_GAME_STATE_MAX 2048 /* bytes reserved for the running game's state; checked against state_size at init */
#endif
#define BK_MAX_GAMES            32
#define BK_SPLASH_TICKS         60
#define BK_MENU_REPEAT_DELAY    24 /* ticks before a held menu key repeats */
#define BK_MENU_REPEAT_INTERVAL 6
#define BK_MENU_VALUE_MAX       9 /* SPEED and LEVEL in the menu are 0..9; games get value + 1 */

typedef struct {
    const bk_game_def_t *def;
    const char *glyph; /* what the menu shows big: "1".."9", "10".., "A".. (1 or 2 characters) */
} bk_catalog_entry_t;

typedef enum {
    BK_SCREEN_SPLASH = 0,
    BK_SCREEN_HOME,
    BK_SCREEN_SETTINGS,
    BK_SCREEN_BUTTON_TEST,
    BK_SCREEN_ABOUT,
    BK_SCREEN_GAME
} bk_screen_t;

typedef enum { BK_MENU_MODE = 0, BK_MENU_SPEED = 1, BK_MENU_LEVEL = 2 } bk_menu_focus_t;

typedef enum {
    BK_SET_SOUND = 0,
    BK_SET_LIGHT,
    BK_SET_RECORDS,
    BK_SET_BUTTONS,
    BK_SET_ABOUT,
    BK_SET_COUNT
} bk_setting_item_t;

typedef struct {
    const bk_catalog_entry_t *catalog;
    size_t catalog_count;
    const bk_hal_t *hal;
    bk_layout_t layout;

    bk_profile_t profile;
    bk_audio_t audio;

    bk_screen_t screen;
    uint32_t ticks;        /* since init */
    uint32_t screen_ticks; /* since the current screen was entered */
    bk_buttons_t held;     /* buttons seen by the last step */
    bk_edge_t edge;
    uint32_t repeat[4]; /* menu auto-repeat: UP, DOWN, LEFT, RIGHT */

    /* HOME */
    uint8_t game_index;
    uint16_t mode;
    uint8_t speed; /* 0..9 */
    uint8_t level; /* 0..9 */
    uint8_t focus; /* bk_menu_focus_t */

    /* SETTINGS */
    uint8_t cursor;
    bool confirm_clear;

    /* GAME */
    union {
        double align_d;
        void *align_p;
        uint64_t align_u;
        uint8_t bytes[BK_GAME_STATE_MAX];
    } game_mem;
    bk_game_t *game; /* NULL outside of a game */
    bk_phase_t last_phase;
    uint32_t seed_base;
    uint32_t parties;

    uint32_t save_errors;    /* failed profile writes (diagnostics; the console keeps running) */
    bk_event_queue_t events; /* events produced by the last step (also what drove the audio) */
    bk_frame_t frame;
    bk_frame_t shown; /* what the display port has last been given */
    bool shown_valid;

    bk_scene_t scene; /* scratch for rendering, kept here so the core has no global state */
    char label[24];
} bk_console_t;

/* Returns false on bad arguments (empty catalog, odd cell size, a game whose state does not fit). */
bool bk_console_init(bk_console_t *c, const bk_catalog_entry_t *catalog, size_t count, const bk_hal_t *hal,
                     unsigned cell_dots);

void bk_console_step(bk_console_t *c, bk_buttons_t held);
const bk_frame_t *bk_console_render(bk_console_t *c);
void bk_console_tick(bk_console_t *c);

/* Mapping of physical buttons to game keys (FN keys never reach a game). */
uint8_t bk_buttons_to_keys(bk_buttons_t buttons);

BK_EXTERN_C_END

#endif /* BRICK_CONSOLE_H */
