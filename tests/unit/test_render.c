/*
 * Compositor: geometry, strict monochrome, positive polarity, cell style, blink resolution, panel containment.
 */
#include <string.h>

#include "bk_test.h"
#include "brick/font.h"
#include "brick/render.h"
#include "frame_dump.h"

static bk_frame_t frame;
static bk_scene_t scene;

static unsigned ink_in(const bk_frame_t *f, int x0, int y0, int x1, int y1)
{
    unsigned n = 0;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            n += bk_frame_get(f, x, y) ? 1u : 0u;
        }
    }
    return n;
}

static void test_layout_geometry(void)
{
    bk_layout_t l;
    CHECK(!bk_layout_init(&l, 7)); /* odd */
    CHECK(!bk_layout_init(&l, 6)); /* too small */
    CHECK(!bk_layout_init(&l, BK_MAX_CELL_DOTS + 2));
    CHECK(bk_layout_init(&l, 8));
    CHECK_EQ(l.width, 132);
    CHECK_EQ(l.height, 160);
    CHECK_EQ(l.field_w, 80);

    for (unsigned p = BK_MIN_CELL_DOTS; p <= BK_MAX_CELL_DOTS; p += 2) {
        CHECK(bk_layout_init(&l, p));
        /* square cells: the playfield is exactly 10 cells wide and 20 cells tall at every size */
        CHECK_EQ(l.field_w / BK_COLS, p);
        CHECK_EQ(l.height / BK_ROWS, p);
        /* the whole screen is 16.5 cells wide (10 playfield + status panel to the RIGHT), never narrower than the field */
        CHECK_EQ(l.width * 2, p * 33);
        CHECK(l.panel_x >= l.field_w);
        CHECK(l.panel_w >= 6 * p);
        CHECK(l.width <= BK_FRAME_MAX_W && l.height <= BK_FRAME_MAX_H);
    }
}

static void test_positive_polarity_and_empty_field(void)
{
    bk_layout_t l;
    CHECK(bk_layout_init(&l, 8));
    memset(&scene, 0, sizeof scene);
    scene.sound_on = true;
    bk_render_scene(&l, &scene, &frame);
    /* an empty game shows an empty (light) playfield: ink is dark, the background is light */
    CHECK_EQ(ink_in(&frame, 0, 0, l.field_w, l.height), 0);
    /* a flooded field is mostly ink-rich but never solid ink: cells keep their gaps */
    scene.over_fill = BK_ROWS;
    bk_render_scene(&l, &scene, &frame);
    unsigned ink = ink_in(&frame, 0, 0, l.field_w, l.height);
    CHECK(ink > l.field_w * l.height / 4);
    CHECK(ink < l.field_w * l.height * 3 / 4);
}

static void test_cell_style(void)
{
    bk_layout_t l;
    CHECK(bk_layout_init(&l, 8));
    memset(&scene, 0, sizeof scene);
    scene.cells[0] = BK_CELL_ON;
    bk_render_scene(&l, &scene, &frame);
    /* 7x7 box in an 8-dot pitch: ring, 1-dot gap, 3x3 core; the last row/column is the gap between cells */
    CHECK(bk_frame_get(&frame, 0, 0));
    CHECK(bk_frame_get(&frame, 6, 6));
    CHECK(!bk_frame_get(&frame, 1, 1));
    CHECK(!bk_frame_get(&frame, 5, 5));
    CHECK(bk_frame_get(&frame, 2, 2));
    CHECK(bk_frame_get(&frame, 4, 4));
    CHECK(!bk_frame_get(&frame, 7, 3));
    CHECK(!bk_frame_get(&frame, 3, 7));
    CHECK_EQ(ink_in(&frame, 0, 0, 8, 8), 7 * 4 - 4 + 9); /* ring (24) + core (9) */
    /* the neighbouring cell stays empty */
    CHECK_EQ(ink_in(&frame, 8, 0, 16, 8), 0);
}

static void test_blink_resolution(void)
{
    bk_layout_t l;
    CHECK(bk_layout_init(&l, 8));
    memset(&scene, 0, sizeof scene);
    scene.cells[5] = BK_CELL_BLINK;
    unsigned on_ticks[3] = {0, BK_BLINK_HALF_TICKS * 2, BK_BLINK_HALF_TICKS * 4 + 3};
    for (int i = 0; i < 3; i++) {
        scene.tick = on_ticks[i];
        bk_render_scene(&l, &scene, &frame);
        CHECK(ink_in(&frame, 5 * 8, 0, 6 * 8, 8) > 0);
    }
    unsigned off_ticks[3] = {BK_BLINK_HALF_TICKS, BK_BLINK_HALF_TICKS * 3, BK_BLINK_HALF_TICKS * 3 + 14};
    for (int i = 0; i < 3; i++) {
        scene.tick = off_ticks[i];
        bk_render_scene(&l, &scene, &frame);
        CHECK_EQ(ink_in(&frame, 5 * 8, 0, 6 * 8, 8), 0);
    }
    /* the frame itself only ever has two levels: a bit is ink or not, by construction of bk_frame_t */
}

static void test_game_over_flood(void)
{
    bk_layout_t l;
    CHECK(bk_layout_init(&l, 8));
    memset(&scene, 0, sizeof scene);
    scene.over_fill = 3;
    bk_render_scene(&l, &scene, &frame);
    CHECK(ink_in(&frame, 0, 17 * 8, l.field_w, l.height) > 0);
    CHECK_EQ(ink_in(&frame, 0, 0, l.field_w, 17 * 8), 0);
}

static void test_panel_stays_in_panel(void)
{
    /* With an empty playfield, every ink dot must be in the divider or the status panel, for every design grid. */
    for (unsigned p = BK_MIN_CELL_DOTS; p <= BK_MAX_CELL_DOTS; p += 2) {
        bk_layout_t l;
        CHECK(bk_layout_init(&l, p));
        memset(&scene, 0, sizeof scene);
        scene.score = 9999999;
        scene.hi_score = 9999999;
        scene.show_score = true;
        scene.speed = 8;
        scene.level = 8;
        scene.sound_on = true;
        scene.paused = true;
        scene.has_preview = true;
        memset(scene.preview, BK_CELL_ON, sizeof scene.preview);
        bk_render_scene(&l, &scene, &frame);
        CHECK_EQ(ink_in(&frame, 0, 0, l.field_w, l.height), 0);
        /* the right edge is not touched by clipping: nothing is cut off at the frame boundary */
        CHECK(ink_in(&frame, l.width - 1, 0, l.width, l.height) < (unsigned)l.height);
        /* the 4x4 preview is fully inside the panel */
        CHECK(l.panel_x + (l.panel_w - 4 * p) / 2 >= l.panel_x);
        CHECK(l.panel_x + (l.panel_w + 4 * p) / 2 <= l.width);
    }
}

static void test_digits(void)
{
    bk_frame_init(&frame, 60, 20);
    bk_draw_number7(&frame, 56, 2, 6, 11, 1, 1, 8, 1);
    /* "8": all seven segments: a,d,g bars of 4, four verticals of 4 => 12 + 16 = 28 dots */
    CHECK_EQ(ink_in(&frame, 0, 0, 60, 20), 28);
    bk_frame_init(&frame, 60, 20);
    bk_draw_number7(&frame, 56, 2, 6, 11, 1, 1, 1, 1);
    CHECK_EQ(ink_in(&frame, 0, 0, 60, 20), 8); /* "1": two vertical segments */
    bk_frame_init(&frame, 60, 20);
    bk_draw_number7(&frame, 56, 2, 6, 11, 1, 1, 0, 3);
    CHECK_EQ(ink_in(&frame, 0, 0, 60, 20), 24); /* a single "0" and no leading zeros */
}

static void test_text(void)
{
    uint8_t rows[BK_GLYPH_H];
    CHECK(bk_font_glyph('A', rows));
    CHECK_EQ(rows[0], 2);            /* .#. */
    CHECK_EQ(rows[2], 7);            /* ### */
    CHECK(bk_font_glyph('a', rows)); /* folded to upper case */
    CHECK_EQ(rows[2], 7);
    CHECK(!bk_font_glyph('~', rows));
    CHECK_EQ(bk_text_width("AB", 1), 7);
    CHECK_EQ(bk_text_width("", 2), 0);
    CHECK_EQ(bk_text_width("AB", 2), 14);
    for (char c = '0'; c <= '9'; c++) {
        CHECK(bk_font_glyph(c, rows));
    }
    for (char c = 'A'; c <= 'Z'; c++) {
        CHECK(bk_font_glyph(c, rows) && bk_big_glyph(c) != NULL);
    }
    for (int d = 0; d < 10; d++) {
        CHECK(bk_small_digit(d) != NULL);
    }
    CHECK(bk_small_digit(10) == NULL);
}

static void test_text_page_fits(void)
{
    static const bk_text_row_t rows[] = {
        {"SOUND", "ON"}, {"LIGHT", "HIGH"}, {"RECORDS", "CLEAR"}, {"BUTTONS", "TEST"}, {"ABOUT", NULL}};
    for (unsigned p = BK_MIN_CELL_DOTS; p <= BK_MAX_CELL_DOTS; p += 2) {
        bk_layout_t l;
        CHECK(bk_layout_init(&l, p));
        bk_render_text_page(&l, "SETTINGS", rows, 5, 2, "BACK MODE", &frame);
        CHECK_EQ(frame.width, l.width);
        CHECK(ink_in(&frame, 0, 0, l.width, l.height) > 100);
        /* the value column is right-aligned inside the margin: the last dot column of the frame stays clear */
        CHECK_EQ(ink_in(&frame, l.width - 1, 0, l.width, l.height), 0);
    }
}

/* Native panel sizes from docs/DISPLAY_CANDIDATES.md (candidates only; no panel is approved). */
typedef struct {
    const char *name;
    unsigned w, h;
    unsigned cell;   /* expected dots per cell */
    unsigned status; /* expected status column width incl. divider */
} panel_t;

static const panel_t PANELS[] = {
    {"LS032B7DD02 3.16in portrait", 336, 536, 26, 76},
    {"LS044Q7DH01 4.4in rotated", 240, 320, 16, 80},
    {"LS027B7DH01 2.7in rotated", 240, 400, 20, 40},
};

static void test_fit_reference_identity(void)
{
    bk_layout_t a;
    bk_layout_t b;
    CHECK(bk_layout_init(&a, 8));
    CHECK(bk_layout_fit(&b, 132, 160));
    CHECK(memcmp(&a, &b, sizeof a) == 0); /* fitting to exactly the reference frame gives the reference layout */
}

static void test_fit_native_panels(void)
{
    for (unsigned i = 0; i < sizeof PANELS / sizeof PANELS[0]; i++) {
        const panel_t *p = &PANELS[i];
        bk_layout_t l;
        CHECK(bk_layout_fit(&l, p->w, p->h));
        CHECK_EQ(l.cell, p->cell);
        CHECK_EQ(l.field_w, p->cell * 10);                                           /* cells are square: 10 columns ... */
        CHECK_EQ(l.field_y * 2 + p->cell * 20, p->h - ((p->h - p->cell * 20) % 2u)); /* ... 20 rows, centred vertically */
        CHECK_EQ(l.width, p->w);
        CHECK_EQ(l.height, p->h);
        CHECK_EQ(l.panel_w + l.ring, p->status); /* the status column takes all the width that is left */
        CHECK(l.panel_w >= BK_MIN_STATUS_DOTS - l.ring);

        /* a full scene on an empty playfield: ink only in the divider and the status column, none cut off */
        memset(&scene, 0, sizeof scene);
        scene.score = scene.hi_score = 9999999;
        scene.show_score = true;
        scene.speed = scene.level = 8;
        scene.sound_on = scene.paused = scene.has_preview = true;
        memset(scene.preview, BK_CELL_ON, sizeof scene.preview);
        bk_render_scene(&l, &scene, &frame);
        CHECK_EQ(frame.width, p->w);
        CHECK_EQ(frame.height, p->h);
        CHECK_EQ(ink_in(&frame, 0, 0, l.field_w, l.height), 0);
        CHECK_EQ(ink_in(&frame, l.width - 1, 0, l.width, l.height), 0); /* right edge clear */
        CHECK(ink_in(&frame, l.panel_x, 0, l.width, l.height) > 0);

        /* the playfield starts below the top margin of the panel and keeps its cell pitch */
        memset(&scene, 0, sizeof scene);
        scene.cells[0] = BK_CELL_ON;
        scene.cells[BK_CELLS - 1] = BK_CELL_ON;
        bk_render_scene(&l, &scene, &frame);
        CHECK(bk_frame_get(&frame, 0, l.field_y));
        CHECK_EQ(ink_in(&frame, 0, 0, l.field_w, l.field_y), 0);
        CHECK(bk_frame_get(&frame, l.field_w - 1 - l.gap, l.field_y + l.cell * 20 - 1 - l.gap));

        /* text pages fit as well */
        static const bk_text_row_t rows[] = {
            {"SOUND", "ON"}, {"LIGHT", "HIGH"}, {"RECORDS", "CLEAR"}, {"BUTTONS", "TEST"}, {"ABOUT", NULL}};
        bk_render_text_page(&l, "SETTINGS", rows, 5, 1, "BACK MODE", &frame);
        CHECK(ink_in(&frame, 0, 0, l.width, l.height) > 100);
        CHECK_EQ(ink_in(&frame, l.width - 1, 0, l.width, l.height), 0);
    }
}

static void test_fit_rejects_what_does_not_fit(void)
{
    bk_layout_t l;
    CHECK(!bk_layout_fit(&l, 100, 400));                /* not even eight dots per cell leave room for the status column */
    CHECK(!bk_layout_fit(&l, 336, 100));                /* shorter than 20 cells of the minimum size */
    CHECK(!bk_layout_fit(&l, BK_FRAME_MAX_W + 1, 536)); /* larger than the frame buffer */
    CHECK(bk_layout_fit(&l, 200, 400));                 /* narrower panel: a smaller cell, more vertical margin */
    CHECK(l.cell < 20 && l.field_y > 0);
    CHECK_EQ(l.field_w % 10u, 0);
    CHECK_EQ(l.field_w / 10u, l.cell);
}

static void test_narrow_status_column_stays_legible(void)
{
    /* On the narrowest accepted column seven digits still have a positive width and the preview shrinks to fit. */
    bk_layout_t l;
    CHECK(bk_layout_fit(&l, 240, 400));
    CHECK(l.digit_w >= 3 && l.digit_h >= 5 && (l.digit_h % 2u) == 1u);
    CHECK(l.preview_cell > 0 && l.preview_cell < l.cell);
    CHECK(l.preview_cell * 4 <= l.panel_w);
    CHECK(7 * (l.digit_w + l.digit_gap) <= l.panel_w - 2 * l.ring + l.digit_gap);
    /* wide columns keep the preview at the full cell pitch */
    CHECK(bk_layout_init(&l, 8));
    CHECK_EQ(l.preview_cell, 8);
}

static void test_frame_ops(void)
{
    bk_frame_init(&frame, 10, 4);
    CHECK_EQ(frame.stride, 2);
    bk_frame_set(&frame, 9, 3, true);
    CHECK(bk_frame_get(&frame, 9, 3));
    CHECK(!bk_frame_get(&frame, 8, 3));
    CHECK(!bk_frame_get(&frame, 10, 3)); /* outside reads as light */
    bk_frame_set(&frame, -1, 0, true);   /* clipped, no effect and no crash */
    bk_frame_set(&frame, 0, 99, true);
    CHECK_EQ(ink_in(&frame, 0, 0, 10, 4), 1);
    bk_frame_set(&frame, 9, 3, false);
    CHECK_EQ(ink_in(&frame, 0, 0, 10, 4), 0);
    bk_frame_fill_rect(&frame, -3, -3, 6, 6, true); /* clipped rectangle */
    CHECK_EQ(ink_in(&frame, 0, 0, 10, 4), 3 * 3);
    bk_frame_t other;
    bk_frame_init(&other, 10, 4);
    CHECK(!bk_frame_equal(&frame, &other));
    bk_frame_clear(&frame);
    CHECK(bk_frame_equal(&frame, &other));
    char text[64];
    bk_frame_to_ascii(&other, text, sizeof text);
    CHECK_STR(text, "..........\n..........\n..........\n..........\n");
}

int main(void)
{
    TEST_RUN(test_layout_geometry);
    TEST_RUN(test_positive_polarity_and_empty_field);
    TEST_RUN(test_cell_style);
    TEST_RUN(test_blink_resolution);
    TEST_RUN(test_game_over_flood);
    TEST_RUN(test_panel_stays_in_panel);
    TEST_RUN(test_digits);
    TEST_RUN(test_text);
    TEST_RUN(test_text_page_fits);
    TEST_RUN(test_fit_reference_identity);
    TEST_RUN(test_fit_native_panels);
    TEST_RUN(test_fit_rejects_what_does_not_fit);
    TEST_RUN(test_narrow_status_column_stays_legible);
    TEST_RUN(test_frame_ops);
    return bk_test_finish("render");
}
