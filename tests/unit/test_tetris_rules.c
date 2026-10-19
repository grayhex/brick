/*
 * Rules of the base Tetris variant. Ported from the reference tests/brick/games/tetris.test.ts (same scenarios,
 * same numbers) so that the C port is checked against the *documented* rules as well as against recorded traces
 * (tests/conformance). The bot test of the reference is covered by the replayed bot traces instead.
 */
#include <string.h>

#include "bk_test.h"
#include "tetris_lab.h"

/* Catalogue indices (donor order Z, S, Line, T, Square, L, MirroredL). */
enum { Z = 0, S = 1, LINE = 2, T = 3, SQUARE = 4, L = 5, J = 6 };

#define FALL_1  27 /* first gravity step at speed 1: ceil(26.7 ticks) */
#define FALL_10 5

static void test_composition(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    CHECK_STR(bk_tetris_def.id, "tetris");
    CHECK_EQ(bk_tetris_def.mode_count, 1);
    CHECK_EQ(lab_g(&lab)->lives, -1);
    CHECK_EQ(lab_g(&lab)->speed, 1);
    CHECK_EQ(lab_g(&lab)->level, 1);
    uint8_t preview[BK_PREVIEW_CELLS];
    CHECK(bk_game_preview(lab_g(&lab), preview));
    CHECK_EQ(BK_CELLS, 200);
}

static void test_piece_catalogue(void)
{
    static const char *const names[BKT_SHAPE_COUNT] = {"Z", "S", "Line", "T", "Square", "L", "MirroredL"};
    for (int s = 0; s < BKT_SHAPE_COUNT; s++) {
        CHECK_STR(bkt_shape_name(s), names[s]);
    }
    /* four rotations return to the start; the square does not rotate */
    for (int s = 0; s < BKT_SHAPE_COUNT; s++) {
        bkt_piece_t start = bkt_piece_make(s, 0);
        bkt_piece_t p = start;
        for (int i = 0; i < 4; i++) {
            p = bkt_piece_rotate(&p);
        }
        CHECK(memcmp(p.cells, start.cells, sizeof p.cells) == 0);
        CHECK_EQ(p.angle, 0);
    }
    bkt_piece_t sq = bkt_piece_make(SQUARE, 0);
    bkt_piece_t sq2 = bkt_piece_rotate(&sq);
    CHECK(memcmp(&sq, &sq2, sizeof sq) == 0);
    bkt_piece_t line = bkt_piece_make(LINE, 0);
    bkt_piece_t line_r = bkt_piece_rotate(&line);
    CHECK_EQ(line.max_y - line.min_y, 3);
    CHECK_EQ(line_r.max_x - line_r.min_x, 3);
}

static void test_piece_alignment(void)
{
    /* After rotation the lowest row is on the anchor; for the line it is 0, +1, -1, +1 by angle. */
    static const int line_lowest[4] = {0, 1, -1, 1};
    for (int s = 0; s < BKT_SHAPE_COUNT; s++) {
        for (int a = 0; a < 4; a++) {
            bkt_piece_t p = bkt_piece_make(s, a);
            CHECK_EQ(p.min_y, s == LINE ? line_lowest[a] : 0);
            CHECK(p.min_x >= -2);
            CHECK(p.max_x <= 2);
            /* the spawn column math relies on the anchor never being negative (ADR-018, erase animation) */
            CHECK(p.min_x <= 0);
        }
    }
}

static void test_spawn_and_preview(void)
{
    for (int s = 0; s < BKT_SHAPE_COUNT; s++) {
        lab_t lab;
        lab_init_plain(&lab);
        lab_wipe(&lab);
        lab_respawn(&lab, s, 0);
        CHECK(lab.game.has_piece);
        CHECK_EQ(lab.game.piece_y + lab.game.piece.min_y, BK_ROWS - 1);
        char rows[BK_ROWS][BK_COLS + 1];
        lab_rows(&lab, rows);
        CHECK(strchr(rows[0], '#') != NULL);
        int lo = BK_COLS;
        int hi = -1;
        for (int x = 0; x < BK_COLS; x++) {
            if (rows[0][x] == '#') {
                lo = x < lo ? x : lo;
                hi = x > hi ? x : hi;
            }
        }
        CHECK(lo >= 3);
        CHECK(hi <= 6);
    }

    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_set_next(&lab, T, 0);
    char pv[BK_PREVIEW_ROWS][BK_PREVIEW_COLS + 1];
    lab_preview_rows(&lab, pv);
    CHECK_STR(pv[0], "....");
    CHECK_STR(pv[1], "..#.");
    CHECK_STR(pv[2], ".###");
    CHECK_STR(pv[3], "....");
    lab_respawn(&lab, T, 0);
    CHECK_EQ(lab.game.piece.shape, T);

    for (int s = 0; s < BKT_SHAPE_COUNT; s++) {
        lab_set_next(&lab, s, 0);
        lab_preview_rows(&lab, pv);
        int lit = 0;
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                lit += pv[r][c] == '#';
            }
        }
        CHECK_EQ(lit, 4);
    }
}

static void test_falling(void)
{
    static const unsigned speeds[2] = {1, 10};
    static const unsigned periods[2] = {FALL_1, FALL_10};
    for (int i = 0; i < 2; i++) {
        lab_t lab;
        bk_game_options_t o = lab_options(5, speeds[i], 1, 0);
        lab_init(&lab, &o);
        lab_wipe(&lab);
        lab_put(&lab, SQUARE, 0, 4, 12);
        lab_idle(&lab, periods[i] - 1);
        CHECK_EQ(lab.game.piece_y, 12);
        lab_idle(&lab, 1);
        CHECK_EQ(lab.game.piece_y, 11);
        lab_idle(&lab, periods[i]);
        CHECK_EQ(lab.game.piece_y, 10);
    }

    /* B0202 table: 445 -> 74 ms, fractional ticks are kept */
    double prev = 1e9;
    for (unsigned speed = 1; speed <= 10; speed++) {
        lab_t lab;
        bk_game_options_t o = lab_options(5, speed, 1, 0);
        lab_init(&lab, &o);
        double p = bkt_fall_ticks(&lab.game);
        if (speed == 1) {
            CHECK(p > 26.70 && p < 26.72); /* 150 * 2048 / 690000 * 60 */
        }
        if (speed == 10) {
            CHECK(p > 4.45 && p < 4.46); /* 25 * 2048 / 690000 * 60 */
        }
        CHECK(p <= prev);
        prev = p;
    }
}

static void test_landing(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, SQUARE, 0, 4, 3);
    lab_until_landed(&lab, 200);
    CHECK_EQ(lab_cell(&lab, 4, 0) + lab_cell(&lab, 5, 0) + lab_cell(&lab, 4, 1) + lab_cell(&lab, 5, 1), 4);
    CHECK(!lab.game.has_piece);
    int next_shape = lab.game.next_piece.shape;
    lab_idle(&lab, FALL_1);
    CHECK(lab.game.has_piece);
    CHECK_EQ(lab.game.piece.shape, next_shape);
    CHECK(!lab.game.falling_finished);

    /* stops on a ledge instead of passing through */
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_fill(&lab, 0, "....#.....");
    lab_put(&lab, SQUARE, 0, 4, 8);
    lab_until_landed(&lab, 400);
    CHECK_EQ(lab_cell(&lab, 4, 1), 1);
    CHECK_EQ(lab_cell(&lab, 5, 1), 1);
    CHECK_EQ(lab_cell(&lab, 4, 2), 1);
    CHECK_EQ(lab_cell(&lab, 5, 2), 1);
    CHECK_EQ(lab_count(&lab), 5);
}

static void test_keys(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, SQUARE, 0, 4, 12);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_LEFT);
    CHECK(lab_has(&lab, BK_EV_MOVE));
    CHECK_EQ(lab.game.piece_x, 3);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_RIGHT);
    CHECK(lab_has(&lab, BK_EV_MOVE));
    CHECK_EQ(lab.game.piece_x, 4);

    /* at a wall the piece stays and there is no move sound */
    lab_put(&lab, SQUARE, 0, 0, 12);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_LEFT);
    CHECK(!lab_has(&lab, BK_EV_MOVE));
    CHECK_EQ(lab.game.piece_x, 0);
    lab_put(&lab, SQUARE, 0, BK_COLS - 2, 12);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_RIGHT);
    CHECK(!lab_has(&lab, BK_EV_MOVE));
    CHECK_EQ(lab.game.piece_x, BK_COLS - 2);
}

static void test_move_autorepeat(void)
{
    /* first shift at once, after ~130 ms a shift every tick */
    static const unsigned ticks[5] = {1, 8, 9, 10, 11};
    static const int xs[5] = {3, 3, 2, 1, 0};
    for (int i = 0; i < 5; i++) {
        lab_t lab;
        lab_init_plain(&lab);
        lab_wipe(&lab);
        lab_put(&lab, SQUARE, 0, 4, 12);
        lab_hold(&lab, BK_KEY_LEFT, ticks[i]);
        CHECK_EQ(lab.game.piece_x, xs[i]);
    }
}

static void test_blocked_by_cells(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_fill(&lab, 5, "###.......");
    lab_put(&lab, SQUARE, 0, 3, 5);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_LEFT);
    CHECK_EQ(lab.log_count, 0);
    CHECK_EQ(lab.game.piece_x, 3);
}

static void test_rotate(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, T, 0, 4, 10);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_ROTATE);
    CHECK(lab_has(&lab, BK_EV_ROTATE));
    CHECK_EQ(lab.game.piece.angle, 1);
    lab_idle(&lab, 20);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_UP); /* UP is a rotation alias */
    CHECK(lab_has(&lab, BK_EV_ROTATE));
    CHECK_EQ(lab.game.piece.angle, 2);
    lab_put(&lab, SQUARE, 0, 4, 10);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_ROTATE);
    CHECK(!lab_has(&lab, BK_EV_ROTATE));
}

static void test_rotate_at_wall(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, LINE, 0, 0, 10);
    lab_tap(&lab, BK_KEY_ROTATE);
    CHECK_EQ(lab.game.piece.angle, 1);
    CHECK(lab.game.piece_x + lab.game.piece.min_x >= 0);
    CHECK(lab.game.piece_x + lab.game.piece.max_x < BK_COLS);

    /* a cell to the right blocks the horizontal line */
    lab_t blocked;
    lab_init_plain(&blocked);
    lab_wipe(&blocked);
    lab_put(&blocked, LINE, 0, 3, 10);
    lab_fill(&blocked, 11, ".....#....");
    lab_log_clear(&blocked);
    lab_tap(&blocked, BK_KEY_ROTATE);
    CHECK(!lab_has(&blocked, BK_EV_ROTATE));
    CHECK_EQ(blocked.game.piece.angle, 0);
}

static void test_soft_drop(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, SQUARE, 0, 4, 14);
    lab_hold(&lab, BK_KEY_DOWN, 7);
    CHECK_EQ(lab.game.piece_y, 14 - 7);
}

static void test_hard_drop(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, SQUARE, 0, 4, 12);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_FIRE);
    CHECK(lab_has(&lab, BK_EV_DROP));
    CHECK(lab_has(&lab, BK_EV_LOCK));
    CHECK(lab.game.falling_finished);
    CHECK_EQ(lab_count(&lab), 4);
    CHECK_EQ(lab_cell(&lab, 4, 0), 1);
}

static void test_keys_ignored_after_landing(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, SQUARE, 0, 4, 12);
    lab_tap(&lab, BK_KEY_FIRE);
    uint8_t before[BK_CELLS];
    memcpy(before, lab.game.board, sizeof before);
    lab_tap(&lab, BK_KEY_LEFT);
    lab_tap(&lab, BK_KEY_ROTATE);
    lab_tap(&lab, BK_KEY_FIRE);
    CHECK(memcmp(before, lab.game.board, sizeof before) == 0);
}

static void test_pause(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_put(&lab, SQUARE, 0, 4, 12);
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_pause(lab_g(&lab), &q);
    CHECK_EQ(q.count, 1);
    CHECK_EQ(q.items[0].type, BK_EV_PAUSED);
    uint8_t cells[BK_CELLS];
    uint8_t cells2[BK_CELLS];
    bk_game_cells(lab_g(&lab), cells);
    lab_idle(&lab, 200);
    lab_press(&lab, BK_KEY_LEFT);
    bk_game_cells(lab_g(&lab), cells2);
    CHECK(memcmp(cells, cells2, sizeof cells) == 0);
    bk_events_clear(&q);
    bk_game_resume(lab_g(&lab), &q);
    CHECK_EQ(q.items[0].type, BK_EV_RESUMED);
    lab_idle(&lab, FALL_1);
    CHECK_EQ(lab.game.piece_y, 11);
}

/* A well of `rows` rows into which a vertical line drops in column 0; rows without a hole in column 0 are full. */
static void well(lab_t *lab, int rows)
{
    lab_init_plain(lab);
    lab_wipe(lab);
    for (int y = 0; y < rows; y++) {
        lab_fill(lab, y, ".#########");
    }
    lab_put(lab, LINE, 0, 0, 10);
}

static void test_lines_and_points(void)
{
    static const int rows[4] = {1, 2, 3, 4};
    static const int points[4] = {1, 3, 7, 15};
    for (int i = 0; i < 4; i++) {
        lab_t lab;
        well(&lab, rows[i]);
        lab_log_clear(&lab);
        lab_tap(&lab, BK_KEY_FIRE);
        CHECK(lab_has(&lab, BK_EV_LINES));
        for (unsigned k = 0; k < lab.log_count; k++) {
            if (lab.log[k].type == BK_EV_LINES) {
                CHECK_EQ(lab.log[k].a, rows[i]);
            }
        }
        for (unsigned n = 0; n < 600 && lab_count(&lab) != 4 - rows[i]; n++) {
            lab_step(&lab, 0, 0);
        }
        CHECK_EQ(lab_g(&lab)->score, points[i]);
    }
}

static void test_row_collapse(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_fill(&lab, 0, "####..####");
    lab_fill(&lab, 1, "#.#.....#.");
    lab_put(&lab, SQUARE, 0, 4, 10);
    lab_tap(&lab, BK_KEY_FIRE);
    for (unsigned n = 0; n < 600; n++) {
        lab_step(&lab, 0, 0);
        char row0[BK_COLS + 1];
        for (int x = 0; x < BK_COLS; x++) {
            row0[x] = (char)('0' + lab.game.board[x]);
        }
        row0[BK_COLS] = '\0';
        if (strcmp(row0, "1010110010") == 0) {
            break;
        }
    }
    char row0[BK_COLS + 1];
    for (int x = 0; x < BK_COLS; x++) {
        row0[x] = (char)('0' + lab.game.board[x]);
    }
    row0[BK_COLS] = '\0';
    CHECK_STR(row0, "1010110010");
    CHECK_EQ(lab_g(&lab)->score, 1);
    int above = 0;
    for (int i = BK_COLS; i < BK_CELLS; i++) {
        above += lab.game.board[i];
    }
    CHECK_EQ(above, 0);
}

static void test_no_points_without_row(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_fill(&lab, 0, "##.......#");
    lab_put(&lab, SQUARE, 0, 4, 10);
    lab_tap(&lab, BK_KEY_FIRE);
    CHECK_EQ(lab_g(&lab)->score, 0);
    CHECK_EQ(lab_count(&lab), 7);
}

static void test_top_row_not_checked(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    lab_fill(&lab, BK_ROWS - 1, "##########");
    lab_put(&lab, SQUARE, 0, 4, 3);
    lab_tap(&lab, BK_KEY_FIRE);
    CHECK_EQ(lab_g(&lab)->score, 0);
}

static void test_speed_up(void)
{
    lab_t lab;
    well(&lab, 1);
    lab_poke(&lab, BKT_POINTS_PER_SPEED - 1);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_FIRE);
    CHECK(lab_has(&lab, BK_EV_LEVEL_UP));
    CHECK_EQ(lab_g(&lab)->speed, 2);
    CHECK_EQ(lab_g(&lab)->score, BKT_POINTS_PER_SPEED);

    /* B0202: the maximum speed stays; level and extra rows do not change */
    lab_t max;
    bk_game_options_t o = lab_options(5, 10, 1, 0);
    lab_init(&max, &o);
    lab_wipe(&max);
    lab_fill(&max, 0, ".#########");
    lab_put(&max, LINE, 0, 0, 10);
    lab_poke(&max, 999);
    lab_log_clear(&max);
    lab_tap(&max, BK_KEY_FIRE);
    CHECK(!lab_has(&max, BK_EV_LEVEL_UP));
    CHECK_EQ(lab_g(&max)->speed, 10);
    CHECK_EQ(lab_g(&max)->level, 1);
    lab_idle(&max, 60);
    CHECK_EQ(lab_count(&max), 3);
}

static void test_level_garbage(void)
{
    lab_t first;
    lab_init_plain(&first);
    CHECK_EQ(lab_count(&first), 0);
    static const unsigned levels[3] = {2, 5, 10};
    for (int i = 0; i < 3; i++) {
        lab_t lab;
        bk_game_options_t o = lab_options(11, 1, levels[i], 0);
        lab_init(&lab, &o);
        for (unsigned y = 0; y < levels[i] - 1; y++) {
            int filled = 0;
            for (int x = 0; x < BK_COLS; x++) {
                filled += lab.game.board[y * BK_COLS + x];
            }
            CHECK(filled > 0);
            CHECK(filled < BK_COLS);
        }
        for (int y = (int)levels[i] - 1; y < BK_ROWS; y++) {
            for (int x = 0; x < BK_COLS; x++) {
                CHECK_EQ(lab.game.board[y * BK_COLS + x], 0);
            }
        }
    }
}

static void test_game_over_by_spawn(void)
{
    lab_t lab;
    bk_game_options_t o = lab_options(5, 1, 1, 5);
    lab_init(&lab, &o);
    lab_wipe(&lab);
    lab_fill(&lab, BK_ROWS - 1, "#.#.#.#.#.");
    lab_poke(&lab, 42);
    lab_log_clear(&lab);
    lab_respawn(&lab, SQUARE, 0);
    lab_idle(&lab, 1);
    CHECK(lab_has(&lab, BK_EV_GAME_OVER));
    CHECK(lab.game.over);
    lab_until_phase(&lab, BK_PHASE_OVER, 200);
    CHECK_EQ(lab_g(&lab)->phase, BK_PHASE_OVER);
    CHECK_EQ(lab_g(&lab)->over_fill, BK_ROWS);
    CHECK_EQ(lab_g(&lab)->hi_score, 42);

    /* a new party is clean, the record stays */
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_start(lab_g(&lab), &q);
    CHECK_EQ(lab_g(&lab)->phase, BK_PHASE_PLAYING);
    CHECK_EQ(lab_g(&lab)->score, 0);
    CHECK_EQ(lab_g(&lab)->hi_score, 42);
    CHECK_EQ(lab_count(&lab), 0);
}

static void test_game_over_by_overhang(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_wipe(&lab);
    for (int y = 0; y < BK_ROWS - 2; y++) {
        lab_fill(&lab, y, "#.#.#.#.#.");
    }
    lab_put(&lab, LINE, 0, 0, BK_ROWS - 1);
    lab_log_clear(&lab);
    lab_tap(&lab, BK_KEY_FIRE);
    CHECK(lab_has(&lab, BK_EV_GAME_OVER));
}

static void test_idle_player_loses(void)
{
    lab_t lab;
    lab_init_plain(&lab);
    lab_until_phase(&lab, BK_PHASE_OVER, 20000);
    CHECK_EQ(lab_g(&lab)->phase, BK_PHASE_OVER);
    CHECK(lab.game.over);
}

int main(void)
{
    TEST_RUN(test_composition);
    TEST_RUN(test_piece_catalogue);
    TEST_RUN(test_piece_alignment);
    TEST_RUN(test_spawn_and_preview);
    TEST_RUN(test_falling);
    TEST_RUN(test_landing);
    TEST_RUN(test_keys);
    TEST_RUN(test_move_autorepeat);
    TEST_RUN(test_blocked_by_cells);
    TEST_RUN(test_rotate);
    TEST_RUN(test_rotate_at_wall);
    TEST_RUN(test_soft_drop);
    TEST_RUN(test_hard_drop);
    TEST_RUN(test_keys_ignored_after_landing);
    TEST_RUN(test_pause);
    TEST_RUN(test_lines_and_points);
    TEST_RUN(test_row_collapse);
    TEST_RUN(test_no_points_without_row);
    TEST_RUN(test_top_row_not_checked);
    TEST_RUN(test_speed_up);
    TEST_RUN(test_level_garbage);
    TEST_RUN(test_game_over_by_spawn);
    TEST_RUN(test_game_over_by_overhang);
    TEST_RUN(test_idle_player_loses);
    return bk_test_finish("tetris_rules");
}
