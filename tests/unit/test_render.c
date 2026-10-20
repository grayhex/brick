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
        scene.sound_on = false; /* every status element at once, the muted speaker included */
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

/* Layouts to run the scene checks on: every design grid and the three native panels. */
static unsigned layouts_for_scenes(bk_layout_t out[16])
{
    unsigned n = 0;
    for (unsigned p = BK_MIN_CELL_DOTS; p <= BK_MAX_CELL_DOTS; p += 2) {
        CHECK(bk_layout_init(&out[n++], p));
    }
    static const struct {
        unsigned w, h;
    } panels[] = {{336, 536}, {240, 320}, {240, 400}};
    for (unsigned i = 0; i < 3; i++) {
        CHECK(bk_layout_fit(&out[n++], panels[i].w, panels[i].h));
    }
    return n;
}

static void test_pause_box_over_the_playfield(void)
{
    bk_layout_t layouts[16];
    unsigned n = layouts_for_scenes(layouts);
    for (unsigned i = 0; i < n; i++) {
        const bk_layout_t *l = &layouts[i];
        const int c = l->cell;
        const int y = l->field_y;
        memset(&scene, 0, sizeof scene);
        scene.sound_on = true;
        memset(scene.cells, BK_CELL_ON, sizeof scene.cells); /* a full field: everything under the box must be hidden */
        bk_render_scene(l, &scene, &frame);
        static bk_frame_t paused;
        scene.paused = true;
        bk_render_scene(l, &scene, &paused);

        /* the box is 6 x 4 cells in the middle (columns 2..7, rows 8..11): its inside is clear except for the two bars */
        const unsigned inside_before = ink_in(&frame, 2 * c, y + 8 * c, 8 * c, y + 12 * c);
        const unsigned inside_after = ink_in(&paused, 2 * c, y + 8 * c, 8 * c, y + 12 * c);
        CHECK(inside_after < inside_before / 2);
        CHECK(ink_in(&paused, 5 * c - c / 2, y + 9 * c, 5 * c + c / 2, y + 11 * c) > 0); /* the two bars of the sign */
        CHECK_EQ(ink_in(&paused, 3 * c, y + 8 * c + l->ring + 1, 5 * c - c, y + 12 * c - l->ring - 1),
                 0); /* clear left of them */
        /* outside the box the field is untouched, and so is the status column */
        CHECK_EQ(ink_in(&paused, 0, 0, l->field_w, y + 8 * c) - ink_in(&frame, 0, 0, l->field_w, y + 8 * c), 0);
        CHECK_EQ(
            ink_in(&paused, 0, y + 12 * c, l->field_w, l->height) - ink_in(&frame, 0, y + 12 * c, l->field_w, l->height), 0);
        CHECK_EQ(ink_in(&paused, l->field_w, 0, l->width, l->height) - ink_in(&frame, l->field_w, 0, l->width, l->height),
                 0);
        /* the sign is two equal bars, half a cell wide and two cells tall, a half cell apart in the middle of the box */
        const int bar_w = c / 2;
        const int bar_gap = c / 2;
        const int by = y + 9 * c;
        const unsigned left_bar = ink_in(&paused, 5 * c - bar_gap / 2 - bar_w, by, 5 * c - bar_gap / 2, by + 2 * c);
        const unsigned right_bar = ink_in(&paused, 5 * c + bar_gap / 2, by, 5 * c + bar_gap / 2 + bar_w, by + 2 * c);
        CHECK_EQ(left_bar, bar_w * 2 * c);
        CHECK_EQ(right_bar, bar_w * 2 * c);
    }
}

static void test_the_menu_panel_is_hi_spd_lvl(void)
{
    bk_layout_t layouts[16];
    unsigned n = layouts_for_scenes(layouts);
    for (unsigned i = 0; i < n; i++) {
        const bk_layout_t *l = &layouts[i];
        memset(&scene, 0, sizeof scene);
        scene.sound_on = true;
        scene.hi_score = 12345;
        scene.speed = 3;
        scene.level = 1;
        bk_render_scene(l, &scene, &frame); /* show_score is false: the menu */
        static bk_frame_t game;
        scene.show_score = true;
        scene.score = 590;
        scene.has_preview = true;
        scene.preview[0] = BK_CELL_ON;
        bk_render_scene(l, &scene, &game);

        const int y_game_speed = l->field_y + l->cell * 100 / 8; /* where the game panel has SPD and LVL */
        const int y_menu_end = l->field_y + l->cell * 80 / 8;    /* the menu panel is done by here: HI, SPD, LVL */
        CHECK(ink_in(&frame, l->panel_x, l->field_y, l->width, y_menu_end) > 0);
        CHECK_EQ(ink_in(&frame, l->panel_x, y_menu_end, l->width, l->height), 0); /* nothing below, no icon either */
        CHECK(ink_in(&game, l->panel_x, y_game_speed, l->width, l->height) > 0);
        /* both panels have the record in the first slot, the same picture there */
        const int y_hi_end = l->field_y + l->cell * 24 / 8;
        for (int y = l->field_y; y < y_hi_end; y++) {
            for (int x = l->panel_x; x < l->width; x++) {
                CHECK(bk_frame_get(&frame, x, y) == bk_frame_get(&game, x, y));
            }
        }
    }
}

/* SPD and LVL are always two digits, with the leading zero, as in the mock-up: "03", "01", "10". */
static void test_spd_and_lvl_are_always_two_digits(void)
{
    bk_layout_t layouts[16];
    unsigned n = layouts_for_scenes(layouts);
    static const struct {
        unsigned speed, level;
    } values[] = {{3, 1}, {1, 10}, {10, 7}, {88, 88}};
    for (unsigned i = 0; i < n; i++) {
        const bk_layout_t *l = &layouts[i];
        const int x0 = l->panel_x + l->ring;
        const int dw = l->digit_w;
        const int dh = l->digit_h;
        const int width = 2 * dw + l->digit_gap;
        for (unsigned v = 0; v < sizeof values / sizeof values[0]; v++) {
            memset(&scene, 0, sizeof scene);
            scene.sound_on = true;
            scene.speed = (uint8_t)values[v].speed;
            scene.level = (uint8_t)values[v].level;
            bk_render_scene(l, &scene, &frame); /* the menu panel: SPD in the second slot, LVL in the third */
            static bk_frame_t want;
            bk_frame_init(&want, l->width, l->height);
            const unsigned shown[2] = {values[v].speed, values[v].level};
            const int y[2] = {l->field_y + l->cell * (26 + 8) / 8, l->field_y + l->cell * (52 + 8) / 8};
            for (int k = 0; k < 2; k++) {
                bk_draw_number7(&want, x0 + dw, y[k], dw, dh, l->digit_t, l->digit_gap, shown[k] / 10u, 1);
                bk_draw_number7(&want, x0 + width, y[k], dw, dh, l->digit_t, l->digit_gap, shown[k] % 10u, 1);
                for (int yy = y[k]; yy < y[k] + dh; yy++) {
                    for (int xx = x0; xx < x0 + width; xx++) {
                        CHECK(bk_frame_get(&frame, xx, yy) == bk_frame_get(&want, xx, yy));
                    }
                }
                CHECK(ink_in(&frame, x0, y[k], x0 + dw, y[k] + dh) > 0); /* the tens digit is there, "0" included */
            }
        }
        /* and nothing wider than two digits fits the narrowest column */
        CHECK(width <= l->panel_w - 2 * l->ring);
    }
}

static void test_life_label_replaces_next_and_the_speaker_shows_only_when_muted(void)
{
    bk_layout_t l;
    CHECK(bk_layout_init(&l, 8));
    memset(&scene, 0, sizeof scene);
    scene.show_score = true;
    scene.sound_on = true;
    scene.has_preview = true;
    scene.preview[5] = BK_CELL_ON;
    static bk_frame_t next_frame;
    static bk_frame_t life_frame;
    bk_render_scene(&l, &scene, &next_frame);
    scene.preview_is_lives = true;
    bk_render_scene(&l, &scene, &life_frame);
    CHECK(!bk_frame_equal(&next_frame, &life_frame));
    /* only the label differs: it is five dots tall on the reference grid, at slot 52 */
    CHECK_EQ(ink_in(&life_frame, l.panel_x, 0, l.width, 52) - ink_in(&next_frame, l.panel_x, 0, l.width, 52), 0);
    CHECK_EQ(ink_in(&life_frame, l.panel_x, 58, l.width, l.height) - ink_in(&next_frame, l.panel_x, 58, l.width, l.height),
             0);
    scene.has_preview = false; /* no box: no label either (a bare "NEXT" over nothing is noise) */
    bk_render_scene(&l, &scene, &life_frame);
    CHECK_EQ(ink_in(&life_frame, l.panel_x, 50, l.width, 98), 0);

    /* the speaker icon is the muted state only: with the sound on, the bottom of the column is empty */
    CHECK_EQ(ink_in(&life_frame, l.panel_x, 147, l.width, l.height), 0);
    scene.sound_on = false;
    bk_render_scene(&l, &scene, &life_frame);
    CHECK(ink_in(&life_frame, l.panel_x, 147, l.width, l.height) > 0);
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

/* The widest texts the console puts on its pages (docs/UI_MENU.md section 5). */
static const bk_text_row_t LIST_ROWS[] = {{"SOUND", "ON"},     {"LIGHT", "HIGH"},      {"BLE", "OFF"},
                                          {"DEMO", "ON"},      {"AUTO OFF", "10 MIN"}, {"RECORDS", "SURE?"},
                                          {"BUTTONS", "TEST"}, {"ABOUT", "INFO"}};
static const bk_text_row_t LINES_ROWS[] = {{"UP", "--"},     {"DOWN", "ON"}, {"LEFT", "--"},   {"RIGHT", "--"},
                                           {"ACTION", "ON"}, {"MODE", "--"}, {"SELECT", "ON"}, {"START", "--"}};
static const bk_text_row_t ABOUT_ROWS[] = {
    {"BRICK", NULL}, {"0.1.0-PROTO", NULL}, {"CELL 16 DOTS", NULL}, {"PROTOTYPE", NULL}, {"NO HW APPROVED", NULL}};

static void make_page(bk_page_t *p, bk_page_style_t style)
{
    memset(p, 0, sizeof *p);
    p->style = style;
    p->cursor = -1;
    p->sound_on = true;
    p->status[0].label = "ITEM";
    p->status[0].value = "8";
    p->status[1].label = "LIGHT";
    p->status[1].value = "HIGH";
    p->status[2].label = "BLE";
    p->status[2].value = "OFF";
    if (style == BK_PAGE_LIST) {
        p->title = "SETTINGS";
        p->rows = LIST_ROWS;
        p->count = BK_ARRAY_LEN(LIST_ROWS);
    } else {
        p->title = "BUTTONS";
        p->rows = LINES_ROWS;
        p->count = BK_ARRAY_LEN(LINES_ROWS);
        p->footer[0] = "EXIT";
        p->footer[1] = "MODE+SELECT+START";
    }
}

/* Every page on every grid and native panel: ink only inside its own area, nothing touching the divider or the frame edge.
 */
static void check_page_fits(const bk_layout_t *l)
{
    bk_page_t p;
    for (int kind = 0; kind < 3; kind++) {
        make_page(&p, kind == 0 ? BK_PAGE_LIST : BK_PAGE_LINES);
        if (kind == 2) {
            p.title = "ABOUT";
            p.rows = ABOUT_ROWS;
            p.count = BK_ARRAY_LEN(ABOUT_ROWS);
            p.footer[0] = "BACK MODE";
            p.footer[1] = NULL;
        }
        p.cursor = kind == 0 ? 4 : -1; /* the widest pair boxed */
        p.top = kind == 0 ? 3 : 0;     /* scrolled: both arrows are shown */
        bk_render_page(l, &p, &frame);
        CHECK_EQ(frame.width, l->width);
        CHECK_EQ(frame.height, l->height);
        CHECK(ink_in(&frame, 0, 0, l->field_w, l->height) > 100);
        /* the text keeps clear of the divider and of the left edge, the top and the bottom of the matrix area */
        CHECK_EQ(ink_in(&frame, l->field_w - l->cell / 3, 0, l->field_w, l->height), 0);
        CHECK_EQ(ink_in(&frame, 0, 0, 1, l->height), 0);
        CHECK_EQ(ink_in(&frame, 0, 0, l->field_w, l->field_y), 0);
        CHECK_EQ(ink_in(&frame, 0, l->field_y + l->cell * BK_ROWS - 2, l->field_w, l->height), 0);
        CHECK_EQ(ink_in(&frame, l->width - 1, 0, l->width, l->height), 0);
        /* the divider and the status column are there */
        CHECK(bk_frame_get(&frame, l->field_w, l->height / 2));
        CHECK(ink_in(&frame, l->panel_x, 0, l->width, l->height) > 40);
    }
}

static void test_text_pages_fit(void)
{
    for (unsigned p = BK_MIN_CELL_DOTS; p <= BK_MAX_CELL_DOTS; p += 2) {
        bk_layout_t l;
        CHECK(bk_layout_init(&l, p));
        check_page_fits(&l);
    }
}

static void test_list_page_shows_five_rows_and_scrolls(void)
{
    static const struct {
        unsigned w, h;
    } sizes[] = {{132, 160}, {336, 536}, {240, 320}, {240, 400}, {200, 400}};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        bk_layout_t l;
        CHECK(bk_layout_fit(&l, sizes[i].w, sizes[i].h));
        CHECK_EQ(bk_page_rows_visible(&l), 5); /* the same list on every panel, as in the mock-up */

        bk_page_t p;
        make_page(&p, BK_PAGE_LIST);
        static bk_frame_t other;
        const int row_h = l.cell * 22 / 8; /* pitch of the pairs */
        const int y0 = l.field_y + l.cell * 32 / 8;

        /* the box follows the cursor and only the cursor row changes */
        p.cursor = 1;
        bk_render_page(&l, &p, &frame);
        const unsigned box1 = ink_in(&frame, l.cell * 4 / 8, y0 + row_h - 8, l.cell * 4 / 8 + 1, y0 + row_h + 4);
        CHECK(box1 > 0); /* the left edge of the box of row 1 */
        p.cursor = 2;
        bk_render_page(&l, &p, &other);
        CHECK(!bk_frame_equal(&frame, &other));
        CHECK_EQ(ink_in(&other, l.cell * 4 / 8, y0 + row_h - 8, l.cell * 4 / 8 + 1, y0 + row_h + 4), 0);
        /* nothing is boxed without a cursor: fewer dots than with one */
        p.cursor = -1;
        bk_render_page(&l, &p, &other);
        CHECK(ink_in(&other, 0, 0, l.field_w, l.height) < ink_in(&frame, 0, 0, l.field_w, l.height));

        /* scrolled by three rows the first visible row is the fourth one, and an arrow shows that the list goes on above */
        p.cursor = 4;
        p.top = 0;
        bk_render_page(&l, &p, &frame);
        p.top = 3;
        bk_render_page(&l, &p, &other);
        CHECK(!bk_frame_equal(&frame, &other));
        const unsigned arrow_up =
            ink_in(&other, l.field_w - l.cell - l.cell / 2, y0 - l.cell, l.field_w - l.cell / 2, y0 - 1);
        CHECK(arrow_up > 0);
        CHECK_EQ(ink_in(&frame, l.field_w - l.cell - l.cell / 2, y0 - l.cell, l.field_w - l.cell / 2, y0 - 1), 0);
    }
}

static void test_page_status_slots_keep_their_places(void)
{
    bk_layout_t l;
    CHECK(bk_layout_init(&l, 8));
    bk_page_t p;
    make_page(&p, BK_PAGE_LINES);
    bk_render_page(&l, &p, &frame);
    const unsigned all = ink_in(&frame, l.panel_x, 0, l.width, l.height);
    const unsigned top_slot = ink_in(&frame, l.panel_x, 0, l.width, 24);
    CHECK(top_slot > 0);
    p.status[0].label = NULL; /* an empty slot leaves the other two where they were */
    p.status[0].value = NULL;
    bk_render_page(&l, &p, &frame);
    CHECK_EQ(ink_in(&frame, l.panel_x, 0, l.width, 24), 0);
    CHECK_EQ(ink_in(&frame, l.panel_x, 24, l.width, l.height), all - top_slot);
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
        scene.sound_on = false;
        scene.has_preview = true;
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
        check_page_fits(&l);
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
    TEST_RUN(test_pause_box_over_the_playfield);
    TEST_RUN(test_the_menu_panel_is_hi_spd_lvl);
    TEST_RUN(test_spd_and_lvl_are_always_two_digits);
    TEST_RUN(test_life_label_replaces_next_and_the_speaker_shows_only_when_muted);
    TEST_RUN(test_digits);
    TEST_RUN(test_text);
    TEST_RUN(test_text_pages_fit);
    TEST_RUN(test_list_page_shows_five_rows_and_scrolls);
    TEST_RUN(test_page_status_slots_keep_their_places);
    TEST_RUN(test_fit_reference_identity);
    TEST_RUN(test_fit_native_panels);
    TEST_RUN(test_fit_rejects_what_does_not_fit);
    TEST_RUN(test_narrow_status_column_stays_legible);
    TEST_RUN(test_frame_ops);
    return bk_test_finish("render");
}
