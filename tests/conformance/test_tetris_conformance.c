/*
 * Conformance of the C Tetris port against recorded traces of the TypeScript reference.
 *
 * For every scenario the exact inputs of the reference run are replayed into the C game, and after EVERY tick
 * a digest of the complete observable state (cells, preview, score, speed, level, phase, flood animation,
 * ordered events) must equal the one the reference produced. The first divergence is reported with the C state.
 * See docs/CONFORMANCE.md for the fixture format and how to regenerate it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conf_runner.h"
#include "tetris_lab.h"

static const jv_t *g_root;

/* ---------------------------------------------------------------- static sections */

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
            const char *kind = jv_str(jv_at(op, 0));
            if (strcmp(kind, "next") == 0) {
                CHECK_EQ(bk_rng_next_u32(&rng), (long long)jv_num(jv_at(op, 1)));
            } else if (strcmp(kind, "int") == 0) {
                CHECK_EQ(bk_rng_int(&rng, (uint32_t)jv_num(jv_at(op, 1))), (long long)jv_num(jv_at(op, 2)));
            } else if (strcmp(kind, "chance") == 0) {
                /* the only probability the game uses is 0.5 */
                CHECK_EQ(bk_rng_chance(&rng, 1, 2) ? 1 : 0, (long long)jv_num(jv_at(op, 2)));
            } else {
                CHECK(0);
            }
        }
    }
}

static void test_catalog(void)
{
    const jv_t *list = jv_get(g_root, "catalog");
    CHECK_EQ(list->len, BKT_SHAPE_COUNT * 4);
    for (size_t i = 0; i < list->len; i++) {
        const jv_t *e = jv_at(list, i);
        int shape = lab_shape_by_name(jv_str(jv_get(e, "shape")));
        CHECK(shape >= 0);
        bkt_piece_t p = bkt_piece_make(shape, (int)jv_num(jv_get(e, "angle")));
        const jv_t *cells = jv_get(e, "cells");
        CHECK_EQ(cells->len, BKT_PIECE_CELLS);
        for (int c = 0; c < BKT_PIECE_CELLS; c++) {
            CHECK_EQ(p.cells[c].x, (long long)jv_num(jv_at(jv_at(cells, (size_t)c), 0)));
            CHECK_EQ(p.cells[c].y, (long long)jv_num(jv_at(jv_at(cells, (size_t)c), 1)));
        }
        CHECK_EQ(p.min_x, (long long)jv_num(jv_get(e, "minX")));
        CHECK_EQ(p.max_x, (long long)jv_num(jv_get(e, "maxX")));
        CHECK_EQ(p.min_y, (long long)jv_num(jv_get(e, "minY")));
        CHECK_EQ(p.max_y, (long long)jv_num(jv_get(e, "maxY")));
    }
}

static void test_fall_ticks(void)
{
    const jv_t *list = jv_get(g_root, "fallTicks");
    CHECK_EQ(list->len, 10);
    for (unsigned speed = 1; speed <= 10; speed++) {
        lab_t lab;
        bk_game_options_t o = lab_options(5, speed, 1, 0);
        lab_init(&lab, &o);
        /* exact equality: the gravity period must be the very same IEEE double as in the reference */
        CHECK(bkt_fall_ticks(&lab.game) == jv_num(jv_at(list, speed - 1)));
    }
}

/* ---------------------------------------------------------------- scenario replay */

static void tetris_init(void *lab, const bk_game_options_t *opt)
{
    lab_init((lab_t *)lab, opt);
}

static bk_game_t *tetris_game(void *lab)
{
    return lab_g((lab_t *)lab);
}

static bool tetris_apply_op(void *labp, const jv_t *op, bk_event_queue_t *pending)
{
    lab_t *lab = labp;
    const char *name = jv_str(jv_at(op, 0));
    lab_log_clear(lab);
    if (strcmp(name, "wipe") == 0) {
        lab_wipe(lab);
    } else if (strcmp(name, "fill") == 0) {
        lab_fill(lab, (int)jv_num(jv_at(op, 1)), jv_str(jv_at(op, 2)));
    } else if (strcmp(name, "put") == 0) {
        lab_put(lab, lab_shape_by_name(jv_str(jv_at(op, 1))), (int)jv_num(jv_at(op, 2)), (int)jv_num(jv_at(op, 3)),
                (int)jv_num(jv_at(op, 4)));
    } else if (strcmp(name, "setNext") == 0) {
        lab_set_next(lab, lab_shape_by_name(jv_str(jv_at(op, 1))), (int)jv_num(jv_at(op, 2)));
    } else if (strcmp(name, "respawn") == 0) {
        lab_respawn(lab, lab_shape_by_name(jv_str(jv_at(op, 1))), (int)jv_num(jv_at(op, 2)));
    } else if (strcmp(name, "dueFall") == 0) {
        lab_due_fall(lab);
    } else if (strcmp(name, "poke") == 0) {
        lab_poke(lab, (uint32_t)jv_num(jv_at(op, 1)));
    } else {
        return false;
    }
    for (unsigned i = 0; i < lab->log_count; i++) {
        bk_events_push(pending, (bk_event_type_t)lab->log[i].type, lab->log[i].a, lab->log[i].b, lab->log[i].c);
    }
    lab_log_clear(lab);
    return true;
}

static const conf_game_t TETRIS = {"tetris", sizeof(lab_t), tetris_init, tetris_game, tetris_apply_op};

static void test_scenarios(void)
{
    conf_run_scenarios(&TETRIS, g_root, 60);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/fixtures/tetris/tetris.conformance.json";
    jv_t *root = conf_load(path, "tetris");
    if (root == NULL) {
        return 2;
    }
    g_root = root;

    TEST_RUN(test_rng);
    TEST_RUN(test_catalog);
    TEST_RUN(test_fall_ticks);
    TEST_RUN(test_scenarios);
    jv_free(root);
    return bk_test_finish("tetris_conformance");
}
