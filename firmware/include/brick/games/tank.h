/*
 * Tank (reference: ApolloBrickGames `tank`, the TanksGame of the donor n0Live/BrickGame): a tank fights three enemy tanks
 * that come in from the edges of the field, drive towards it and shoot. One variant (ADR-036, ADR-044): three enemies and
 * the ready-made fields of levels 2..10; the donor's other seven variants (four to six enemies, random fields) are not
 * ported. The rules are a reconstruction from the donor, NOT checked against the ROM of a real device (docs/GAME_TANK.md).
 */
#ifndef BRICK_GAMES_TANK_H
#define BRICK_GAMES_TANK_H

#include "brick/game.h"

BK_EXTERN_C_BEGIN

#define BK_TANK_UID 4u

extern const bk_game_def_t bk_tank_def;

bk_game_t *bk_tank_create(void *storage, const bk_game_options_t *opt);

BK_EXTERN_C_END

#endif /* BRICK_GAMES_TANK_H */
