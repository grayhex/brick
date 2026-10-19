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

#include "bk_digest.h"
#include "bk_test.h"
#include "mini_json.h"
#include "tetris_lab.h"

#define CHECKPOINT_EVERY 250

static const jv_t *g_root;

static unsigned hex_digit(char c)
{
    return (unsigned)(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
}

static unsigned hex_at(const char *s, int digits)
{
    unsigned v = 0;
    for (int i = 0; i < digits; i++) {
        v = v * 16u + hex_digit(s[i]);
    }
    return v;
}

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

static void dump_state(lab_t *lab, const bk_event_t *events, unsigned count)
{
    char rows[BK_ROWS][BK_COLS + 1];
    lab_rows(lab, rows);
    const bk_game_t *g = lab_g(lab);
    fprintf(stderr, "    C state: tick=%u phase=%d score=%u speed=%u fill=%u events=[", g->tick, (int)g->phase, g->score,
            g->speed, g->over_fill);
    for (unsigned i = 0; i < count; i++) {
        fprintf(stderr, "%s%u(a=%u c=%u)", i ? "," : "", events[i].type, events[i].a, events[i].c);
    }
    fprintf(stderr, "]\n");
    for (int r = 0; r < BK_ROWS; r++) {
        fprintf(stderr, "    |%s|\n", rows[r]);
    }
}

typedef struct {
    lab_t lab;
    bk_event_queue_t pending; /* events raised by setup ops, delivered with the next plain step (as the reference queue) */
} runner_t;

static void apply_op(runner_t *r, const jv_t *op)
{
    const char *name = jv_str(jv_at(op, 0));
    lab_t *lab = &r->lab;
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
        fprintf(stderr, "unknown fixture op %s\n", name);
        CHECK(0);
    }
    for (unsigned i = 0; i < lab->log_count; i++) {
        bk_events_push(&r->pending, (bk_event_type_t)lab->log[i].type, lab->log[i].a, lab->log[i].b, lab->log[i].c);
    }
    lab_log_clear(lab);
}

/* One harness tick; returns false if the C game diverged from the reference. */
static bool replay_tick(runner_t *r, const jv_t *scenario, const char *digests, unsigned index, unsigned code)
{
    bk_game_t *g = lab_g(&r->lab);
    unsigned cmd = code >> 12;
    bk_event_queue_t q;
    bk_events_clear(&q);
    if (r->pending.count > 0) {
        if (cmd != 0 || g->phase != BK_PHASE_PLAYING) {
            fprintf(stderr, "  fixture error: pending setup events need a plain step\n");
            return false;
        }
        q = r->pending;
        bk_events_clear(&r->pending);
    }
    switch (cmd) {
    case 1:
        bk_game_start(g, &q);
        break;
    case 2:
        bk_game_pause(g, &q);
        break;
    case 3:
        bk_game_resume(g, &q);
        break;
    case 4:
        bk_game_restart(g);
        break;
    default:
        break;
    }
    bk_input_t in = bk_input_make((uint8_t)((code >> 6) & 63u), (uint8_t)(code & 63u));
    bk_game_step(g, &in, &q);
    if (q.dropped != 0) {
        fprintf(stderr, "  event queue overflow at tick %u\n", index);
        return false;
    }

    uint32_t h = bk_digest_state(g, q.items, q.count);
    unsigned expected = hex_at(digests + 4u * index, 4);
    bool ok = bk_digest_fold16(h) == expected;
    const jv_t *cps = jv_get(scenario, "checkpoints");
    if (ok && (index + 1u) % CHECKPOINT_EVERY == 0u) {
        const jv_t *cp = jv_at(cps, (index + 1u) / CHECKPOINT_EVERY - 1u);
        ok = (uint32_t)jv_num(jv_get(cp, "h")) == h && (unsigned)jv_num(jv_get(cp, "i")) == index;
    }
    if (!ok) {
        fprintf(stderr, "  MISMATCH at tick index %u: C digest %04x, reference %04x\n", index, bk_digest_fold16(h),
                expected);
        dump_state(&r->lab, q.items, q.count);
    }
    return ok;
}

static void compare_final(runner_t *r, const jv_t *final)
{
    const bk_game_t *g = lab_g(&r->lab);
    CHECK_EQ(g->phase, (long long)jv_num(jv_get(final, "phase")));
    CHECK_EQ(g->tick, (long long)jv_num(jv_get(final, "tick")));
    CHECK_EQ(g->score, (long long)jv_num(jv_get(final, "score")));
    CHECK_EQ(g->hi_score, (long long)jv_num(jv_get(final, "hiScore")));
    CHECK_EQ(g->speed, (long long)jv_num(jv_get(final, "speed")));
    CHECK_EQ(g->level, (long long)jv_num(jv_get(final, "level")));
    CHECK_EQ(g->over_fill, (long long)jv_num(jv_get(final, "overFill")));
    char rows[BK_ROWS][BK_COLS + 1];
    lab_rows(&r->lab, rows);
    const jv_t *expected_rows = jv_get(final, "rows");
    CHECK_EQ(expected_rows->len, BK_ROWS);
    for (int i = 0; i < BK_ROWS; i++) {
        CHECK_STR(rows[i], jv_str(jv_at(expected_rows, (size_t)i)));
    }
    char pv[BK_PREVIEW_ROWS][BK_PREVIEW_COLS + 1];
    lab_preview_rows(&r->lab, pv);
    const jv_t *expected_pv = jv_get(final, "preview");
    for (int i = 0; i < BK_PREVIEW_ROWS; i++) {
        CHECK_STR(pv[i], jv_str(jv_at(expected_pv, (size_t)i)));
    }
}

static bool run_scenario(const jv_t *s, unsigned long long *ticks_done)
{
    const char *name = jv_str(jv_get(s, "name"));
    const jv_t *opt = jv_get(s, "options");
    bk_game_options_t o = lab_options((uint32_t)jv_num(jv_get(opt, "seed")), (unsigned)jv_num(jv_get(opt, "speed")),
                                      (unsigned)jv_num(jv_get(opt, "level")), (uint32_t)jv_num(jv_get(opt, "hiScore")));
    static runner_t r; /* large; keep off the stack */
    memset(&r, 0, sizeof r);
    lab_init(&r.lab, &o);
    bk_events_clear(&r.pending);

    const char *digests = jv_str(jv_get(s, "digests"));
    unsigned total = (unsigned)jv_num(jv_get(s, "ticks"));
    CHECK_EQ(strlen(digests), 4u * total);

    const jv_t *trace = jv_get(s, "trace");
    unsigned index = 0;
    for (size_t i = 0; i < trace->len; i++) {
        const jv_t *entry = jv_at(trace, i);
        if (jv_at(entry, 0)->type == JV_STR) {
            apply_op(&r, entry);
            continue;
        }
        unsigned code = (unsigned)jv_num(jv_at(entry, 0));
        unsigned count = (unsigned)jv_num(jv_at(entry, 1));
        for (unsigned k = 0; k < count; k++, index++) {
            if (!replay_tick(&r, s, digests, index, code)) {
                fprintf(stderr, "  scenario %s diverged\n", name);
                bk_test_failures++;
                return false;
            }
            (*ticks_done)++;
        }
    }
    CHECK_EQ(index, total);
    compare_final(&r, jv_get(s, "final"));
    return true;
}

static void test_scenarios(void)
{
    const jv_t *list = jv_get(g_root, "scenarios");
    CHECK(list->len >= 60);
    unsigned long long ticks = 0;
    unsigned ok = 0;
    for (size_t i = 0; i < list->len; i++) {
        bk_test_current = jv_str(jv_get(jv_at(list, i), "name"));
        if (run_scenario(jv_at(list, i), &ticks)) {
            ok++;
        }
    }
    bk_test_current = "test_scenarios";
    printf("replayed %u/%zu scenarios, %llu ticks, every tick digest-checked\n", ok, list->len, ticks);
    CHECK_EQ(ok, list->len);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/fixtures/tetris/tetris.conformance.json";
    char *text = jv_read_file(path);
    if (text == NULL) {
        fprintf(stderr, "cannot read fixture %s\n", path);
        return 2;
    }
    char err[160];
    jv_t *root = jv_parse(text, err, sizeof err);
    free(text);
    if (root == NULL) {
        fprintf(stderr, "%s\n", err);
        return 2;
    }
    g_root = root;
    CHECK_STR(jv_str(jv_get(root, "format")), "brick-conformance/1");
    CHECK_STR(jv_str(jv_get(root, "game")), "tetris");
    printf("reference commit %s\n", jv_str(jv_get(jv_get(root, "reference"), "commit")));

    TEST_RUN(test_rng);
    TEST_RUN(test_catalog);
    TEST_RUN(test_fall_ticks);
    TEST_RUN(test_scenarios);
    jv_free(root);
    return bk_test_finish("tetris_conformance");
}
