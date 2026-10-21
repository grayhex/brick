/* Setup helpers for Tank tests: the C counterpart of the operations the exporter applies to the reference. */
#ifndef BK_TANK_LAB_H
#define BK_TANK_LAB_H

#include "brick/games/tank.h"
#include "conf_runner.h"
#include "tank_internal.h"

typedef struct {
    bkk_game_t game;
} tank_lab_t;

/* The exporter's "no tank / no bullet in this cell" value of x (null in the reference). */
#define TANK_LAB_NONE (-100)

void tank_lab_init(void *lab, const bk_game_options_t *opt); /* create and start, like the reference plainLab */
bk_game_t *tank_lab_game(void *lab);
/* ['poke', score] | ['lives', n] | ['player', x, y, dir] | ['enemy', i, x, y, dir] | ['pbullet', i, x, y, dir] |
 * ['ebullet', i, x, y, dir] | ['obst', x, y, v] | ['obstclear'] | ['killed', n] | ['onboard', n] | ['dead', 0|1] |
 * ['eidx', n] | ['ai', n] | ['start', n] | ['btimer', n] | ['delay', key, n] */
bool tank_lab_apply_op(void *lab, const jv_t *op, bk_event_queue_t *pending);

extern const conf_game_t TANK_CONF;

#endif
