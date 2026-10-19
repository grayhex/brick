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
    t.text_scale = (uint8_t)(width / 66u > 1u ? width / 66u : 1u);
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
static const char *const ICON_SOUND_ON[7] = {"..#....", ".##..#.", "###...#", "###...#", "###...#", ".##..#.", "..#...."};
static const char *const ICON_SOUND_OFF[7] = {"..#....", ".##.#.#", "###..#.", "###.#.#", "###....", ".##....", "..#...."};
static const char *const ICON_PAUSE[7] = {"##.##", "##.##", "##.##", "##.##", "##.##", "##.##", "##.##"};
// clang-format on

/* ---------------------------------------------------------------- game screen */

#define D(n) ((int)(n) * (int)l->cell / 8) /* design dots (cell = 8) -> real dots */
#define Y(n) ((int)l->field_y + D(n))      /* the same, below the vertical margin of a native panel */

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

    /* optional captions (menu: game title and mode label), centred in the playfield */
    if (s->caption1 != NULL) {
        bk_text_draw(out, (l->field_w - bk_text_width(s->caption1, l->label_scale)) / 2, Y(105), l->label_scale,
                     s->caption1);
    }
    if (s->caption2 != NULL) {
        bk_text_draw(out, (l->field_w - bk_text_width(s->caption2, l->label_scale)) / 2, Y(113), l->label_scale,
                     s->caption2);
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

    bk_text_draw(out, x0, Y(2), ls, "HI");
    bk_draw_number7(out, x_right, Y(9), dw, dh, dt, dg, s->hi_score, 7);

    if (s->show_score) {
        bk_text_draw(out, x0, Y(26), ls, "SCORE");
        bk_draw_number7(out, x_right, Y(33), dw, dh, dt, dg, s->score, 7);
    }

    /* NEXT preview: 4 x 4 cells, at the full cell pitch when the panel is wide enough, smaller otherwise */
    bk_text_draw(out, x0, Y(52), ls, "NEXT");
    if (s->has_preview) {
        const int pc = l->preview_cell;
        int px = l->panel_x + (l->panel_w - BK_PREVIEW_COLS * pc) / 2;
        int py = Y(60);
        for (int r = 0; r < BK_PREVIEW_ROWS; r++) {
            for (int c = 0; c < BK_PREVIEW_COLS; c++) {
                uint8_t v = s->preview[r * BK_PREVIEW_COLS + c];
                if (v == BK_CELL_ON || (v == BK_CELL_BLINK && blink_on)) {
                    draw_cell(out, pc, px + c * pc, py + r * pc);
                }
            }
        }
    }

    /* SPEED / LEVEL: single digits; the focused one blinks while the menu waits for input */
    bk_text_draw(out, x0, Y(100), ls, "SPEED");
    if (s->focus != BK_FOCUS_SPEED || blink_on) {
        bk_draw_number7(out, x0 + dw, Y(108), dw, dh, dt, dg, s->speed, 1);
    }
    bk_text_draw(out, x0, Y(124), ls, "LEVEL");
    if (s->focus != BK_FOCUS_LEVEL || blink_on) {
        bk_draw_number7(out, x0 + dw, Y(132), dw, dh, dt, dg, s->level, 1);
    }

    /* state icons */
    draw_bitmap(out, x0, Y(148), ls, s->sound_on ? ICON_SOUND_ON : ICON_SOUND_OFF, 7);
    if (s->paused) {
        draw_bitmap(out, x0 + inner_w - 5 * ls, Y(148), ls, ICON_PAUSE, 7);
    }
}

/* ---------------------------------------------------------------- text pages */

void bk_render_text_page(const bk_layout_t *l, const char *title, const bk_text_row_t *rows, size_t count, int cursor,
                         const char *footer, bk_frame_t *out)
{
    bk_frame_init(out, l->width, l->height);
    const int ts = l->text_scale;
    const int margin = D(4);
    const int pitch = bk_text_pitch(ts);
    const int right = l->width - margin;

    bk_text_draw(out, margin, D(6), ts, title);
    bk_frame_fill_rect(out, margin, D(6) + bk_text_height(ts) + D(3), l->width - 2 * margin, l->ring, true);

    const int line_h = bk_text_height(ts) + D(4);
    int y = D(28);
    for (size_t i = 0; i < count; i++, y += line_h) {
        if ((int)i == cursor) {
            bk_text_draw(out, margin, y, ts, ">");
        }
        bk_text_draw(out, margin + pitch, y, ts, rows[i].label);
        if (rows[i].value != NULL) {
            bk_text_draw(out, right - bk_text_width(rows[i].value, ts), y, ts, rows[i].value);
        }
    }
    if (footer != NULL) {
        bk_text_draw(out, margin, l->height - D(10), l->label_scale, footer);
    }
}
