#include "stub_game.h"

#include <string.h>

static void stub_reset(bk_game_t *g)
{
    stub_game_t *s = (stub_game_t *)g;
    s->steps = 0;
    s->rotate_presses = 0;
}

static void stub_update(bk_game_t *g, const bk_input_t *in)
{
    stub_game_t *s = (stub_game_t *)g;
    s->steps++;
    s->last_held = in->held;
    if (in->pressed & BK_KEY_ROTATE) {
        s->rotate_presses++;
        bk_game_add_score(g, 10);
        bk_game_emit(g, BK_EV_ROTATE, 0, 0, 0);
    }
    if (g->tick % 30 == 0) {
        bk_game_add_score(g, 1);
    }
    if (g->score >= STUB_GOAL + (uint32_t)g->mode) {
        bk_game_game_over(g);
    }
}

static void stub_draw(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    memset(cells, 0, BK_CELLS);
    cells[(BK_ROWS - 1) * BK_COLS + (g->tick % BK_COLS)] = BK_CELL_ON;
    cells[0] = BK_CELL_BLINK;
}

static const bk_game_vtable_t STUB_VT = {stub_reset, stub_update, stub_draw, NULL, NULL, NULL, NULL};

static bk_game_t *stub_create_multi(void *storage, const bk_game_options_t *opt)
{
    stub_game_t *s = (stub_game_t *)storage;
    memset(s, 0, sizeof *s);
    bk_game_base_init(&s->base, &STUB_VT, opt, 3);
    s->created_seed = opt->seed;
    bk_game_restart(&s->base);
    return &s->base;
}

static bk_game_t *stub_create_single(void *storage, const bk_game_options_t *opt)
{
    stub_game_t *s = (stub_game_t *)storage;
    memset(s, 0, sizeof *s);
    bk_game_base_init(&s->base, &STUB_VT, opt, 1);
    s->created_seed = opt->seed;
    bk_game_restart(&s->base);
    return &s->base;
}

static void stub_label(uint16_t mode, char *buf, size_t cap)
{
    if (cap >= 8) {
        memcpy(buf, "MODE ", 5);
        buf[5] = (char)('1' + mode);
        buf[6] = '\0';
    }
}

static void stub_thumb(uint8_t frame, uint8_t cells[BK_COLS * 5])
{
    (void)frame;
    memset(cells, 0, BK_COLS * 5);
    for (int i = 0; i < BK_COLS; i++) {
        cells[(i % 5) * BK_COLS + i] = BK_CELL_ON; /* diagonal */
    }
}

const bk_game_def_t stub_def_multi = {
    100, "stub-multi", "MULTI", 3, 0, sizeof(stub_game_t), _Alignof(stub_game_t), stub_create_multi, stub_label, 1,
    0,   stub_thumb};
const bk_game_def_t stub_def_single = {
    101, "stub-single", "SINGLE", 1, 0, sizeof(stub_game_t), _Alignof(stub_game_t), stub_create_single, NULL, 0, 0, NULL};
