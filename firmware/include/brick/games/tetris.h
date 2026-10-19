/*
 * Tetris, base variant (reference: ApolloBrickGames game `tetris`, mode 01 = donor class I, type 1).
 * Rules are a port of src/brick/games/tetris/core.ts; see docs/GAME_TETRIS.md for what is and is not ported.
 */
#ifndef BRICK_GAMES_TETRIS_H
#define BRICK_GAMES_TETRIS_H

#include "brick/game.h"

BK_EXTERN_C_BEGIN

#define BK_TETRIS_UID 1u

extern const bk_game_def_t bk_tetris_def;

bk_game_t *bk_tetris_create(void *storage, const bk_game_options_t *opt);

BK_EXTERN_C_END

#endif /* BRICK_GAMES_TETRIS_H */
