/*
 * Conformance of the C Tank port against recorded traces of the TypeScript reference: after EVERY tick a digest of the
 * complete observable state must equal the one the reference produced (tests/support/conf_runner.c,
 * docs/CONFORMANCE.md). Besides the scenarios the fixture holds samples of the pure functions: the tank shapes, the
 * ready-made fields, the AI weights and the chance of a shot (compared bit by bit), and moves of tanks. Regenerate the
 * fixture with `npm run export:tank` in tools/conformance.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tank_lab.h"

static const jv_t *g_root;

static void test_rng(void)
{
    const jv_t *list = jv_get(g_root, "rng");
    CHECK(list->len > 0);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *entry = jv_at(list, i);
        bk_rng_t rng;
        bk_rng_seed(&rng, (uint32_t)jv_num(jv_get(entry, "seed")));
        const jv_t *ops = jv_get(entry, "ops");
        for (size_t k = 0; k < ops->len; k++) {
            const jv_t *op = jv_at(ops, k);
            if (strcmp(jv_str(jv_at(op, 0)), "int") == 0) {
                CHECK_EQ(bk_rng_int(&rng, (uint32_t)jv_num(jv_at(op, 1))), (long long)jv_num(jv_at(op, 2)));
            } else {
                CHECK_STR(jv_str(jv_at(op, 0)), "u32");
                CHECK_EQ(bk_rng_next_u32(&rng), (long long)jv_num(jv_at(op, 1)));
            }
        }
    }
}

static void test_enemy_ticks(void)
{
    const jv_t *row = jv_get(g_root, "enemyTicks");
    CHECK_EQ(row->len, 10);
    for (int speed = 1; speed <= 10; speed++) {
        /* exact: the same rounding of the same IEEE doubles as in the reference */
        CHECK_EQ(bkk_enemy_ticks(speed), (long long)jv_num(jv_at(row, (size_t)speed - 1)));
    }
}

static int cmp_cell(const void *a, const void *b)
{
    const int8_t *p = a;
    const int8_t *q = b;
    return p[0] != q[0] ? p[0] - q[0] : p[1] - q[1];
}

static void test_masks(void)
{
    const jv_t *list = jv_get(g_root, "masks");
    CHECK_EQ(list->len, 12);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *entry = jv_at(list, i);
        int kind = (int)jv_num(jv_get(entry, "kind"));
        int dir = (int)jv_num(jv_get(entry, "dir"));
        const jv_t *cells = jv_get(entry, "cells");
        int8_t mine[9][2];
        int n = bkk_mask(kind, dir, mine);
        CHECK_EQ(n, (long long)cells->len);
        qsort(mine, (size_t)n, sizeof mine[0], cmp_cell);
        for (int k = 0; k < n && (size_t)k < cells->len; k++) {
            CHECK_EQ(mine[k][0], (long long)jv_num(jv_at(jv_at(cells, (size_t)k), 0)));
            CHECK_EQ(mine[k][1], (long long)jv_num(jv_at(jv_at(cells, (size_t)k), 1)));
        }
    }
}

static void test_levels(void)
{
    const jv_t *list = jv_get(g_root, "levels");
    CHECK_EQ(list->len, 10);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *entry = jv_at(list, i);
        uint8_t board[BK_CELLS];
        bkk_preset_obstacles((int)jv_num(jv_get(entry, "level")), board);
        const jv_t *rows = jv_get(entry, "rows");
        CHECK_EQ(rows->len, 20);
        for (int y = 0; y < 20; y++) {
            const char *row = jv_str(jv_at(rows, (size_t)y));
            for (int x = 0; x < 10; x++) {
                CHECK_EQ(board[y * BKK_W + x], row[x] == '#' ? 1 : 0);
            }
        }
    }
}

static void load_board(uint8_t board[BK_CELLS], const char *text)
{
    CHECK_EQ(strlen(text), BK_CELLS);
    for (int i = 0; i < BK_CELLS; i++) {
        board[i] = text[i] == '1' ? 1u : 0u;
    }
}

static bkk_tank_t tank_of(const jv_t *triple)
{
    bkk_tank_t t = {(int8_t)jv_num(jv_at(triple, 0)), (int8_t)jv_num(jv_at(triple, 1)), (uint8_t)jv_num(jv_at(triple, 2))};
    return t;
}

static void test_ai_samples(void)
{
    const jv_t *list = jv_get(g_root, "aiSamples");
    CHECK(list->len > 100);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *e = jv_at(list, i);
        static bkk_game_t s; /* the AI reads only the tanks of the game */
        memset(&s, 0, sizeof s);
        uint8_t board[BK_CELLS];
        load_board(board, jv_str(jv_get(e, "board")));
        s.enemy[0] = tank_of(jv_get(e, "tank"));
        s.enemy_live[0] = true;
        s.player = tank_of(jv_get(e, "player"));
        const jv_t *enemies = jv_get(e, "enemies");
        for (int k = 1; k < BKK_ENEMIES; k++) {
            const jv_t *other = jv_at(enemies, (size_t)k);
            if ((int)jv_num(jv_at(other, 0)) != TANK_LAB_NONE) {
                s.enemy[k] = tank_of(other);
                s.enemy_live[k] = true;
            }
        }

        double weights[4];
        bkk_move_weights(board, &s.enemy[0], &s.player, weights);
        const jv_t *w4 = jv_get(e, "weights4");
        for (int d = 0; d < 4; d++) {
            CHECK_EQ((long long)(weights[d] * 4.0), (long long)jv_num(jv_at(w4, (size_t)d)));
        }
        int order[4];
        bkk_sort_by_weight(weights, order);
        for (int d = 0; d < 4; d++) {
            CHECK_EQ(order[d], (long long)jv_num(jv_at(jv_get(e, "order"), (size_t)d)));
        }
        double chance = bkk_shot_chance(board, &s, 0);
        uint64_t bits;
        memcpy(&bits, &chance, sizeof bits);
        char mine[17];
        snprintf(mine, sizeof mine, "%016llx", (unsigned long long)bits);
        CHECK_STR(mine, jv_str(jv_get(e, "chance"))); /* the IEEE double, bit for bit */
        for (int d = 0; d < 4; d++) {
            CHECK_EQ(bkk_distance_to_obstacle(board, s.enemy[0].x, s.enemy[0].y, d),
                     (long long)jv_num(jv_at(jv_get(e, "distances"), (size_t)d)));
        }
    }
}

static void test_move_samples(void)
{
    const jv_t *list = jv_get(g_root, "moveSamples");
    CHECK(list->len > 100);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *e = jv_at(list, i);
        uint8_t board[BK_CELLS];
        load_board(board, jv_str(jv_get(e, "board")));
        int kind = (int)jv_num(jv_get(e, "kind"));
        int dir = (int)jv_num(jv_get(e, "dir"));
        bkk_tank_t next = bkk_move_tank(board, kind, tank_of(jv_get(e, "tank")), dir, jv_get(e, "rotationOnly")->num != 0.0);
        bkk_tank_t want = tank_of(jv_get(e, "result"));
        CHECK_EQ(next.x, want.x);
        CHECK_EQ(next.y, want.y);
        CHECK_EQ(next.dir, want.dir);
        const jv_t *probe = jv_get(e, "probe");
        int px = (int)jv_num(jv_at(probe, 0));
        int py = (int)jv_num(jv_at(probe, 1));
        CHECK_EQ(bkk_hits_board(board, BKK_BOX, dir, px, py) ? 1 : 0, jv_get(e, "hits")->num != 0.0 ? 1 : 0);
        CHECK_EQ(bkk_can_place(board, kind, dir, px, py) ? 1 : 0, jv_get(e, "place")->num != 0.0 ? 1 : 0);
    }
}

static void test_pick_samples(void)
{
    const jv_t *list = jv_get(g_root, "pickSamples");
    CHECK(list->len > 20);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *e = jv_at(list, i);
        uint8_t board[BK_CELLS];
        load_board(board, jv_str(jv_get(e, "board")));
        bk_rng_t rng;
        bk_rng_seed(&rng, (uint32_t)jv_num(jv_get(e, "seed")));
        bkk_tank_t tank = tank_of(jv_get(e, "tank"));
        bkk_tank_t player = tank_of(jv_get(e, "player"));
        CHECK_EQ(bkk_choose_move(board, &tank, &player, &rng), (long long)jv_num(jv_get(e, "dir")));
        CHECK_EQ(bk_rng_next_u32(&rng), (long long)jv_num(jv_get(e, "next"))); /* the same number of dice was thrown */
    }
}

static void test_scenarios(void)
{
    conf_run_scenarios(&TANK_CONF, g_root, 60);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/fixtures/tank/tank.conformance.json";
    jv_t *root = conf_load(path, "tank");
    if (root == NULL) {
        return 2;
    }
    g_root = root;
    TEST_RUN(test_rng);
    TEST_RUN(test_enemy_ticks);
    TEST_RUN(test_masks);
    TEST_RUN(test_levels);
    TEST_RUN(test_ai_samples);
    TEST_RUN(test_move_samples);
    TEST_RUN(test_pick_samples);
    TEST_RUN(test_scenarios);
    jv_free(root);
    return bk_test_finish("tank_conformance");
}
