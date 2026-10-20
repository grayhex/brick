/*
 * Looped gameplay clips: the animated preview of a game in the menu (docs/UI_MENU.md section 4, ADR-041).
 *
 * The menu shows the game number, the mode number and, in the bottom five rows of the 10 x 20 matrix, a short looped
 * animation of the game being played: a piece lands and a line burns out, a snake eats, a car dodges. A clip is plain
 * constant data owned by the game (no state, no heap, no RNG): `frame_count` frames of 10 x 5 cells that the console
 * steps through at `frame_ticks` per frame. It is a picture of the gameplay, not the game: the rules are not run in the
 * menu (the game state memory is not touched either).
 *
 * The animation is a source of panel refreshes: a Memory LCD is rewritten for every new picture (the Sharp LS032B7DD02
 * quotes 1..10 Hz, docs/DISPLAY_CANDIDATES.md), so clips are authored at a few pictures per second and the user can
 * switch the animation off (setting DEMO): the menu then shows the `poster` frame and the picture stays still.
 */
#ifndef BRICK_CLIP_H
#define BRICK_CLIP_H

#include "brick/defs.h"

BK_EXTERN_C_BEGIN

#define BK_CLIP_COLS  BK_COLS
#define BK_CLIP_ROWS  5
#define BK_CLIP_CELLS (BK_CLIP_COLS * BK_CLIP_ROWS)

/* The matrix row of the menu on which the clip starts (the clip fills rows 15..19). */
#define BK_CLIP_FIRST_ROW (BK_ROWS - BK_CLIP_ROWS)

typedef struct {
    uint16_t frame_count; /* frames in the loop, at least 2 */
    uint8_t frame_ticks;  /* ticks one frame stays on the screen: BK_TICK_HZ / frame_ticks pictures per second */
    uint8_t poster;       /* the frame shown while the animation is off, < frame_count */
    /* frame_count strings of BK_CLIP_ROWS rows of BK_CLIP_COLS characters, row 0 on top: '#' = lit cell, '.' = dark */
    const char (*frames)[BK_CLIP_CELLS + 1];
} bk_clip_t;

/* Frame on the screen `tick` ticks after the clip started; the loop wraps. */
uint16_t bk_clip_frame_at(const bk_clip_t *clip, uint32_t tick);

/* Expand a frame into BK_CELL_OFF / BK_CELL_ON cells (row 0 on top). A frame number past the end wraps. */
void bk_clip_paint(const bk_clip_t *clip, uint16_t frame, uint8_t cells[BK_CLIP_CELLS]);

/* Structural check used by the tests and at console start: sizes, frame characters, poster in range. */
bool bk_clip_valid(const bk_clip_t *clip);

BK_EXTERN_C_END

#endif /* BRICK_CLIP_H */
