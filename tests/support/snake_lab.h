/* Setup helpers for Snake tests: the C counterpart of the operations the exporter applies to the reference. */
#ifndef BK_SNAKE_LAB_H
#define BK_SNAKE_LAB_H

#include "brick/games/snake.h"
#include "conf_runner.h"
#include "snake_internal.h"

typedef struct {
    bks_game_t game;
} snake_lab_t;

void snake_lab_init(void *lab, const bk_game_options_t *opt); /* create and start, like the reference plainLab */
bk_game_t *snake_lab_game(void *lab);
/* ['snake', dir, ...cells] | ['food', cell] | ['eaten', n] | ['timer', t] | ['lives', n] | ['poke', score] */
bool snake_lab_apply_op(void *lab, const jv_t *op, bk_event_queue_t *pending);

/* Direct setup for unit tests. */
void snake_lab_set_worm(snake_lab_t *lab, int dir, const int *cells, unsigned count);

extern const conf_game_t SNAKE_CONF;

#endif
