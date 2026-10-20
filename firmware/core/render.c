#include "brick/render.h"

#include <string.h>

#include "brick/font.h"

/* Everything below the frame size and the cell pitch follows from these two (plus the vertical margin). */
static bool layout_build(bk_layout_t *l, unsigned cell, unsigned width, unsigned height, unsigned field_y)
{
    if (width > BK_FRAME_MAX_W || height > BK_FRAME_MAX_H || cell < BK_MIN_CELL_DOTS) {
        return false;
    }
    bk_layout_t t;
    memset(&t, 0, sizeof t);
    t.cell = (uint8_t)cell;
    t.width = (uint16_t)width;
    t.height = (uint16_t)height;
    t.field_y = (uint16_t)field_y;
    t.field_w = (uint16_t)(cell * BK_COLS);
    t.gap = (uint8_t)((cell + 4u) / 8u);
    t.ring = t.gap;
    t.panel_x = (uint16_t)(t.field_w + t.ring);
    if (t.panel_x >= width) {
        return false;
    }
    t.panel_w = (uint16_t)(width - t.panel_x);

    /* Panel content is sized from the width actually available, so a narrow status column still holds seven digits. */
    const unsigned inner = t.panel_w - 2u * t.ring;
    const unsigned pitch = inner / 7u;
    t.digit_gap = (uint8_t)(pitch / 7u > 1u ? pitch / 7u : 1u);
    t.digit_w = (uint8_t)(pitch - t.digit_gap);
    t.digit_h = (uint8_t)((t.digit_w * 9u / 5u) | 1u);
    t.digit_t = (uint8_t)(t.digit_h / 7u > 1u ? t.digit_h / 7u : 1u); /* thick enough to be a stroke, not a hairline */
    /* Labels are about 0.6 cell tall (5 dots * scale); never wider than the column ("SCORE" = 19 dots * scale). */
    unsigned ls = (cell * 3u + 12u) / 25u;
    if (ls > inner / 19u) {
        ls = inner / 19u;
    }
    t.label_scale = (uint8_t)(ls > 1u ? ls : 1u);
    t.preview_cell = (uint8_t)(inner / 4u < cell ? inner / 4u : cell);
    *l = t;
    return true;
}

bool bk_layout_init(bk_layout_t *l, unsigned cell_dots)
{
    if (cell_dots > BK_MAX_CELL_DOTS || (cell_dots % 2u) != 0u) {
        return false;
    }
    return layout_build(l, cell_dots, cell_dots * 33u / 2u, cell_dots * BK_ROWS, 0u);
}

bool bk_layout_fit(bk_layout_t *l, unsigned width, unsigned height)
{
    for (unsigned cell = height / BK_ROWS; cell >= BK_MIN_CELL_DOTS; cell--) {
        unsigned ring = (cell + 4u) / 8u;
        if (cell * BK_COLS + ring + BK_MIN_STATUS_DOTS <= width) {
            return layout_build(l, cell, width, height, (height - cell * BK_ROWS) / 2u);
        }
    }
    return false;
}

bool bk_blink_phase(uint32_t tick)
{
    return ((tick / BK_BLINK_HALF_TICKS) & 1u) == 0u;
}

/* ---------------------------------------------------------------- cells */

/* One lit cell: outline, gap, core (classic LCD look); always a square. The preview uses a smaller pitch on a narrow panel.
 */
static void draw_cell(bk_frame_t *f, int pitch, int x, int y)
{
    int gap = (pitch + 4) / 8;
    int box = pitch - gap;
    int t = gap;
    bk_frame_fill_rect(f, x, y, box, box, true);
    bk_frame_fill_rect(f, x + t, y + t, box - 2 * t, box - 2 * t, false);
    bk_frame_fill_rect(f, x + 2 * t, y + 2 * t, box - 4 * t, box - 4 * t, true);
}

/* ---------------------------------------------------------------- seven segments */

/* Segment masks a..g = bits 0..6. */
static const uint8_t SEGMENTS[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

static void draw_digit7(bk_frame_t *f, int x, int y, int w, int h, int t, int digit)
{
    if (digit < 0 || digit > 9) {
        return;
    }
    uint8_t s = SEGMENTS[digit];
    int mid = (h - t) / 2; /* top of the middle bar */
    int upper = mid - t;   /* height of an upper vertical segment */
    int lower = h - t - (mid + t);
    if (s & 0x01) {
        bk_frame_fill_rect(f, x + t, y, w - 2 * t, t, true); /* a */
    }
    if (s & 0x02) {
        bk_frame_fill_rect(f, x + w - t, y + t, t, upper, true); /* b */
    }
    if (s & 0x04) {
        bk_frame_fill_rect(f, x + w - t, y + mid + t, t, lower, true); /* c */
    }
    if (s & 0x08) {
        bk_frame_fill_rect(f, x + t, y + h - t, w - 2 * t, t, true); /* d */
    }
    if (s & 0x10) {
        bk_frame_fill_rect(f, x, y + mid + t, t, lower, true); /* e */
    }
    if (s & 0x20) {
        bk_frame_fill_rect(f, x, y + t, t, upper, true); /* f */
    }
    if (s & 0x40) {
        bk_frame_fill_rect(f, x + t, y + mid, w - 2 * t, t, true); /* g */
    }
}

void bk_draw_number7(bk_frame_t *f, int x_right, int y, int digit_w, int digit_h, int thick, int gap, uint32_t value,
                     int max_digits)
{
    int digits[10];
    int n = 0;
    do {
        digits[n++] = (int)(value % 10u);
        value /= 10u;
    } while (value != 0u && n < max_digits);
    for (int i = 0; i < n; i++) {
        int x = x_right - digit_w - i * (digit_w + gap);
        draw_digit7(f, x, y, digit_w, digit_h, thick, digits[i]);
    }
}

/* Two seven-segment digits with the leading zero ("03"), the left one starting at x. */
static void draw_two_digits(bk_frame_t *f, int x, int y, int digit_w, int digit_h, int thick, int gap, unsigned value)
{
    value %= 100u;
    draw_digit7(f, x, y, digit_w, digit_h, thick, (int)(value / 10u));
    draw_digit7(f, x + digit_w + gap, y, digit_w, digit_h, thick, (int)(value % 10u));
}

/* ---------------------------------------------------------------- icons */

static void draw_bitmap(bk_frame_t *f, int x, int y, int scale, const char *const *rows, int nrows)
{
    for (int r = 0; r < nrows; r++) {
        for (int c = 0; rows[r][c] != '\0'; c++) {
            if (rows[r][c] == '#') {
                bk_frame_fill_rect(f, x + c * scale, y + r * scale, scale, scale, true);
            }
        }
    }
}

// clang-format off
static const char *const ICON_SOUND_OFF[7] = {"..#....", ".##.#.#", "###..#.", "###.#.#", "###....", ".##....", "..#...."};
// clang-format on

/* ---------------------------------------------------------------- game screen */

#define D(n) ((int)(n) * (int)l->cell / 8) /* design dots (cell = 8) -> real dots */
#define Y(n) ((int)l->field_y + D(n))      /* the same, below the vertical margin of a native panel */

/* Slots of the status column in design dots: the label's top; a value is drawn 7 dots below it. */
#define SLOT_HI    2
#define SLOT_SCORE 26
#define SLOT_NEXT  52
#define SLOT_SPEED 100
#define SLOT_LEVEL 124
#define SLOT_ICON  148

/* The pause sign of the mock-up (ui-tetris-pause): a box of 6 x 4 cells in the middle of the playfield with two bars. The
 * field behind it is cleared, so blinking cells never show through. */
static void draw_pause_box(bk_frame_t *out, const bk_layout_t *l)
{
    const int cell = l->cell;
    const int x = 2 * cell;
    const int y = (int)l->field_y + 8 * cell;
    const int w = 6 * cell;
    const int h = 4 * cell;
    const int t = l->ring > 0 ? l->ring : 1;
    bk_frame_fill_rect(out, x, y, w, h, true);
    bk_frame_fill_rect(out, x + t, y + t, w - 2 * t, h - 2 * t, false);
    const int bar_w = cell / 2;
    const int bar_gap = cell / 2;
    const int bar_h = 2 * cell;
    const int cx = x + w / 2;
    const int by = y + (h - bar_h) / 2;
    bk_frame_fill_rect(out, cx - bar_gap / 2 - bar_w, by, bar_w, bar_h, true);
    bk_frame_fill_rect(out, cx + bar_gap / 2, by, bar_w, bar_h, true);
}

void bk_render_scene(const bk_layout_t *l, const bk_scene_t *s, bk_frame_t *out)
{
    bk_frame_init(out, l->width, l->height);
    const bool blink_on = bk_blink_phase(s->tick);
    const int P = l->cell;

    /* playfield: only lit cells are drawn; BLINK cells follow the blink phase; the game-over flood overrides */
    for (int row = 0; row < BK_ROWS; row++) {
        for (int col = 0; col < BK_COLS; col++) {
            uint8_t v = s->cells[row * BK_COLS + col];
            bool lit = v == BK_CELL_ON || (v == BK_CELL_BLINK && blink_on) || row >= BK_ROWS - (int)s->over_fill;
            if (lit) {
                draw_cell(out, P, col * P, (int)l->field_y + row * P);
            }
        }
    }
    if (s->paused) {
        draw_pause_box(out, l);
    }

    /* divider between playfield and panel */
    bk_frame_fill_rect(out, l->field_w, 0, l->ring, l->height, true);

    /* status panel */
    const int x0 = l->panel_x + l->ring;
    const int inner_w = l->panel_w - 2 * l->ring;
    const int ls = l->label_scale;
    const int dw = l->digit_w;
    const int dh = l->digit_h;
    const int dt = l->digit_t;
    const int dg = l->digit_gap;
    const int x_right = x0 + inner_w;

    bk_text_draw(out, x0, Y(SLOT_HI), ls, "HI");
    bk_draw_number7(out, x_right, Y(SLOT_HI + 7), dw, dh, dt, dg, s->hi_score, 7);

    /* the game panel is HI, SCORE, NEXT (or LIFE), SPD, LVL; the menu panel is HI, SPD, LVL in the first slots */
    const int slot_speed = s->show_score ? SLOT_SPEED : SLOT_SCORE;
    const int slot_level = s->show_score ? SLOT_LEVEL : SLOT_NEXT;

    if (s->show_score) {
        bk_text_draw(out, x0, Y(SLOT_SCORE), ls, "SCORE");
        bk_draw_number7(out, x_right, Y(SLOT_SCORE + 7), dw, dh, dt, dg, s->score, 7);

        /* NEXT / LIFE box: 4 x 4 cells, at the full cell pitch when the panel is wide enough, smaller otherwise */
        if (s->has_preview) {
            bk_text_draw(out, x0, Y(SLOT_NEXT), ls, s->preview_is_lives ? "LIFE" : "NEXT");
            const int pc = l->preview_cell;
            int px = l->panel_x + (l->panel_w - BK_PREVIEW_COLS * pc) / 2;
            int py = Y(SLOT_NEXT + 8);
            for (int r = 0; r < BK_PREVIEW_ROWS; r++) {
                for (int c = 0; c < BK_PREVIEW_COLS; c++) {
                    uint8_t v = s->preview[r * BK_PREVIEW_COLS + c];
                    if (v == BK_CELL_ON || (v == BK_CELL_BLINK && blink_on)) {
                        draw_cell(out, pc, px + c * pc, py + r * pc);
                    }
                }
            }
        }
    }

    /* SPD / LVL: always two digits, as in the mock-up (01..10); the focused one blinks while the menu waits for input */
    bk_text_draw(out, x0, Y(slot_speed), ls, "SPD");
    if (s->focus != BK_FOCUS_SPEED || blink_on) {
        draw_two_digits(out, x0, Y(slot_speed + 8), dw, dh, dt, dg, s->speed);
    }
    bk_text_draw(out, x0, Y(slot_level), ls, "LVL");
    if (s->focus != BK_FOCUS_LEVEL || blink_on) {
        draw_two_digits(out, x0, Y(slot_level + 8), dw, dh, dt, dg, s->level);
    }

    if (!s->sound_on) {
        draw_bitmap(out, x0, Y(SLOT_ICON), ls, ICON_SOUND_OFF, 7);
    }
}

/* ---------------------------------------------------------------- text pages */

/* Largest scale not above `scale` at which `s` is at most `max_w` dots wide (at least 1). */
static int fit_scale(const char *s, int scale, int max_w)
{
    while (scale > 1 && bk_text_width(s, scale) > max_w) {
        scale--;
    }
    return scale;
}

/* Text of the rows: about 0.6 .. 0.75 of a cell tall, as in the mock-up (label and value are the same size). */
static int item_scale(const bk_layout_t *l)
{
    int s = ((int)l->cell * 3 + 10) / 20;
    return s > 1 ? s : 1;
}

/* One scale for every row of the page: the largest at which the widest row still fits `avail_w`. */
static int page_scale(const bk_layout_t *l, const bk_page_t *p, int avail_w)
{
    int s = item_scale(l);
    for (size_t i = 0; i < p->count; i++) {
        const char *label = p->rows[i].label;
        const char *value = p->rows[i].value;
        while (s > 1) {
            int w = bk_text_width(label, s);
            if (value != NULL && p->style == BK_PAGE_LINES) {
                w += bk_text_pitch(s) + bk_text_width(value, s);
            } else if (value != NULL && bk_text_width(value, s) > w) {
                w = bk_text_width(value, s);
            }
            if (w <= avail_w) {
                break;
            }
            s--;
        }
    }
    return s;
}

static void draw_box(bk_frame_t *f, int x, int y, int w, int h, int t)
{
    bk_frame_fill_rect(f, x, y, w, t, true);
    bk_frame_fill_rect(f, x, y + h - t, w, t, true);
    bk_frame_fill_rect(f, x, y, t, h, true);
    bk_frame_fill_rect(f, x + w - t, y, t, h, true);
}

/* A small triangle, three rows of `scale` dots tall, centred on cx: the list continues up or down. */
static void draw_arrow(bk_frame_t *f, int cx, int y, int scale, bool up)
{
    for (int i = 0; i < 3; i++) {
        int half = (up ? i : 2 - i) * scale;
        bk_frame_fill_rect(f, cx - half - scale / 2, y + i * scale, 2 * half + scale, scale, true);
    }
}

/* The right column of a page: three items, label over value (text), and the muted-speaker icon. */
static void draw_page_status(bk_frame_t *out, const bk_layout_t *l, const bk_page_t *p)
{
    static const int slot[BK_PAGE_STATUS_ITEMS] = {SLOT_HI, SLOT_SCORE, SLOT_NEXT};
    const int x0 = l->panel_x + l->ring;
    const int inner_w = l->panel_w - 2 * l->ring;
    const int ls = l->label_scale;
    for (int i = 0; i < BK_PAGE_STATUS_ITEMS; i++) {
        if (p->status[i].label == NULL) {
            continue; /* the slot stays empty, the others keep their places */
        }
        bk_text_draw(out, x0, Y(slot[i]), fit_scale(p->status[i].label, ls, inner_w), p->status[i].label);
        if (p->status[i].value != NULL) {
            bk_text_draw(out, x0, Y(slot[i] + 7), fit_scale(p->status[i].value, ls + 1, inner_w), p->status[i].value);
        }
    }
    if (!p->sound_on) {
        draw_bitmap(out, x0, Y(SLOT_ICON), ls, ICON_SOUND_OFF, 7);
    }
}

/* Rows of the list: pairs 22 design dots apart (2.75 cells, the rhythm of the mock-up) from design dot 32 down to one cell
 * above the bottom of the matrix. */
#define LIST_FIRST_Y 32
#define LIST_PITCH   22

size_t bk_page_rows_visible(const bk_layout_t *l)
{
    const int pitch = D(LIST_PITCH);
    const int avail = D(BK_ROWS * 8 - 8) - D(LIST_FIRST_Y);
    return (pitch > 0 && avail > 0) ? (size_t)(avail / pitch) : 0u;
}

static void render_list(const bk_layout_t *l, const bk_page_t *p, bk_frame_t *out)
{
    const int x_box = D(4);
    const int x_text = D(8);
    const int box_w = (int)l->field_w - 2 * x_box;
    const int is = page_scale(l, p, (int)l->field_w - 2 * x_text);
    const int th = 5 * is;
    const int gap_in = is;           /* between the label and its value */
    const int pad = is > 2 ? is : 2; /* between the pair and its box */
    const int pair_h = 2 * th + gap_in;
    const size_t visible = bk_page_rows_visible(l);

    for (size_t k = 0; k < visible && p->top + k < p->count; k++) {
        const size_t i = p->top + k;
        const int y = Y(LIST_FIRST_Y) + (int)k * D(LIST_PITCH);
        bk_text_draw(out, x_text, y, is, p->rows[i].label);
        if (p->rows[i].value != NULL) {
            bk_text_draw(out, x_text, y + th + gap_in, is, p->rows[i].value);
        }
        if ((int)i == p->cursor) {
            draw_box(out, x_box, y - pad, box_w, pair_h + 2 * pad, l->ring > 0 ? l->ring : 1);
        }
    }
    const int cx = (int)l->field_w - D(8);
    if (p->top > 0) {
        draw_arrow(out, cx, Y(LIST_FIRST_Y) - D(8), is, true);
    }
    if (p->top + visible < p->count) {
        draw_arrow(out, cx, Y(BK_ROWS * 8 - 14), is, false);
    }
}

static void render_lines(const bk_layout_t *l, const bk_page_t *p, bk_frame_t *out)
{
    const int margin = D(4);
    const int is = page_scale(l, p, (int)l->field_w - 2 * margin);
    int y = Y(24);
    for (size_t i = 0; i < p->count; i++, y += D(8)) {
        bk_text_draw(out, margin, y, is, p->rows[i].label);
        if (p->rows[i].value != NULL) {
            bk_text_draw(out, (int)l->field_w - margin - bk_text_width(p->rows[i].value, is), y, is, p->rows[i].value);
        }
    }
    /* the footer lines end one cell above the bottom of the matrix */
    int lines = 0;
    while (lines < BK_PAGE_FOOTER_LINES && p->footer[lines] != NULL) {
        lines++;
    }
    const int ls = l->label_scale;
    const int line_h = 6 * ls;
    int fy = Y(BK_ROWS * 8 - 8) - lines * line_h;
    for (int i = 0; i < lines; i++, fy += line_h) {
        bk_text_draw(out, margin, fy, fit_scale(p->footer[i], ls, (int)l->field_w - 2 * margin), p->footer[i]);
    }
}

void bk_render_page(const bk_layout_t *l, const bk_page_t *p, bk_frame_t *out)
{
    bk_frame_init(out, l->width, l->height);
    const int x_text = D(8);
    const int title_w = (int)l->field_w - 2 * D(4);
    bk_text_draw(out, x_text, Y(8), fit_scale(p->title, l->label_scale + 1, title_w), p->title);
    if (p->style == BK_PAGE_LIST) {
        render_list(l, p, out);
    } else {
        render_lines(l, p, out);
    }
    bk_frame_fill_rect(out, l->field_w, 0, l->ring, l->height, true);
    draw_page_status(out, l, p);
}
