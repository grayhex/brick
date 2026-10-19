/*
 * Compositor: turns a "scene" (what the console wants to show) into a strictly 1-bit logical frame.
 * It knows nothing about the physical display: only a design grid of `cell` dots per game cell.
 *
 *   +----------------------+-------------+
 *   |                      | HI-SCORE    |
 *   |   10 x 20 cells      | SCORE       |
 *   |   of the game        | NEXT 4x4    |   panel width = 6.5 cells, to the RIGHT of the playfield (ADR-016)
 *   |                      | SPEED LEVEL |
 *   |                      | icons       |
 *   +----------------------+-------------+
 *
 * Cells are always square; the aspect ratio is never distorted. Positive polarity: ink on a light background (ADR-015).
 * Unlit cells are drawn as nothing at all (no "ghost" grey): the logical frame has no third level.
 */
#ifndef BRICK_RENDER_H
#define BRICK_RENDER_H

#include "brick/defs.h"
#include "brick/frame.h"

BK_EXTERN_C_BEGIN

typedef struct {
    uint8_t cell;     /* dots per cell, even, BK_MIN_CELL_DOTS..BK_MAX_CELL_DOTS */
    uint16_t width;   /* 16.5 cells */
    uint16_t height;  /* 20 cells */
    uint16_t field_w; /* 10 cells */
    uint16_t panel_x; /* first dot right of the divider */
    uint16_t panel_w;
    uint8_t gap;         /* dots between neighbouring cells */
    uint8_t ring;        /* thickness of the cell outline and of the divider */
    uint8_t label_scale; /* text scale of the panel labels */
    uint8_t text_scale;  /* text scale of full-screen text pages */
} bk_layout_t;

/* Returns false (and leaves *l untouched) if `cell_dots` is odd or out of range. */
bool bk_layout_init(bk_layout_t *l, unsigned cell_dots);

typedef enum { BK_FOCUS_NONE = 0, BK_FOCUS_SPEED = 1, BK_FOCUS_LEVEL = 2 } bk_focus_t;

typedef struct {
    uint8_t cells[BK_CELLS]; /* BK_CELL_OFF / ON / BLINK, row 0 = top */
    bool has_preview;
    uint8_t preview[BK_PREVIEW_CELLS];
    uint32_t score;
    uint32_t hi_score;
    bool show_score;   /* false in the menu: only the record is shown */
    uint8_t speed;     /* displayed value, 0..9 */
    uint8_t level;     /* displayed value, 0..9 */
    uint8_t over_fill; /* flooded rows from the bottom (game over animation) */
    bool paused;
    bool sound_on;
    uint8_t focus;        /* bk_focus_t: the focused value blinks (menu) */
    uint32_t tick;        /* source of the blink phase */
    const char *caption1; /* optional text lines centred under the menu art (rows 13-14); NULL = none */
    const char *caption2;
} bk_scene_t;

void bk_render_scene(const bk_layout_t *l, const bk_scene_t *scene, bk_frame_t *out);

/* True while blinking elements are visible. */
bool bk_blink_phase(uint32_t tick);

typedef struct {
    const char *label;
    const char *value; /* may be NULL */
} bk_text_row_t;

/* Full-screen list page (settings, about, button test). `cursor` < 0: no cursor. */
void bk_render_text_page(const bk_layout_t *l, const char *title, const bk_text_row_t *rows, size_t count, int cursor,
                         const char *footer, bk_frame_t *out);

/* Draw `value` right-aligned in a row of 7-segment digits ending at x_right; digits < 0 are blank. Exposed for tests. */
void bk_draw_number7(bk_frame_t *f, int x_right, int y, int digit_w, int digit_h, int thick, int gap, uint32_t value,
                     int max_digits);

BK_EXTERN_C_END

#endif /* BRICK_RENDER_H */
