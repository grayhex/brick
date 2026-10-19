/*
 * Logical monochrome frame buffer: strictly 1 bit per dot (AGENTS.md, ADR-003).
 *
 * Polarity is POSITIVE (ADR-015): bit 1 = ink (dark dot), bit 0 = light background. The frame never contains
 * grey levels; whatever the physical panel does with it (reflective Memory LCD, transflective FSTN, scaling,
 * margins) is the display driver's business and is not decided here.
 *
 * Layout: row-major, MSB first (the leftmost dot of a byte is bit 7), `stride` bytes per row.
 */
#ifndef BRICK_FRAME_H
#define BRICK_FRAME_H

#include "brick/defs.h"

BK_EXTERN_C_BEGIN

/* Design grid: dots per game cell (even, BK_MIN_CELL_DOTS..BK_MAX_CELL_DOTS). The panel resolution is OPEN, so the
 * compositor is parametric; 8 is the reference grid used by the golden screens. */
#define BK_MIN_CELL_DOTS 8
#ifndef BK_MAX_CELL_DOTS
#define BK_MAX_CELL_DOTS 16
#endif

/* Whole screen = 10 playfield columns + 6.5 columns of status panel, 20 rows (ADR-016). */
#define BK_FRAME_MAX_W      (BK_MAX_CELL_DOTS * 33 / 2)
#define BK_FRAME_MAX_H      (BK_MAX_CELL_DOTS * BK_ROWS)
#define BK_FRAME_MAX_STRIDE ((BK_FRAME_MAX_W + 7) / 8)

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t stride; /* bytes per row */
    uint8_t bits[BK_FRAME_MAX_STRIDE * BK_FRAME_MAX_H];
} bk_frame_t;

void bk_frame_init(bk_frame_t *f, uint16_t width, uint16_t height); /* all light */
void bk_frame_clear(bk_frame_t *f);
bool bk_frame_get(const bk_frame_t *f, int x, int y);                         /* true = ink; outside = false */
void bk_frame_set(bk_frame_t *f, int x, int y, bool ink);                     /* clipped */
void bk_frame_fill_rect(bk_frame_t *f, int x, int y, int w, int h, bool ink); /* clipped */
bool bk_frame_equal(const bk_frame_t *a, const bk_frame_t *b);
size_t bk_frame_bytes(const bk_frame_t *f); /* bytes in use: stride * height */

BK_EXTERN_C_END

#endif /* BRICK_FRAME_H */
