#include "tank_lab.h"

#include <string.h>

void tank_lab_init(void *labp, const bk_game_options_t *opt)
{
    tank_lab_t *lab = labp;
    memset(lab, 0, sizeof *lab);
    bk_tank_create(&lab->game, opt);
    bk_event_queue_t q;
    bk_events_clear(&q);
    bk_game_start(&lab->game.base, &q);
}

bk_game_t *tank_lab_game(void *lab)
{
    return &((tank_lab_t *)lab)->game.base;
}

static int num(const jv_t *op, size_t i)
{
    return (int)jv_num(jv_at(op, i));
}

static void set_tank(bkk_tank_t *tank, bool *live, const jv_t *op, size_t i)
{
    int x = num(op, i);
    if (live != NULL) {
        *live = x != TANK_LAB_NONE;
    }
    if (x != TANK_LAB_NONE) {
        tank->x = (int8_t)x;
        tank->y = (int8_t)num(op, i + 1);
        tank->dir = (uint8_t)num(op, i + 2);
    }
}

static void set_bullet(bkk_bullet_t *bullet, const jv_t *op, size_t i)
{
    int x = num(op, i);
    bullet->live = x != TANK_LAB_NONE;
    if (bullet->live) {
        bullet->x = (int8_t)x;
        bullet->y = (int8_t)num(op, i + 1);
        bullet->dir = (uint8_t)num(op, i + 2);
    }
}

bool tank_lab_apply_op(void *labp, const jv_t *op, bk_event_queue_t *pending)
{
    tank_lab_t *lab = labp;
    bkk_game_t *s = &lab->game;
    const char *name = jv_str(jv_at(op, 0));
    /* setScore may raise an event (an extra life); it stays queued for the next plain step */
    bk_event_queue_t q;
    bk_events_clear(&q);
    s->base.out = &q;
    bool ok = true;
    if (strcmp(name, "poke") == 0) {
        bk_game_set_score(&s->base, (uint32_t)jv_num(jv_at(op, 1)));
    } else if (strcmp(name, "lives") == 0) {
        bk_game_set_lives(&s->base, num(op, 1));
    } else if (strcmp(name, "player") == 0) {
        set_tank(&s->player, NULL, op, 1);
    } else if (strcmp(name, "enemy") == 0) {
        int i = num(op, 1);
        set_tank(&s->enemy[i], &s->enemy_live[i], op, 2);
        s->on_board = 0;
        for (int k = 0; k < BKK_ENEMIES; k++) {
            s->on_board = (uint8_t)(s->on_board + (s->enemy_live[k] ? 1 : 0));
        }
    } else if (strcmp(name, "pbullet") == 0) {
        set_bullet(&s->player_bullet[num(op, 1)], op, 2);
    } else if (strcmp(name, "ebullet") == 0) {
        set_bullet(&s->enemy_bullet[num(op, 1)], op, 2);
    } else if (strcmp(name, "obst") == 0) {
        s->obstacles[num(op, 2) * BKK_W + num(op, 1)] = (uint8_t)num(op, 3);
    } else if (strcmp(name, "obstclear") == 0) {
        memset(s->obstacles, 0, sizeof s->obstacles);
    } else if (strcmp(name, "killed") == 0) {
        s->killed = (uint8_t)num(op, 1);
    } else if (strcmp(name, "onboard") == 0) {
        s->on_board = (uint8_t)num(op, 1);
    } else if (strcmp(name, "dead") == 0) {
        s->dead = num(op, 1) != 0;
    } else if (strcmp(name, "eidx") == 0) {
        s->enemy_index = (uint8_t)num(op, 1);
    } else if (strcmp(name, "ai") == 0) {
        int v = num(op, 1); /* the exporter's "never" is 100000: more than any scenario lasts */
        s->ai_left = (int16_t)(v > 30000 ? 30000 : v);
    } else if (strcmp(name, "start") == 0) {
        s->start_left = (int16_t)num(op, 1);
    } else if (strcmp(name, "btimer") == 0) {
        s->bullet_left = (uint8_t)(num(op, 1) > 255 ? 255 : num(op, 1));
    } else if (strcmp(name, "delay") == 0) {
        s->key_delay[num(op, 1)] = (uint8_t)num(op, 2);
    } else {
        ok = false;
    }
    s->base.out = NULL;
    for (unsigned i = 0; i < q.count; i++) {
        bk_events_push(pending, q.items[i].type, q.items[i].a, q.items[i].b, q.items[i].c);
    }
    return ok;
}

const conf_game_t TANK_CONF = {"tank", sizeof(tank_lab_t), tank_lab_init, tank_lab_game, tank_lab_apply_op};
