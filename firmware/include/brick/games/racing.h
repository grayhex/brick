/*
 * Racing (reference: ApolloBrickGames `racing`, the RacingGame of the donor n0Live/BrickGame): a car on two lanes dodges
 * oncoming traffic. One variant (ADR-036): the donor's three lanes and upside-down field are not ported. The rules are a
 * reconstruction from the donor, NOT checked against the ROM of a real device (docs/GAME_RACING.md).
 */
#ifndef BRICK_GAMES_RACING_H
#define BRICK_GAMES_RACING_H

#include "brick/game.h"

BK_EXTERN_C_BEGIN

#define BK_RACING_UID 3u

extern const bk_game_def_t bk_racing_def;

bk_game_t *bk_racing_create(void *storage, const bk_game_options_t *opt);

BK_EXTERN_C_END

#endif /* BRICK_GAMES_RACING_H */
