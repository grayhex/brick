#include "conf_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECKPOINT_EVERY 250

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

static void rows_of(const uint8_t *cells, int width, int height, char *out)
{
    for (int r = 0; r < height; r++) {
        for (int c = 0; c < width; c++) {
            out[r * (width + 1) + c] = ".#*"[cells[r * width + c] % 3];
        }
        out[r * (width + 1) + width] = '\0';
    }
}

static void dump_state(const bk_game_t *g, const bk_event_t *events, unsigned count)
{
    uint8_t cells[BK_CELLS];
    char rows[BK_ROWS * (BK_COLS + 1)];
    bk_game_cells(g, cells);
    rows_of(cells, BK_COLS, BK_ROWS, rows);
    fprintf(stderr, "    C state: tick=%u phase=%d score=%u speed=%u fill=%u events=[", g->tick, (int)g->phase, g->score,
            g->speed, g->over_fill);
    for (unsigned i = 0; i < count; i++) {
        fprintf(stderr, "%s%u(a=%u c=%u)", i ? "," : "", events[i].type, events[i].a, events[i].c);
    }
    fprintf(stderr, "]\n");
    for (int r = 0; r < BK_ROWS; r++) {
        fprintf(stderr, "    |%s|\n", rows + r * (BK_COLS + 1));
    }
}

typedef struct {
    const conf_game_t *cg;
    void *lab;
    bk_event_queue_t pending;
} runner_t;

/* One harness tick; returns false if the C game diverged from the reference. */
static bool replay_tick(runner_t *r, const jv_t *scenario, const char *digests, unsigned index, unsigned code)
{
    bk_game_t *g = r->cg->game_of(r->lab);
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
        dump_state(g, q.items, q.count);
    }
    return ok;
}

static void compare_final(const bk_game_t *g, const jv_t *final)
{
    CHECK_EQ(g->phase, (long long)jv_num(jv_get(final, "phase")));
    CHECK_EQ(g->tick, (long long)jv_num(jv_get(final, "tick")));
    CHECK_EQ(g->score, (long long)jv_num(jv_get(final, "score")));
    CHECK_EQ(g->hi_score, (long long)jv_num(jv_get(final, "hiScore")));
    CHECK_EQ(g->speed, (long long)jv_num(jv_get(final, "speed")));
    CHECK_EQ(g->level, (long long)jv_num(jv_get(final, "level")));
    CHECK_EQ(g->over_fill, (long long)jv_num(jv_get(final, "overFill")));
    uint8_t cells[BK_CELLS];
    char rows[BK_ROWS * (BK_COLS + 1)];
    bk_game_cells(g, cells);
    rows_of(cells, BK_COLS, BK_ROWS, rows);
    const jv_t *expected_rows = jv_get(final, "rows");
    CHECK_EQ(expected_rows->len, BK_ROWS);
    for (int i = 0; i < BK_ROWS; i++) {
        CHECK_STR(rows + i * (BK_COLS + 1), jv_str(jv_at(expected_rows, (size_t)i)));
    }
    uint8_t pcells[BK_PREVIEW_CELLS];
    char pv[BK_PREVIEW_ROWS * (BK_PREVIEW_COLS + 1)];
    bool has_preview = bk_game_preview(g, pcells);
    const jv_t *expected_pv = jv_get(final, "preview");
    CHECK_EQ(has_preview ? 1 : 0, expected_pv->type == JV_ARR ? 1 : 0);
    if (has_preview && expected_pv->type == JV_ARR) {
        rows_of(pcells, BK_PREVIEW_COLS, BK_PREVIEW_ROWS, pv);
        for (int i = 0; i < BK_PREVIEW_ROWS; i++) {
            CHECK_STR(pv + i * (BK_PREVIEW_COLS + 1), jv_str(jv_at(expected_pv, (size_t)i)));
        }
    }
}

static bool run_scenario(const conf_game_t *cg, const jv_t *s, unsigned long long *ticks_done)
{
    const char *name = jv_str(jv_get(s, "name"));
    const jv_t *opt = jv_get(s, "options");
    bk_game_options_t o;
    memset(&o, 0, sizeof o);
    o.seed = (uint32_t)jv_num(jv_get(opt, "seed"));
    o.speed = (uint8_t)jv_num(jv_get(opt, "speed"));
    o.level = (uint8_t)jv_num(jv_get(opt, "level"));
    o.hi_score = (uint32_t)jv_num(jv_get(opt, "hiScore"));
    o.mode = (uint16_t)jv_num(jv_get(opt, "mode"));

    runner_t r;
    memset(&r, 0, sizeof r);
    r.cg = cg;
    r.lab = calloc(1, cg->lab_size);
    CHECK(r.lab != NULL);
    if (r.lab == NULL) {
        return false;
    }
    cg->init(r.lab, &o);
    bk_events_clear(&r.pending);

    const char *digests = jv_str(jv_get(s, "digests"));
    unsigned total = (unsigned)jv_num(jv_get(s, "ticks"));
    CHECK_EQ(strlen(digests), 4u * total);

    bool ok = true;
    const jv_t *trace = jv_get(s, "trace");
    unsigned index = 0;
    for (size_t i = 0; ok && i < trace->len; i++) {
        const jv_t *entry = jv_at(trace, i);
        if (jv_at(entry, 0)->type == JV_STR) {
            if (!cg->apply_op(r.lab, entry, &r.pending)) {
                fprintf(stderr, "unknown fixture op %s\n", jv_str(jv_at(entry, 0)));
                CHECK(0);
                ok = false;
            }
            continue;
        }
        unsigned code = (unsigned)jv_num(jv_at(entry, 0));
        unsigned count = (unsigned)jv_num(jv_at(entry, 1));
        for (unsigned k = 0; k < count; k++, index++) {
            if (!replay_tick(&r, s, digests, index, code)) {
                fprintf(stderr, "  scenario %s diverged\n", name);
                bk_test_failures++;
                ok = false;
                break;
            }
            (*ticks_done)++;
        }
    }
    if (ok) {
        CHECK_EQ(index, total);
        compare_final(cg->game_of(r.lab), jv_get(s, "final"));
    }
    free(r.lab);
    return ok;
}

unsigned conf_run_scenarios(const conf_game_t *cg, const jv_t *root, unsigned min_scenarios)
{
    const jv_t *list = jv_get(root, "scenarios");
    CHECK(list->len >= min_scenarios);
    unsigned long long ticks = 0;
    unsigned ok = 0;
    for (size_t i = 0; i < list->len; i++) {
        bk_test_current = jv_str(jv_get(jv_at(list, i), "name"));
        if (run_scenario(cg, jv_at(list, i), &ticks)) {
            ok++;
        }
    }
    bk_test_current = "scenarios";
    printf("replayed %u/%zu scenarios, %llu ticks, every tick digest-checked\n", ok, list->len, ticks);
    CHECK_EQ(ok, list->len);
    return (unsigned)list->len - ok;
}

jv_t *conf_load(const char *path, const char *game)
{
    char *text = jv_read_file(path);
    if (text == NULL) {
        fprintf(stderr, "cannot read fixture %s\n", path);
        return NULL;
    }
    char err[160];
    jv_t *root = jv_parse(text, err, sizeof err);
    free(text);
    if (root == NULL) {
        fprintf(stderr, "%s\n", err);
        return NULL;
    }
    CHECK_STR(jv_str(jv_get(root, "format")), "brick-conformance/1");
    CHECK_STR(jv_str(jv_get(root, "game")), game);
    printf("reference commit %s\n", jv_str(jv_get(jv_get(root, "reference"), "commit")));
    return root;
}
