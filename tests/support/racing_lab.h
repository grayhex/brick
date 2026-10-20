/* Setup helpers for Racing tests: the C counterpart of the operations the exporter applies to the reference. */
#ifndef BK_RACING_LAB_H
#define BK_RACING_LAB_H

#include "brick/games/racing.h"
#include "conf_runner.h"
#include "racing_internal.h"

typedef struct {
    bkr_game_t game;
} racing_lab_t;

/* The exporter's "no group on the road" value of `tail` (null in the reference). */
#define RACING_LAB_NO_TAIL (-1000)

void racing_lab_init(void *lab, const bk_game_options_t *opt); /* create and start, like the reference plainLab */
bk_game_t *racing_lab_game(void *lab);
/* ['poke', score] | ['lives', n] | ['opps', x, y, ...] | ['tail', y] | ['lane', i] | ['since', n] | ['odd', 0|1] |
 * ['border', phase] */
bool racing_lab_apply_op(void *lab, const jv_t *op, bk_event_queue_t *pending);

/* Direct setup for unit tests. */
void racing_lab_set_cars(racing_lab_t *lab, const int *xy, unsigned count); /* x, y pairs */

extern const conf_game_t RACING_CONF;

#endif
