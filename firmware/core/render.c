#include "brick/render.h"

#include <string.h>

#include "brick/font.h"

bool bk_layout_init(bk_layout_t *l, unsigned cell_dots)
{
    if (cell_dots < BK_MIN_CELL_DOTS || cell_dots > BK_MAX_CELL_DOTS || (cell_dots % 2u) != 0u) {
        return false;
    }
    l->cell = (uint8_t)cell_dots;
    l->width = (uint16_t)(cell_dots * 33u / 2u);
    l->height = (uint16_t)(cell_dots * BK_ROWS);
    l->field_w = (uint16_t)(cell_dots * BK_COLS);
    l->gap = (uint8_t)((cell_dots + 4u) / 8u);
    l->ring = l->gap;
    l->panel_x = (uint16_t)(l->field_w + l->ring);
    l->panel_w = (uint16_t)(l->width - l->panel_x);
    l->label_scale = (uint8_t)(cell_dots / 8u);
    l->text_scale = (uint8_t)(cell_dots / 4u);
    return true;
}

bool bk_blink_phase(uint32_t tick)
{
    return ((tick / BK_BLINK_HALF_TICKS) & 1u) == 0u;
}

/* ---------------------------------------------------------------- cells */

/* One lit cell: outline, gap, core (classic LCD look) when the pitch allows; always a square. */
static void draw_cell(bk_frame_t *f, const bk_layout_t *l, int x, int y)
{
    int box = l->cell - l->gap;
    int t = l->ring;
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

static int odd_up(int v)
{
    return v | 1;
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
                draw_cell(out, l, col * P, row * P);
            }
        }
    }

    /* optional captions (menu: game title and mode label), centred in the playfield */
    if (s->caption1 != NULL) {
        bk_text_draw(out, (l->field_w - bk_text_width(s->caption1, l->label_scale)) / 2, D(105), l->label_scale,
                     s->caption1);
    }
    if (s->caption2 != NULL) {
        bk_text_draw(out, (l->field_w - bk_text_width(s->caption2, l->label_scale)) / 2, D(113), l->label_scale,
                     s->caption2);
    }

    /* divider between playfield and panel */
    bk_frame_fill_rect(out, l->field_w, 0, l->ring, l->height, true);

    /* status panel */
    const int pad = l->ring;
    const int x0 = l->panel_x + pad;
    const int inner_w = l->panel_w - 2 * pad;
    const int ls = l->label_scale;
    const int thick = l->ring;
    const int dgap = l->gap;
    const int digit_pitch = inner_w / 7;
    const int digit_w = digit_pitch - dgap;
    const int digit_h = odd_up(D(11));
    const int x_right = x0 + inner_w;

    bk_text_draw(out, x0, D(2), ls, "HI");
    bk_draw_number7(out, x_right, D(9), digit_w, digit_h, thick, dgap, s->hi_score, 7);

    if (s->show_score) {
        bk_text_draw(out, x0, D(26), ls, "SCORE");
        bk_draw_number7(out, x_right, D(33), digit_w, digit_h, thick, dgap, s->score, 7);
    }

    /* NEXT preview: 4 x 4 cells at the full cell pitch */
    bk_text_draw(out, x0, D(52), ls, "NEXT");
    if (s->has_preview) {
        int px = l->panel_x + (l->panel_w - BK_PREVIEW_COLS * P) / 2;
        int py = D(60);
        for (int r = 0; r < BK_PREVIEW_ROWS; r++) {
            for (int c = 0; c < BK_PREVIEW_COLS; c++) {
                uint8_t v = s->preview[r * BK_PREVIEW_COLS + c];
                if (v == BK_CELL_ON || (v == BK_CELL_BLINK && blink_on)) {
                    draw_cell(out, l, px + c * P, py + r * P);
                }
            }
        }
    }

    /* SPEED / LEVEL: single digits; the focused one blinks while the menu waits for input */
    bk_text_draw(out, x0, D(100), ls, "SPEED");
    if (s->focus != BK_FOCUS_SPEED || blink_on) {
        bk_draw_number7(out, x0 + digit_w, D(108), digit_w, digit_h, thick, dgap, s->speed, 1);
    }
    bk_text_draw(out, x0, D(124), ls, "LEVEL");
    if (s->focus != BK_FOCUS_LEVEL || blink_on) {
        bk_draw_number7(out, x0 + digit_w, D(132), digit_w, digit_h, thick, dgap, s->level, 1);
    }

    /* state icons */
    draw_bitmap(out, x0, D(148), ls, s->sound_on ? ICON_SOUND_ON : ICON_SOUND_OFF, 7);
    if (s->paused) {
        draw_bitmap(out, x0 + inner_w - 5 * ls, D(148), ls, ICON_PAUSE, 7);
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
