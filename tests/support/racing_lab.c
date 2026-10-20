#include "racing_lab.h"

#include <string.h>

void racing_lab_init(void *labp, const bk_game_options_t *opt)
{
    racing_lab_t *lab = labp;
    memset(lab, 0, sizeof *lab);
    bk_racing_create(&lab->game, opt);
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_start(&lab->game.base, &q);
}

bk_game_t *racing_lab_game(void *lab)
{
    return &((racing_lab_t *)lab)->game.base;
}

void racing_lab_set_cars(racing_lab_t *lab, const int *xy, unsigned count)
{
    lab->game.opp_count = (uint8_t)count;
    for (unsigned i = 0; i < count; i++) {
        lab->game.opp[i].x = (int8_t)xy[2 * i];
        lab->game.opp[i].y = (int8_t)xy[2 * i + 1];
    }
}

bool racing_lab_apply_op(void *labp, const jv_t *op, bk_event_queue_t *pending)
{
    racing_lab_t *lab = labp;
    bkr_game_t *s = &lab->game;
    const char *name = jv_str(jv_at(op, 0));
    /* setScore and setLives may raise events (a speed step, an extra life); they stay queued for the next plain step */
    bk_event_queue_t q;
    bk_events_clear(&q);
    s->base.out = &q;
    bool ok = true;
    if (strcmp(name, "poke") == 0) {
        bkr_set_score(s, (uint32_t)jv_num(jv_at(op, 1)));
    } else if (strcmp(name, "lives") == 0) {
        bk_game_set_lives(&s->base, (int)jv_num(jv_at(op, 1)));
    } else if (strcmp(name, "opps") == 0) {
        size_t n = (op->len - 1) / 2;
        if (n > BKR_MAX_OPPONENTS) {
            ok = false;
        } else {
            s->opp_count = (uint8_t)n;
            for (size_t i = 0; i < n; i++) {
                s->opp[i].x = (int8_t)jv_num(jv_at(op, 1 + 2 * i));
                s->opp[i].y = (int8_t)jv_num(jv_at(op, 2 + 2 * i));
            }
        }
    } else if (strcmp(name, "tail") == 0) {
        int v = (int)jv_num(jv_at(op, 1));
        s->tail_valid = v != RACING_LAB_NO_TAIL;
        s->tail_y = (int16_t)(s->tail_valid ? v : 0);
    } else if (strcmp(name, "lane") == 0) {
        s->lane = (uint8_t)jv_num(jv_at(op, 1));
        s->car_x = (int8_t)BKR_LANE_X[s->lane];
    } else if (strcmp(name, "since") == 0) {
        s->since_step = (uint16_t)jv_num(jv_at(op, 1));
    } else if (strcmp(name, "odd") == 0) {
        s->odd_move = jv_num(jv_at(op, 1)) != 0.0;
    } else if (strcmp(name, "border") == 0) {
        s->border_phase = (uint8_t)jv_num(jv_at(op, 1));
    } else {
        ok = false;
    }
    s->base.out = NULL;
    for (unsigned i = 0; i < q.count; i++) {
        bk_events_push(pending, q.items[i].type, q.items[i].a, q.items[i].b, q.items[i].c);
    }
    return ok;
}

const conf_game_t RACING_CONF = {"racing", sizeof(racing_lab_t), racing_lab_init, racing_lab_game, racing_lab_apply_op};
