/*
 * Internal state of the Tetris game. Exposed so that tests can set up exact positions (the C counterpart of
 * the reference `Lab` test helper). Not part of the public API.
 */
#ifndef BRICK_TETRIS_INTERNAL_H
#define BRICK_TETRIS_INTERNAL_H

#include "brick/games/tetris.h"
#include "tetris_shapes.h"

BK_EXTERN_C_BEGIN

/* Timings of the reference (ticks at 60 Hz). */
#define BKT_MOVE_DELAY_TICKS  8 /* msToTicks(130): auto-repeat of LEFT/RIGHT starts after ~130 ms */
#define BKT_MOVE_REPEAT_TICKS 1
#define BKT_SOFT_DROP_TICKS   1
#define BKT_CLEAR_STEP_TICKS  4 /* msToTicks(60): row erase animation step */
#define BKT_POINTS_PER_SPEED  1000

#define BKT_SCENE_ERASE 1u

typedef struct {
    bk_game_t base; /* must be first */

    /* Settled cells: board[y * 10 + x], y = 0 is the BOTTOM of the well. The falling piece is not stored here. */
    uint8_t board[BK_CELLS];
    bool has_piece;
    bkt_piece_t piece;
    int piece_x;
    int piece_y;
    bkt_piece_t next_piece;
    bool falling_finished; /* landed; the next piece appears on the next fall step */
    double last_fall;      /* tick of the last gravity step, fractional (see ADR-024) */
    bool over;

    uint32_t move_cnt[2]; /* auto-repeat counters: LEFT, RIGHT */
    uint32_t down_cnt;    /* auto-repeat counter: soft drop */

    /* Landing pipeline (erase animation continuation). */
    int cl_anchor;
    int cl_removed;
    int cl_y;
    int land_y;
    int land_max_y;
} bkt_game_t;

/* Gravity period in ticks (fractional) at the current speed. */
double bkt_fall_ticks(const bkt_game_t *t);

/* Make `next_piece` the falling piece (reference: spawn). May end the game. */
void bkt_spawn(bkt_game_t *t);

BK_EXTERN_C_END

#endif /* BRICK_TETRIS_INTERNAL_H */
