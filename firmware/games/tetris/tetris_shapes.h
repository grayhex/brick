/*
 * Tetris piece catalogue (port of ApolloBrickGames src/brick/games/tetris/shapes.ts, tetromino part only).
 *
 * Coordinates follow the reference: x grows to the right, y grows UP (y = 0 is the bottom of the well).
 * A piece is a set of offsets from an anchor point. Rotation by 90 degrees clockwise maps (x, y) -> (y, -x);
 * afterwards the piece is "aligned" (shifted right and/or up) exactly like TetrisShape.setShape of the donor.
 *
 * Scope: only the seven tetrominoes of the base variant. The reference catalogue also holds tromino,
 * pentomino and "special" shapes used by other variants and by Pentix; they are intentionally NOT ported yet
 * (docs/GAME_TETRIS.md, "Not ported").
 */
#ifndef BRICK_TETRIS_SHAPES_H
#define BRICK_TETRIS_SHAPES_H

#include "brick/defs.h"

BK_EXTERN_C_BEGIN

/* Catalogue order of the donor: Z, S, Line, T, Square, L, MirroredL. */
#define BKT_SHAPE_COUNT  7
#define BKT_SHAPE_LINE   2
#define BKT_SHAPE_SQUARE 4

#define BKT_PIECE_CELLS 4

typedef struct {
    int8_t x;
    int8_t y;
} bkt_coord_t;

typedef struct {
    uint8_t shape;                      /* 0..6 */
    uint8_t angle;                      /* 0..3, clockwise quarter turns */
    bkt_coord_t cells[BKT_PIECE_CELLS]; /* after rotation and alignment */
    int8_t min_x, max_x, min_y, max_y;
} bkt_piece_t;

/* Piece in a given pose (angle wraps modulo 4; shape wraps modulo 7). */
bkt_piece_t bkt_piece_make(int shape, int angle);

/* Quarter turn clockwise; the square does not rotate. */
bkt_piece_t bkt_piece_rotate(const bkt_piece_t *piece);

const char *bkt_shape_name(int shape);

BK_EXTERN_C_END

#endif /* BRICK_TETRIS_SHAPES_H */
