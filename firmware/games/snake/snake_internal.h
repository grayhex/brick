/*
 * Internal state of the Snake game. Exposed so that tests can set up exact positions (the C counterpart of the
 * reference tests reaching into the game). Not part of the public API.
 */
#ifndef BRICK_SNAKE_INTERNAL_H
#define BRICK_SNAKE_INTERNAL_H

#include "brick/games/snake.h"

BK_EXTERN_C_BEGIN

#define BKS_MEALS_PER_LEVEL 13
#define BKS_LIVES           4
#define BKS_SCENE_LOSE      1u /* clear scene after a lost life: the round restarts */
#define BKS_SCENE_WIN       2u /* clear scene after a won level: next level, the round restarts */

/* Cell = y * 10 + x, y counted from the TOP; direction 0 up, 1 right, 2 down, 3 left (reference order). */
typedef struct {
    bk_game_t base; /* must be first */

    uint8_t body[BK_CELLS]; /* head first */
    uint8_t len;
    uint8_t dir;
    uint8_t food;
    uint8_t eaten;
    double timer; /* ticks since the last step, fractional (the ROM periods are not whole ticks, see ADR-024) */
} bks_game_t;

/* Step period in ticks at `speed` (1..10) as the exact IEEE double the reference computes. */
double bks_step_ticks(int speed);

BK_EXTERN_C_END

#endif /* BRICK_SNAKE_INTERNAL_H */
