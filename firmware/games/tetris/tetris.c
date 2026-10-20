/*
 * Tetris, base variant: port of ApolloBrickGames src/brick/games/tetris/core.ts (class TetrisCore) restricted to
 * the plain rules (donor class I, type 1): seven tetrominoes, clockwise rotation, line clearing 1/3/7/15, speed
 * rising every 1000 points, start level N = N-1 random rows at the bottom.
 *
 * Differences from the reference are structural only:
 *  - closures of cutscenes (erase animation) are an explicit state machine (scene kind + cl_* fields);
 *  - the reference precomputes animation frames, here a frame is derived from the elapsed ticks on demand;
 *  - cells live in fixed arrays inside the game struct.
 * Behaviour (cells, events, score, timing) is verified tick by tick against the reference, see
 * tests/conformance and docs/CONFORMANCE.md.
 *
 * Inside the game the field is stored like the donor does: board[y * 10 + x], y = 0 is the BOTTOM of the well.
 * Screen rows (cells()) are 19 - y. Pieces appear with their lowest row in the top row of the field and the rest
 * above the screen, then "slide" into the field while falling.
 */
#include "brick/games/tetris.h"

#include <math.h>
#include <string.h>

#include "tetris_internal.h"

#define MIDDLE_X (BK_COLS / 2 - 1)

/* Apollo B0202 timer table: fall time = counts * 2048 / 690000 s (reference b0202.ts). */
static const uint8_t B0202_TETRIS_COUNTS[10] = {150, 120, 100, 82, 60, 41, 35, 31, 27, 25};
/* Points for the 1st, 2nd, 3rd and 4th row of one landing: 1, 2, 4, 8 (i.e. 1, 3, 7, 15 for 1..4 rows). */
static const uint8_t LINE_POINTS[5] = {0, 1, 2, 4, 8};

static inline int cell_index(int x, int y)
{
    return y * BK_COLS + x;
}

static inline bkt_game_t *self(bk_game_t *g)
{
    return (bkt_game_t *)g;
}

static inline const bkt_game_t *cself(const bk_game_t *g)
{
    return (const bkt_game_t *)g;
}

/* ---------------------------------------------------------------- timing */

double bkt_fall_ticks(const bkt_game_t *t)
{
    int i = (int)t->base.speed - 1;
    i = i < 0 ? 0 : (i > 9 ? 9 : i);
    /* Same operation order as the reference so the result is bit-identical (compiled with -ffp-contract=off). */
    double ms = (double)((int64_t)B0202_TETRIS_COUNTS[i] * 2048 * 1000) / 690000.0;
    return (ms * BK_TICK_HZ) / 1000.0;
}

/* ---------------------------------------------------------------- field access and drawing */

static int cell_at(const bkt_game_t *t, int x, int y)
{
    if (x < 0 || x >= BK_COLS || y < 0 || y >= BK_ROWS) {
        return 0;
    }
    return t->board[cell_index(x, y)];
}

/* Screen picture: settled cells of `board` (bottom-up in memory, top-down on screen) plus optionally the falling piece. */
static void render(const bkt_game_t *t, const uint8_t board[BK_CELLS], bool with_piece, uint8_t out[BK_CELLS])
{
    memset(out, 0, BK_CELLS);
    for (int y = 0; y < BK_ROWS; y++) {
        int row = BK_ROWS - 1 - y;
        for (int x = 0; x < BK_COLS; x++) {
            uint8_t v = board[cell_index(x, y)];
            if (v != 0) {
                out[cell_index(x, row)] = v;
            }
        }
    }
    if (with_piece && t->has_piece) {
        for (int i = 0; i < BKT_PIECE_CELLS; i++) {
            int x = t->piece_x + t->piece.cells[i].x;
            int y = t->piece_y + t->piece.cells[i].y;
            if (x < 0 || x >= BK_COLS || y < 0 || y >= BK_ROWS) {
                continue;
            }
            out[cell_index(x, BK_ROWS - 1 - y)] = BK_CELL_ON;
        }
    }
}

static void tetris_draw(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    const bkt_game_t *t = cself(g);
    render(t, t->board, true, cells);
}

/* NEXT preview: centred in 4 x 4 (formulas of newPiece in the reference). */
static bool tetris_draw_preview(const bk_game_t *g, uint8_t cells[BK_PREVIEW_CELLS])
{
    const bkt_game_t *t = cself(g);
    const bkt_piece_t *p = &t->next_piece;
    int ox = BK_PREVIEW_COLS / 2 - (p->max_x - p->min_x + 1) / 2 - p->min_x;
    int oy = BK_PREVIEW_ROWS / 2 - (p->max_y - p->min_y + 1) / 2 - p->min_y;
    memset(cells, 0, BK_PREVIEW_CELLS);
    for (int i = 0; i < BKT_PIECE_CELLS; i++) {
        int x = ox + p->cells[i].x;
        int y = oy + p->cells[i].y;
        if (x < 0 || x >= BK_PREVIEW_COLS || y < 0 || y >= BK_PREVIEW_ROWS) {
            continue;
        }
        cells[(BK_PREVIEW_ROWS - 1 - y) * BK_PREVIEW_COLS + x] = BK_CELL_ON;
    }
    return true;
}

/* ---------------------------------------------------------------- pieces and party start */

/* Random garbage row of the donor (addLinesToBoard): about half of the cells set, but never full and never empty. */
static void write_garbage_row(bkt_game_t *t, int y)
{
    bool filled[BK_COLS];
    bool has_empty = false;
    bool has_full = false;
    for (int x = 0; x < BK_COLS; x++) {
        bool empty = bk_rng_chance(&t->base.rng, 1, 2);
        filled[x] = !empty;
        if (empty) {
            has_empty = true;
        } else {
            has_full = true;
        }
    }
    if (!has_empty || !has_full) {
        filled[bk_rng_int(&t->base.rng, BK_COLS)] = !has_full;
    }
    for (int x = 0; x < BK_COLS; x++) {
        t->board[cell_index(x, y)] = filled[x] ? BK_CELL_ON : 0;
    }
}

/* randomPiece: a uniformly random tetromino at a random angle (two RNG draws, shape first). */
static bkt_piece_t random_piece(bkt_game_t *t)
{
    int shape = (int)bk_rng_int(&t->base.rng, BKT_SHAPE_COUNT);
    int angle = (int)bk_rng_int(&t->base.rng, 4);
    return bkt_piece_make(shape, angle);
}

static bool hits(const bkt_game_t *t, const bkt_piece_t *p, int x, int y)
{
    for (int i = 0; i < BKT_PIECE_CELLS; i++) {
        if (cell_at(t, x + p->cells[i].x, y + p->cells[i].y) != 0) {
            return true;
        }
    }
    return false;
}

/*
 * tryMove of the reference. A piece sticking out of a side wall is pushed back into the field (this is how
 * rotating and moving into a wall succeed without moving). The ceiling is not checked: pieces spawn above the screen.
 */
static bool try_move(bkt_game_t *t, const bkt_piece_t *next, int new_x, int new_y)
{
    int x = new_x;
    while (x + next->min_x < 0 || x + next->max_x >= BK_COLS) {
        x = (x + next->min_x < 0) ? x + 1 : x - 1;
    }
    if (new_y + next->min_y < 0) {
        return false;
    }
    if (hits(t, next, x, new_y)) {
        return false;
    }
    t->piece = *next;
    t->has_piece = true;
    t->piece_x = x;
    t->piece_y = new_y;
    return true;
}

void bkt_spawn(bkt_game_t *t)
{
    t->has_piece = false;
    bkt_piece_t next = t->next_piece;
    if (!try_move(t, &next, MIDDLE_X, BK_ROWS - 1 - next.min_y)) {
        t->over = true;
        bk_game_game_over(&t->base);
        return;
    }
    t->next_piece = random_piece(t); /* pickNext for the plain family */
}

static void tetris_reset(bk_game_t *g)
{
    bkt_game_t *t = self(g);
    memset(t->board, 0, sizeof t->board);
    t->has_piece = false;
    t->piece_x = 0;
    t->piece_y = 0;
    t->falling_finished = false;
    t->last_fall = 0.0;
    t->over = false;
    memset(t->move_cnt, 0, sizeof t->move_cnt);
    t->down_cnt = 0;
    t->cl_anchor = t->cl_removed = t->cl_y = 0;
    t->land_y = t->land_max_y = 0;
    /* the level is the number of random rows at the start (none on the first level) */
    for (int y = 0; y < (int)g->start_level - 1; y++) {
        write_garbage_row(t, y);
    }
    t->next_piece = random_piece(t);
    bkt_spawn(t);
}

/* ---------------------------------------------------------------- landing, lines, score */

static void add_points(bkt_game_t *t, uint32_t points)
{
    bk_game_t *g = &t->base;
    uint32_t before = g->score / BKT_POINTS_PER_SPEED;
    bk_game_add_score(g, points);
    uint32_t steps = g->score / BKT_POINTS_PER_SPEED - before;
    /* B0202: a carry into the thousands raises the speed; the maximum speed stays. */
    if (steps == 0 || g->speed == BK_MAX_SPEED) {
        return;
    }
    uint32_t speed = g->speed + steps;
    bk_game_set_speed(g, (int)(speed > BK_MAX_SPEED ? BK_MAX_SPEED : speed));
    bk_game_emit(g, BK_EV_LEVEL_UP, g->speed, 0, 0);
}

static bool row_full(const bkt_game_t *t, int y)
{
    for (int x = 0; x < BK_COLS; x++) {
        if (cell_at(t, x, y) == 0) {
            return false;
        }
    }
    return true;
}

/* Remove row y: everything above drops by one, an empty row appears on top. */
static void drop_rows_above(bkt_game_t *t, int y)
{
    memmove(&t->board[cell_index(0, y)], &t->board[cell_index(0, y + 1)], (size_t)(BK_CELLS - cell_index(0, y + 1)));
    memset(&t->board[cell_index(0, BK_ROWS - 1)], 0, BK_COLS);
}

static void clear_next(bkt_game_t *t);

/* Erase animation: from the piece column outwards, one cell per step (animatedClearLine). */
static void erase_row(bkt_game_t *t, int y, int anchor)
{
    t->cl_y = y;
    int steps = anchor > BK_COLS - anchor ? anchor : BK_COLS - anchor;
    bk_game_cutscene(&t->base, BKT_SCENE_ERASE, (uint32_t)(steps * BKT_CLEAR_STEP_TICKS));
}

static void erase_frame(bkt_game_t *t, uint16_t elapsed)
{
    int anchor = t->cl_anchor;
    int steps = anchor > BK_COLS - anchor ? anchor : BK_COLS - anchor;
    int last = elapsed / BKT_CLEAR_STEP_TICKS;
    if (last > steps - 1) {
        last = steps - 1;
    }
    uint8_t work[BK_CELLS];
    memcpy(work, t->board, sizeof work);
    for (int i = 0; i <= last; i++) {
        int left = anchor - 1 - i;
        int right = anchor + i;
        if (left >= 0 && left < BK_COLS) {
            work[cell_index(left, t->cl_y)] = 0;
        }
        if (right >= 0 && right < BK_COLS) {
            work[cell_index(right, t->cl_y)] = 0;
        }
    }
    render(t, work, true, bk_game_overlay(&t->base));
}

/* End of the landing pipeline: a piece that still sticks out above the field ends the game. */
static void after_clear(bkt_game_t *t)
{
    if (t->land_y + t->land_max_y >= BK_ROWS) {
        t->over = true;
        bk_game_game_over(&t->base);
    }
}

/* removeFullLines: rows are removed one by one from the bottom; the top row of the field is never checked. */
static void clear_next(bkt_game_t *t)
{
    int count = 0;
    int first = -1;
    uint32_t mask = 0;
    for (int y = 0; y < BK_ROWS - 1; y++) {
        if (row_full(t, y)) {
            if (first < 0) {
                first = y;
            }
            count++;
            mask |= 1u << (BK_ROWS - 1 - y);
        }
    }
    if (first < 0) {
        after_clear(t);
        return;
    }
    t->cl_removed++;
    if (t->cl_removed == 1) {
        bk_game_emit(&t->base, BK_EV_LINES, (uint8_t)count, 0, mask);
    }
    add_points(t, t->cl_removed <= 4 ? LINE_POINTS[t->cl_removed] : 1u);
    erase_row(t, first, t->cl_anchor);
}

static void clear_lines(bkt_game_t *t, int anchor)
{
    t->cl_anchor = anchor;
    t->cl_removed = 0;
    clear_next(t);
}

static void piece_dropped(bkt_game_t *t)
{
    t->falling_finished = true;
    if (!t->has_piece) {
        return;
    }
    int x = t->piece_x;
    int y = t->piece_y;
    bk_game_emit(&t->base, BK_EV_LOCK, 0, 0, 0);
    t->land_y = y;
    t->land_max_y = t->piece.max_y;
    for (int i = 0; i < BKT_PIECE_CELLS; i++) {
        int bx = x + t->piece.cells[i].x;
        int by = y + t->piece.cells[i].y;
        if (bx >= 0 && bx < BK_COLS && by >= 0 && by < BK_ROWS) {
            t->board[cell_index(bx, by)] = BK_CELL_ON;
        }
    }
    t->has_piece = false;
    clear_lines(t, x);
}

static void tetris_scene_frame(bk_game_t *g, uint8_t kind, uint16_t elapsed)
{
    if (kind == BKT_SCENE_ERASE) {
        erase_frame(self(g), elapsed);
    }
}

static void tetris_scene_done(bk_game_t *g, uint8_t kind)
{
    bkt_game_t *t = self(g);
    if (kind == BKT_SCENE_ERASE) {
        drop_rows_above(t, t->cl_y);
        clear_next(t);
    }
}

/* ---------------------------------------------------------------- movement and keys */

static void one_line_down(bkt_game_t *t)
{
    if (!t->has_piece) {
        return;
    }
    bkt_piece_t p = t->piece;
    if (!try_move(t, &p, t->piece_x, t->piece_y - 1)) {
        piece_dropped(t);
    }
}

/* Hard drop: all the way down, then landing. Exists in the engine only; the console has no FIRE button. */
static void drop_down(bkt_game_t *t)
{
    bk_game_emit(&t->base, BK_EV_DROP, 0, 0, 0);
    while (t->has_piece) {
        bkt_piece_t p = t->piece;
        if (!try_move(t, &p, t->piece_x, t->piece_y - 1)) {
            break;
        }
    }
    piece_dropped(t);
}

static void move_sideways(bkt_game_t *t, int dx)
{
    if (!t->has_piece) {
        return;
    }
    bkt_piece_t p = t->piece;
    int before = t->piece_x;
    /* At a wall try_move "succeeds" by pushing the piece back: the sound only plays if it actually moved. */
    if (try_move(t, &p, before + dx, t->piece_y) && t->piece_x != before) {
        bk_game_emit(&t->base, BK_EV_MOVE, 0, 0, 0);
    }
}

static void turn(bkt_game_t *t)
{
    if (!t->has_piece) {
        return;
    }
    bkt_piece_t turned = bkt_piece_rotate(&t->piece);
    bool changed = turned.shape != t->piece.shape || turned.angle != t->piece.angle;
    if (try_move(t, &turned, t->piece_x, t->piece_y) && changed) {
        bk_game_emit(&t->base, BK_EV_ROTATE, 0, 0, 0);
    }
}

typedef struct {
    bool turn;
    bool left;
    bool right;
    bool down;
    bool drop;
} keys_t;

/* ROTATE and UP both rotate; DOWN is the soft drop; FIRE is the hard drop. Auto-repeat is polled every tick. */
static keys_t read_keys(bkt_game_t *t, const bk_input_t *in)
{
    keys_t k;
    k.turn = ((in->pressed & (BK_KEY_ROTATE | BK_KEY_UP)) != 0u);
    k.left = bk_repeat_fire(&t->move_cnt[0], BK_KEY_LEFT, in, BKT_MOVE_DELAY_TICKS, BKT_MOVE_REPEAT_TICKS);
    k.right = bk_repeat_fire(&t->move_cnt[1], BK_KEY_RIGHT, in, BKT_MOVE_DELAY_TICKS, BKT_MOVE_REPEAT_TICKS);
    k.down = bk_repeat_fire(&t->down_cnt, BK_KEY_DOWN, in, BKT_SOFT_DROP_TICKS, BKT_SOFT_DROP_TICKS);
    k.drop = ((in->pressed & BK_KEY_FIRE) != 0u);
    return k;
}

static bool alive(const bkt_game_t *t)
{
    return t->has_piece && !t->falling_finished && !bk_game_busy(&t->base);
}

/* Donor order: rotate, left, right, down, hard drop. After a landing or a cutscene the rest is skipped. */
static void apply_keys(bkt_game_t *t, const keys_t *k)
{
    if (k->turn) {
        turn(t);
    }
    if (alive(t) && k->left) {
        move_sideways(t, -1);
    }
    if (alive(t) && k->right) {
        move_sideways(t, 1);
    }
    if (alive(t) && k->down) {
        one_line_down(t);
        t->last_fall = (double)t->base.tick; /* resets gravity so a soft drop never doubles up with a gravity step */
    }
    if (alive(t) && k->drop) {
        drop_down(t);
    }
}

static void tetris_update(bk_game_t *g, const bk_input_t *in)
{
    bkt_game_t *t = self(g);
    keys_t keys = read_keys(t, in);
    double period = bkt_fall_ticks(t);
    double now = (double)g->tick;
    if (now - t->last_fall >= period) {
        /* Keep the fractional remainder: rounding the period would accumulate drift, especially at speed 10.
         * A cutscene skips the elapsed steps instead of replaying a burst afterwards. */
        t->last_fall = now - fmod(now - t->last_fall, period);
        if (t->falling_finished) {
            t->falling_finished = false;
            bkt_spawn(t);
        } else if (!keys.down && !keys.drop) {
            one_line_down(t);
        }
    }
    if (t->has_piece && !t->falling_finished && !bk_game_busy(g)) {
        apply_keys(t, &keys);
    }
}

/* ---------------------------------------------------------------- definition */

static const bk_game_vtable_t TETRIS_VTABLE = {
    tetris_reset, tetris_update, tetris_draw, tetris_draw_preview, NULL, tetris_scene_frame, tetris_scene_done,
};

bk_game_t *bk_tetris_create(void *storage, const bk_game_options_t *opt)
{
    bkt_game_t *t = (bkt_game_t *)storage;
    memset(t, 0, sizeof *t);
    bk_game_base_init(&t->base, &TETRIS_VTABLE, opt, 1);
    bk_game_restart(&t->base);
    return &t->base;
}

/*
 * The menu clip (ADR-041, own artwork): the bottom five rows of the field. Four pieces (O, O, J, L) build a heap around a
 * two-cell well, a vertical S drops into it, the two completed rows blink twice and burn out, and the loop starts again from
 * the one cell that is left. Every frame differs from the previous one by a single step of the action.
 */
// clang-format off
static const char TETRIS_CLIP_FRAMES[][BK_CLIP_CELLS + 1] = {
    /* 0: O enters */
    "##........" ".........." ".........." ".........." "....#.....",
    "##........" "##........" ".........." ".........." "....#.....",
    ".........." "##........" "##........" ".........." "....#.....",
    ".........." ".........." "##........" "##........" "....#.....",
    ".........." ".........." ".........." "##........" "##..#.....",
    /* 5: second O */
    "..##......" ".........." ".........." "##........" "##..#.....",
    "..##......" "..##......" ".........." "##........" "##..#.....",
    ".........." "..##......" "..##......" "##........" "##..#.....",
    ".........." ".........." "..##......" "####......" "##..#.....",
    ".........." ".........." ".........." "####......" "#####.....",
    /* 10: J */
    ".......###" ".........." ".........." "####......" "#####.....",
    ".........#" ".......###" ".........." "####......" "#####.....",
    ".........." ".........#" ".......###" "####......" "#####.....",
    ".........." ".........." ".........#" "####...###" "#####.....",
    ".........." ".........." ".........." "####.....#" "#####..###",
    /* 15: L */
    "......#..." ".........." ".........." "####.....#" "#####..###",
    "......###." "......#..." ".........." "####.....#" "#####..###",
    ".........." "......###." "......#..." "####.....#" "#####..###",
    ".........." ".........." "......###." "####..#..#" "#####..###",
    ".........." ".........." ".........." "####..####" "#####.####",
    /* 20: the S drops into the well (poster: frame 22) */
    ".....#...." ".........." ".........." "####..####" "#####.####",
    "....##...." ".....#...." ".........." "####..####" "#####.####",
    "....#....." "....##...." ".....#...." "####..####" "#####.####",
    ".........." "....#....." "....##...." "####.#####" "#####.####",
    ".........." ".........." "....#....." "##########" "##########",
    /* 25: the two rows blink and burn out, the last frame is the first one without the O */
    ".........." ".........." "....#....." ".........." "..........",
    ".........." ".........." "....#....." "##########" "##########",
    ".........." ".........." "....#....." ".........." "..........",
    ".........." ".........." ".........." ".........." "....#.....",
};
// clang-format on

static const bk_clip_t TETRIS_CLIP = {
    (uint16_t)BK_ARRAY_LEN(TETRIS_CLIP_FRAMES),
    10,
    22,
    TETRIS_CLIP_FRAMES,
};

const bk_game_def_t bk_tetris_def = {
    BK_TETRIS_UID, "tetris", "TETRIS", 1, 0, sizeof(bkt_game_t), _Alignof(bkt_game_t), bk_tetris_create, &TETRIS_CLIP,
};
