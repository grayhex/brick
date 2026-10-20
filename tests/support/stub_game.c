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

/* A dot walks along the bottom row of the preview: four frames, 20 ticks each, the poster is the second one. */
// clang-format off
static const char STUB_CLIP_FRAMES[][BK_CLIP_CELLS + 1] = {
    ".........." ".........." ".........." ".........." "#.........",
    ".........." ".........." ".........." ".........." ".#........",
    ".........." ".........." ".........." ".........." "..#.......",
    ".........." ".........." ".........." ".........." "...#......",
};
// clang-format on
const bk_clip_t stub_clip = {(uint16_t)BK_ARRAY_LEN(STUB_CLIP_FRAMES), STUB_CLIP_TICKS, 1, STUB_CLIP_FRAMES};

const bk_game_def_t stub_def_multi = {
    100, "stub-multi", "MULTI", 3, 0, sizeof(stub_game_t), _Alignof(stub_game_t), stub_create_multi, &stub_clip};
const bk_game_def_t stub_def_single = {
    101, "stub-single", "SINGLE", 1, 0, sizeof(stub_game_t), _Alignof(stub_game_t), stub_create_single, NULL};
