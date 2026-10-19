/*
 * "Lab" for Tetris tests: the C counterpart of the reference tests/brick/games/tetrisLab.ts.
 * It sets exact positions (wipe / fill / put) and steps the game, collecting events.
 * y is the donor height: 0 is the bottom of the well.
 */
#ifndef BK_TETRIS_LAB_H
#define BK_TETRIS_LAB_H

#include "brick/games/tetris.h"
#include "tetris_internal.h"

#define LAB_LOG_CAP 512

typedef struct {
    bkt_game_t game;
    bk_event_t log[LAB_LOG_CAP];
    unsigned log_count;
} lab_t;

/* Options with the reference defaults used by plainLab: seed 5, mode 0. */
bk_game_options_t lab_options(uint32_t seed, unsigned speed, unsigned level, uint32_t hi_score);

/* Create and start (like plainLab()). */
void lab_init(lab_t *lab, const bk_game_options_t *opt);
void lab_init_plain(lab_t *lab); /* seed 5, speed 1, level 1 */

static inline bk_game_t *lab_g(lab_t *lab)
{
    return &lab->game.base;
}

/* ---- stepping ---- */
void lab_log_clear(lab_t *lab);
void lab_step(lab_t *lab, uint8_t pressed, uint8_t held);
void lab_idle(lab_t *lab, unsigned ticks);
void lab_tap(lab_t *lab, uint8_t keys);   /* one tick pressed, one tick released */
void lab_press(lab_t *lab, uint8_t keys); /* one tick pressed, no release */
void lab_hold(lab_t *lab, uint8_t key, unsigned ticks);
unsigned lab_until_landed(lab_t *lab, unsigned limit);
unsigned lab_until_phase(lab_t *lab, bk_phase_t phase, unsigned limit);

bool lab_has(const lab_t *lab, bk_event_type_t type);
unsigned lab_count_events(const lab_t *lab, bk_event_type_t type);

/* ---- state setup (reference Lab methods) ---- */
void lab_wipe(lab_t *lab);
void lab_fill(lab_t *lab, int y, const char *pattern); /* 10 chars, '#' is a cell */
void lab_put(lab_t *lab, int shape, int angle, int x, int y);
void lab_set_next(lab_t *lab, int shape, int angle);
void lab_respawn(lab_t *lab, int shape, int angle);
void lab_due_fall(lab_t *lab);
void lab_poke(lab_t *lab, uint32_t score);

/* ---- queries ---- */
int lab_cell(const lab_t *lab, int x, int y);
int lab_count(const lab_t *lab);
void lab_rows(lab_t *lab, char rows[BK_ROWS][BK_COLS + 1]); /* screen picture */
void lab_preview_rows(lab_t *lab, char rows[BK_PREVIEW_ROWS][BK_PREVIEW_COLS + 1]);

/* Catalogue index by donor name ("Z", "S", "Line", "T", "Square", "L", "MirroredL"); -1 if unknown. */
int lab_shape_by_name(const char *name);

#endif /* BK_TETRIS_LAB_H */
