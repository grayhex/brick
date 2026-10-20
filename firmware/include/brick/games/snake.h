/*
 * Snake (reference: ApolloBrickGames library game `snake`). Variant 01 is the profile checked against the ROM of
 * E88 no. 5 / E-23 no. 93 by the reference authors (start, reversing, ten step periods); variant 02 "Frame" is a
 * reconstruction from the wiki. See docs/GAME_SNAKE.md for what is verified and what is an adaptation.
 */
#ifndef BRICK_GAMES_SNAKE_H
#define BRICK_GAMES_SNAKE_H

#include "brick/game.h"

BK_EXTERN_C_BEGIN

#define BK_SNAKE_UID 2u

extern const bk_game_def_t bk_snake_def;

bk_game_t *bk_snake_create(void *storage, const bk_game_options_t *opt);

BK_EXTERN_C_END

#endif /* BRICK_GAMES_SNAKE_H */
