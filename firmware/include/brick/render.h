/*
 * Compositor: turns a "scene" (what the console wants to show) into a strictly 1-bit logical frame.
 * It knows nothing about the physical display: only a grid of `cell` dots per game cell, either the reference design
 * grid (bk_layout_init) or a fit to a native panel size (bk_layout_fit).
 *
 *   +----------------------+-------------+
 *   |                      | HI          |
 *   |   10 x 20 cells      | SCORE       |
 *   |   of the game        | NEXT 4x4    |   panel width = 6.5 cells, to the RIGHT of the playfield (ADR-016)
 *   |                      | SPD         |   (the menu shows only HI, SPD and LVL, text pages their own three items)
 *   |                      | LVL         |
 *   +----------------------+-------------+
 *   (the status column is 6.5 cells wide on the reference grid, as narrow as ~3 cells when fitted to a portrait panel)
 *
 * The layout follows the owner's interface mock-ups (design/renders, ADR-041): the pause is a boxed pause sign over the
 * playfield, text pages (settings) show label over value with a box round the selected item and the same status column on
 * the right.
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
    uint8_t cell;         /* dots per game cell (square) */
    uint16_t width;       /* frame width in dots */
    uint16_t height;      /* frame height in dots */
    uint16_t field_y;     /* top margin above the 10 x 20 playfield (vertical slack of a native panel) */
    uint16_t field_w;     /* 10 cells */
    uint16_t panel_x;     /* first dot right of the divider */
    uint16_t panel_w;     /* status panel width, divider excluded */
    uint8_t gap;          /* dots between neighbouring cells */
    uint8_t ring;         /* thickness of the cell outline and of the divider */
    uint8_t label_scale;  /* text scale of the panel labels */
    uint8_t preview_cell; /* pitch of the NEXT 4 x 4 preview (<= cell: it shrinks to fit a narrow panel) */
    uint8_t digit_w;      /* seven-segment digits: width, height, segment thickness, gap */
    uint8_t digit_h;
    uint8_t digit_t;
    uint8_t digit_gap;
} bk_layout_t;

/* Narrowest status column (divider excluded) the compositor accepts: seven small digits still fit. */
#define BK_MIN_STATUS_DOTS 28

/*
 * Reference design grid: `cell_dots` per cell (even, BK_MIN_CELL_DOTS..BK_MAX_CELL_DOTS), screen = 16.5 x 20 cells,
 * i.e. a status panel 6.5 cells wide. Returns false (and leaves *l untouched) for an odd or out-of-range size or a
 * frame that does not fit BK_FRAME_MAX_W x BK_FRAME_MAX_H.
 */
bool bk_layout_init(bk_layout_t *l, unsigned cell_dots);

/*
 * Fit to a native panel of width x height dots: the largest whole number of dots per cell such that the 10 x 20
 * playfield fits and at least BK_MIN_STATUS_DOTS remain on the RIGHT for the status column, which takes ALL the
 * remaining width (so the status column is as wide as the panel allows, e.g. 76 dots on 336 x 536). The playfield is
 * centred vertically when the panel is taller than 20 cells. Cells stay square. Returns false if nothing fits.
 */
bool bk_layout_fit(bk_layout_t *l, unsigned width, unsigned height);

typedef enum { BK_FOCUS_NONE = 0, BK_FOCUS_SPEED = 1, BK_FOCUS_LEVEL = 2 } bk_focus_t;

typedef struct {
    uint8_t cells[BK_CELLS]; /* BK_CELL_OFF / ON / BLINK, row 0 = top */
    bool has_preview;        /* the 4 x 4 box under the label NEXT (or LIFE) */
    bool preview_is_lives;   /* the box shows the lives of the game, not the next piece: label LIFE */
    uint8_t preview[BK_PREVIEW_CELLS];
    uint32_t score;
    uint32_t hi_score;
    bool show_score;   /* false in the menu: only HI, SPD and LVL, one under the other */
    uint8_t speed;     /* SPD, 0..99, always two digits (01..10 in the menu and the games) */
    uint8_t level;     /* LVL, the same */
    uint8_t over_fill; /* flooded rows from the bottom (game over animation) */
    bool paused;       /* the pause sign in a box over the playfield */
    bool sound_on;     /* the speaker icon is shown only while the sound is switched off */
    uint8_t focus;     /* bk_focus_t: the focused value blinks (menu) */
    uint32_t tick;     /* source of the blink phase */
} bk_scene_t;

void bk_render_scene(const bk_layout_t *l, const bk_scene_t *scene, bk_frame_t *out);

/* True while blinking elements are visible. */
bool bk_blink_phase(uint32_t tick);

typedef struct {
    const char *label;
    const char *value; /* may be NULL */
} bk_text_row_t;

typedef enum {
    BK_PAGE_LIST = 0,  /* settings: the label over its value, a box round the selected row, scrolls */
    BK_PAGE_LINES = 1, /* button test, about: one line per row, the value right-aligned, no cursor */
} bk_page_style_t;

#define BK_PAGE_STATUS_ITEMS 3
#define BK_PAGE_FOOTER_LINES 2

/* A full-screen text page: the same 10 x 20 matrix area and status column as the game screens (design/renders). */
typedef struct {
    const char *title;
    bk_page_style_t style;
    const bk_text_row_t *rows;
    size_t count;
    int cursor; /* LIST: the boxed row, < 0 none */
    size_t top; /* LIST: the first row shown (the console keeps the cursor in view, see bk_page_rows_visible) */
    const char *footer[BK_PAGE_FOOTER_LINES]; /* LINES: short lines at the bottom of the matrix area; NULL = none */
    bk_text_row_t
        status[BK_PAGE_STATUS_ITEMS]; /* the right column, label over value (text); a NULL label leaves the slot empty */
    bool sound_on;                    /* the speaker icon is shown only while the sound is switched off */
} bk_page_t;

/* Rows of a LIST page that fit under the title (the same for every grid: 5). */
size_t bk_page_rows_visible(const bk_layout_t *l);

void bk_render_page(const bk_layout_t *l, const bk_page_t *page, bk_frame_t *out);

/* Draw `value` right-aligned in a row of 7-segment digits ending at x_right; digits < 0 are blank. Exposed for tests. */
void bk_draw_number7(bk_frame_t *f, int x_right, int y, int digit_w, int digit_h, int thick, int gap, uint32_t value,
                     int max_digits);

BK_EXTERN_C_END

#endif /* BRICK_RENDER_H */
