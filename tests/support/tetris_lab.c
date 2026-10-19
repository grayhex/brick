#include "tetris_lab.h"

#include <string.h>

bk_game_options_t lab_options(uint32_t seed, unsigned speed, unsigned level, uint32_t hi_score)
{
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = seed;
    o.speed = (uint8_t)speed;
    o.level = (uint8_t)level;
    o.hi_score = hi_score;
    o.mode = 0;
    return o;
}

void lab_init(lab_t *lab, const bk_game_options_t *opt)
{
    memset(lab, 0, sizeof *lab);
    bk_tetris_create(&lab->game, opt);
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_start(lab_g(lab), &q);
}

void lab_init_plain(lab_t *lab)
{
    bk_game_options_t o = lab_options(5, 1, 1, 0);
    lab_init(lab, &o);
}

void lab_log_clear(lab_t *lab)
{
    lab->log_count = 0;
}

void lab_step(lab_t *lab, uint8_t pressed, uint8_t held)
{
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_input_t in = bk_input_make(pressed, held);
    bk_game_step(lab_g(lab), &in, &q);
    for (unsigned i = 0; i < q.count && lab->log_count < LAB_LOG_CAP; i++) {
        lab->log[lab->log_count++] = q.items[i];
    }
}

void lab_idle(lab_t *lab, unsigned ticks)
{
    for (unsigned i = 0; i < ticks; i++) {
        lab_step(lab, 0, 0);
    }
}

void lab_tap(lab_t *lab, uint8_t keys)
{
    lab_step(lab, keys, keys);
    lab_step(lab, 0, 0);
}

void lab_press(lab_t *lab, uint8_t keys)
{
    lab_step(lab, keys, keys);
}

void lab_hold(lab_t *lab, uint8_t key, unsigned ticks)
{
    for (unsigned i = 0; i < ticks; i++) {
        lab_step(lab, i == 0 ? key : 0, key);
    }
}

unsigned lab_until_landed(lab_t *lab, unsigned limit)
{
    unsigned n = 0;
    while (!lab->game.falling_finished && n < limit) {
        lab_step(lab, 0, 0);
        n++;
    }
    return n;
}

unsigned lab_until_phase(lab_t *lab, bk_phase_t phase, unsigned limit)
{
    unsigned n = 0;
    while (lab_g(lab)->phase != phase && n < limit) {
        lab_step(lab, 0, 0);
        n++;
    }
    return n;
}

bool lab_has(const lab_t *lab, bk_event_type_t type)
{
    return lab_count_events(lab, type) > 0;
}

unsigned lab_count_events(const lab_t *lab, bk_event_type_t type)
{
    unsigned n = 0;
    for (unsigned i = 0; i < lab->log_count; i++) {
        if (lab->log[i].type == type) {
            n++;
        }
    }
    return n;
}

void lab_wipe(lab_t *lab)
{
    memset(lab->game.board, 0, sizeof lab->game.board);
    lab->game.has_piece = false;
}

void lab_fill(lab_t *lab, int y, const char *pattern)
{
    for (int x = 0; x < BK_COLS; x++) {
        lab->game.board[y * BK_COLS + x] = pattern[x] == '#' ? 1 : 0;
    }
}

void lab_put(lab_t *lab, int shape, int angle, int x, int y)
{
    lab->game.piece = bkt_piece_make(shape, angle);
    lab->game.has_piece = true;
    lab->game.piece_x = x;
    lab->game.piece_y = y;
    lab->game.falling_finished = false;
}

void lab_set_next(lab_t *lab, int shape, int angle)
{
    lab->game.next_piece = bkt_piece_make(shape, angle);
}

/* Events raised outside of a step (the spawn may end the game) are kept in the log, like the pending queue of the reference.
 */
void lab_respawn(lab_t *lab, int shape, int angle)
{
    bk_event_queue_t q;
    bk_events_clear(&q);
    lab->game.next_piece = bkt_piece_make(shape, angle);
    lab->game.falling_finished = false;
    lab_g(lab)->out = &q;
    bkt_spawn(&lab->game);
    lab_g(lab)->out = NULL;
    for (unsigned i = 0; i < q.count && lab->log_count < LAB_LOG_CAP; i++) {
        lab->log[lab->log_count++] = q.items[i];
    }
}

void lab_due_fall(lab_t *lab)
{
    lab->game.last_fall = (double)lab_g(lab)->tick - bkt_fall_ticks(&lab->game);
}

void lab_poke(lab_t *lab, uint32_t score)
{
    bk_game_set_score(lab_g(lab), score);
}

int lab_cell(const lab_t *lab, int x, int y)
{
    return lab->game.board[y * BK_COLS + x];
}

int lab_count(const lab_t *lab)
{
    int n = 0;
    for (int i = 0; i < BK_CELLS; i++) {
        n += lab->game.board[i] != 0;
    }
    return n;
}

void lab_rows(lab_t *lab, char rows[BK_ROWS][BK_COLS + 1])
{
    uint8_t cells[BK_CELLS];
    bk_game_cells(lab_g(lab), cells);
    for (int r = 0; r < BK_ROWS; r++) {
        for (int c = 0; c < BK_COLS; c++) {
            rows[r][c] = ".#*"[cells[r * BK_COLS + c] % 3];
        }
        rows[r][BK_COLS] = '\0';
    }
}

void lab_preview_rows(lab_t *lab, char rows[BK_PREVIEW_ROWS][BK_PREVIEW_COLS + 1])
{
    uint8_t cells[BK_PREVIEW_CELLS];
    bk_game_preview(lab_g(lab), cells);
    for (int r = 0; r < BK_PREVIEW_ROWS; r++) {
        for (int c = 0; c < BK_PREVIEW_COLS; c++) {
            rows[r][c] = ".#*"[cells[r * BK_PREVIEW_COLS + c] % 3];
        }
        rows[r][BK_PREVIEW_COLS] = '\0';
    }
}

int lab_shape_by_name(const char *name)
{
    for (int i = 0; i < BKT_SHAPE_COUNT; i++) {
        if (strcmp(bkt_shape_name(i), name) == 0) {
            return i;
        }
    }
    return -1;
}
