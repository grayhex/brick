/*
 * Stub games for tests of the console and of the game base class. They prove that the console is game-agnostic
 * (several games, several modes, glyph letters) without pretending that more games are ported.
 * Rules: ROTATE adds 10 points; every 30 ticks adds 1; the game ends at STUB_GOAL points. The mode only shifts the goal.
 */
#ifndef BK_STUB_GAME_H
#define BK_STUB_GAME_H

#include "brick/game.h"

#define STUB_GOAL 30

extern const bk_game_def_t stub_def_multi;  /* uid 100, 3 modes */
extern const bk_game_def_t stub_def_single; /* uid 101, 1 mode */

/* What the stub has seen, for assertions. Reset by creating a new game. */
typedef struct {
    bk_game_t base;
    uint32_t steps;
    uint32_t rotate_presses;
    uint8_t last_held;
    uint32_t created_seed;
} stub_game_t;

#endif
