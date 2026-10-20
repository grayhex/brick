/*
 * Platform-neutral game interface (docs/GAME_PORTING.md section 3, now concrete).
 *
 * A game is a plain-old-data struct whose first member is bk_game_t. All state lives inside it:
 * no heap, no globals. That gives us, for free:
 *   - static allocation on the MCU (bk_game_def_t.state_size / state_align),
 *   - deterministic replay (same seed + same inputs => same states),
 *   - snapshot/restore by copying the struct (same firmware build only, see save.h).
 *
 * bk_game_t reproduces the phase machine of the reference BrickBase (ready -> playing <-> paused -> over),
 * score/hi-score, speed/level, cutscenes (no-input animations) and the game-over fill animation.
 * A concrete game provides a bk_game_vtable_t.
 */
#ifndef BRICK_GAME_H
#define BRICK_GAME_H

#include "brick/clip.h"
#include "brick/defs.h"
#include "brick/event.h"
#include "brick/input.h"
#include "brick/rng.h"

BK_EXTERN_C_BEGIN

typedef enum { BK_PHASE_READY = 0, BK_PHASE_PLAYING = 1, BK_PHASE_PAUSED = 2, BK_PHASE_OVER = 3 } bk_phase_t;

typedef struct bk_game bk_game_t;

typedef struct {
    uint32_t seed;     /* RNG seed */
    uint32_t hi_score; /* best score of this game+mode so far */
    uint8_t speed;     /* start speed 1..10 */
    uint8_t level;     /* start level 1..10 */
    uint16_t mode;     /* variant, 0-based; out of range wraps modulo the number of modes */
} bk_game_options_t;

typedef struct {
    void (*reset)(bk_game_t *g);                        /* start a fresh party; score, speed and level are already reset */
    void (*update)(bk_game_t *g, const bk_input_t *in); /* one tick, never called during a cutscene or game over */
    void (*draw)(const bk_game_t *g, uint8_t cells[BK_CELLS]); /* BK_CELL_* values, row 0 = top */
    /* Fill the NEXT preview; return false when the game has none. May be NULL. */
    bool (*draw_preview)(const bk_game_t *g, uint8_t cells[BK_PREVIEW_CELLS]);
    void (*on_start)(bk_game_t *g); /* phase just became PLAYING. May be NULL. */
    /* Cutscene hooks (kind is chosen by the game). frame() paints the overlay (bk_game_overlay()); done() runs on the last
     * tick. */
    void (*scene_frame)(bk_game_t *g, uint8_t kind, uint16_t elapsed);
    void (*scene_done)(bk_game_t *g, uint8_t kind);
} bk_game_vtable_t;

struct bk_game {
    const bk_game_vtable_t *vt;

    /* ---- observable state (read-only for the shell) ---- */
    bk_phase_t phase;
    uint32_t score;
    uint32_t hi_score;
    uint8_t speed;     /* current speed 1..10 */
    uint8_t level;     /* current level 1..10 */
    int8_t lives;      /* -1: the game has no lives */
    uint32_t tick;     /* ticks played in PLAYING since the party began */
    uint8_t over_fill; /* game-over animation: rows already flooded from the bottom, 0..BK_ROWS */

    /* ---- base-class internals ---- */
    bk_rng_t rng;
    uint32_t
        scores_to_extra_life; /* > 0: every this many points an extra life (BrickBase.scoresToExtraLife); set by the game */
    uint16_t extra_lives;     /* extra lives earned in this party */
    uint16_t mode;            /* normalised variant */
    uint16_t mode_count;
    uint8_t start_speed;
    uint8_t start_level;
    uint8_t overlay[BK_CELLS]; /* cutscene frame, valid while overlay_on */
    bool overlay_on;
    bool dying; /* game-over animation running */
    uint16_t over_timer;
    bool scene_on;
    uint8_t scene_kind;
    uint16_t scene_total;
    uint16_t scene_elapsed;
    bk_event_queue_t *out; /* where emit() writes during a call; NULL outside of one */
};

/* ---- construction (called by a game's create function) ---- */
void bk_game_base_init(bk_game_t *g, const bk_game_vtable_t *vt, const bk_game_options_t *opt, uint16_t mode_count);

/* ---- shell API ---- */
void bk_game_start(bk_game_t *g, bk_event_queue_t *q); /* READY or OVER -> PLAYING */
void bk_game_pause(bk_game_t *g, bk_event_queue_t *q);
void bk_game_resume(bk_game_t *g, bk_event_queue_t *q);
void bk_game_restart(bk_game_t *g);                                         /* fresh party, back to READY */
void bk_game_step(bk_game_t *g, const bk_input_t *in, bk_event_queue_t *q); /* one tick; no-op unless PLAYING */
void bk_game_cells(const bk_game_t *g, uint8_t cells[BK_CELLS]);            /* overlay if any, else draw() */
bool bk_game_preview(const bk_game_t *g, uint8_t cells[BK_PREVIEW_CELLS]);  /* false: no preview */

/* What the 4 x 4 box of the status panel shows: the game's own NEXT preview, else the lives indicator (label LIFE). */
typedef enum { BK_PREVIEW_NONE = 0, BK_PREVIEW_NEXT = 1, BK_PREVIEW_LIVES = 2 } bk_preview_kind_t;
bk_preview_kind_t bk_game_preview_kind(const bk_game_t *g, uint8_t cells[BK_PREVIEW_CELLS]);

/* ---- helpers for game implementations ---- */
void bk_game_emit(bk_game_t *g, bk_event_type_t type, uint8_t a, uint16_t b, uint32_t c);
void bk_game_add_score(bk_game_t *g, uint32_t points);
void bk_game_set_score(bk_game_t *g, uint32_t value);
void bk_game_set_speed(bk_game_t *g, int value);                         /* wraps 0 -> 10 and 11 -> 1 like the toy */
void bk_game_game_over(bk_game_t *g);                                    /* start the flood animation, then PHASE_OVER */
void bk_game_cutscene(bk_game_t *g, uint8_t kind, uint32_t total_ticks); /* no-input animation */
bool bk_game_busy(const bk_game_t *g);                                   /* cutscene or game-over animation running */
uint8_t *bk_game_overlay(bk_game_t *g);                                  /* writable overlay; marks it as displayed */

/* ---- lives and "clear the field" scenes (BrickBase.setLives / animateClear / nextLevel, LibraryGame.lose / win) ---- */
#define BK_MAX_LIVES         4  /* lives shown by the indicator */
#define BK_CLEAR_SCENE_TICKS 60 /* the clear animation after a lost life or a won level (library games) */
#define BK_CLEAR_LOSE_TICKS  72 /* the same after a lost life in games derived straight from BrickBase (CLEAR_LOSE_TICKS) */

void bk_game_set_lives(bk_game_t *g, int count); /* clamped to 0..BK_MAX_LIVES */
/* Level + 1; past BK_MAX_LEVEL it wraps to 1 and the speed goes up by one (wrapping 10 -> 1). Emits BK_EV_LEVEL_UP. */
void bk_game_next_level(bk_game_t *g);
/* Paints the clear animation frame `elapsed` of a `total` tick scene into the overlay: the field floods from the bottom
 * in the first half and is erased from the top in the second. Call it from the game's scene_frame hook. */
void bk_game_paint_clear(bk_game_t *g, uint32_t total, uint16_t elapsed);
/* A life is lost: lives - 1, BK_EV_LIFE_LOST, then game over when none is left, else a clear scene of `scene_kind`
 * (BK_CLEAR_SCENE_TICKS) after which the game's scene_done hook must restart the round. */
void bk_game_lose_life(bk_game_t *g, uint8_t scene_kind);
/* The same with the length of the clear scene chosen by the game (BK_CLEAR_LOSE_TICKS for BrickBase games). */
void bk_game_lose_life_ticks(bk_game_t *g, uint8_t scene_kind, uint32_t ticks);

/* The explosion of BrickBase.animateKaboom: four passes over seven 5 x 5 waves around the cell (x, y) of the screen (column,
 * row from the top; the centre is moved inside the field), BK_KABOOM_WAVE_TICKS ticks per wave. Paints frame `elapsed` of
 * the BK_KABOOM_TICKS long scene into the overlay over a copy of `base` (the field as it was when the explosion began, which
 * the game must keep). */
#define BK_KABOOM_WAVE_TICKS 4
#define BK_KABOOM_TICKS      (4 * 7 * BK_KABOOM_WAVE_TICKS)
void bk_game_paint_kaboom(bk_game_t *g, const uint8_t base[BK_CELLS], int x, int y, uint16_t elapsed);
/* The level is won: +100 points, BK_EV_WIN, a clear scene of `scene_kind` after which scene_done must call
 * bk_game_next_level() and restart the round. */
void bk_game_win_level(bk_game_t *g, uint8_t scene_kind);

/* Ticks of the flood animation per row (reference: 127 ms at 60 Hz, 7.62 ticks) and total length. */
#define BK_GAME_OVER_ROW_MS     127
#define BK_GAME_OVER_HOLD_TICKS 30

/* ---- catalog entry ---- */

#define BK_GAME_USES_FIRE (1u << 0) /* cannot be played without a separate FIRE key (reference: usesFire) */

typedef struct {
    uint16_t uid;      /* stable numeric identity used in saves; never reuse or renumber */
    const char *id;    /* "tetris" (matches the reference registry id) */
    const char *title; /* upper-case ASCII for the built-in font */
    uint16_t mode_count;
    uint8_t flags;
    size_t state_size; /* sizeof the concrete game struct */
    size_t state_align;
    /* Construct a game in caller-provided storage (size >= state_size, aligned to state_align). */
    bk_game_t *(*create)(void *storage, const bk_game_options_t *opt);
    /* The looped gameplay animation of the menu (brick/clip.h). May be NULL: the menu then leaves the area empty. */
    const bk_clip_t *preview;
} bk_game_def_t;

BK_EXTERN_C_END

#endif /* BRICK_GAME_H */
