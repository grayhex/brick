/*
 * Conformance of the C Racing port against recorded traces of the TypeScript reference: after EVERY tick a digest of the
 * complete observable state must equal the one the reference produced (tests/support/conf_runner.c,
 * docs/CONFORMANCE.md). Regenerate the fixture with `npm run export:racing` in tools/conformance.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "racing_lab.h"

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
            CHECK_STR(jv_str(jv_at(op, 0)), "int");
            CHECK_EQ(bk_rng_int(&rng, (uint32_t)jv_num(jv_at(op, 1))), (long long)jv_num(jv_at(op, 2)));
        }
    }
}

static void test_half_step_ticks(void)
{
    const jv_t *rows = jv_get(g_root, "halfStepTicks");
    CHECK_EQ(rows->len, 10);
    for (int speed = 1; speed <= 10; speed++) {
        const jv_t *row = jv_at(rows, (size_t)speed - 1);
        CHECK_EQ(row->len, 10);
        for (int level = 1; level <= 10; level++) {
            /* exact: the same rounding of the same IEEE doubles as in the reference */
            CHECK_EQ(bkr_half_step_ticks(speed, level), (long long)jv_num(jv_at(row, (size_t)level - 1)));
        }
    }
}

static void test_scenarios(void)
{
    conf_run_scenarios(&RACING_CONF, g_root, 60);
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/fixtures/racing/racing.conformance.json";
    jv_t *root = conf_load(path, "racing");
    if (root == NULL) {
        return 2;
    }
    g_root = root;
    TEST_RUN(test_rng);
    TEST_RUN(test_half_step_ticks);
    TEST_RUN(test_scenarios);
    jv_free(root);
    return bk_test_finish("racing_conformance");
}
