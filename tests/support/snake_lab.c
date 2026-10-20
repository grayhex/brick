#include "snake_lab.h"

#include <string.h>

void snake_lab_init(void *labp, const bk_game_options_t *opt)
{
    snake_lab_t *lab = labp;
    memset(lab, 0, sizeof *lab);
    bk_snake_create(&lab->game, opt);
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_start(&lab->game.base, &q);
}

bk_game_t *snake_lab_game(void *lab)
{
    return &((snake_lab_t *)lab)->game.base;
}

void snake_lab_set_worm(snake_lab_t *lab, int dir, const int *cells, unsigned count)
{
    for (unsigned i = 0; i < count; i++) {
        lab->game.body[i] = (uint8_t)cells[i];
    }
    lab->game.len = (uint8_t)count;
    lab->game.dir = (uint8_t)dir;
}

bool snake_lab_apply_op(void *labp, const jv_t *op, bk_event_queue_t *pending)
{
    (void)pending; /* none of the setup operations raises events */
    snake_lab_t *lab = labp;
    const char *name = jv_str(jv_at(op, 0));
    if (strcmp(name, "snake") == 0) {
        size_t n = op->len - 2;
        if (n > BK_CELLS) {
            return false;
        }
        lab->game.dir = (uint8_t)jv_num(jv_at(op, 1));
        lab->game.len = (uint8_t)n;
        for (size_t i = 0; i < n; i++) {
            lab->game.body[i] = (uint8_t)jv_num(jv_at(op, i + 2));
        }
    } else if (strcmp(name, "food") == 0) {
        lab->game.food = (uint8_t)jv_num(jv_at(op, 1));
    } else if (strcmp(name, "eaten") == 0) {
        lab->game.eaten = (uint8_t)jv_num(jv_at(op, 1));
    } else if (strcmp(name, "timer") == 0) {
        lab->game.timer = jv_num(jv_at(op, 1));
    } else if (strcmp(name, "lives") == 0) {
        bk_game_set_lives(&lab->game.base, (int)jv_num(jv_at(op, 1)));
    } else if (strcmp(name, "poke") == 0) {
        bk_game_set_score(&lab->game.base, (uint32_t)jv_num(jv_at(op, 1)));
    } else {
        return false;
    }
    return true;
}

const conf_game_t SNAKE_CONF = {"snake", sizeof(snake_lab_t), snake_lab_init, snake_lab_game, snake_lab_apply_op};
