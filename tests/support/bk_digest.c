#include "bk_digest.h"

typedef struct {
    uint32_t h;
} fnv_t;

static void u8(fnv_t *f, uint32_t v)
{
    f->h = (f->h ^ (v & 0xFFu)) * 0x01000193u;
}

static void u16(fnv_t *f, uint32_t v)
{
    u8(f, v);
    u8(f, v >> 8);
}

static void u32(fnv_t *f, uint32_t v)
{
    u16(f, v);
    u16(f, v >> 16);
}

uint32_t bk_digest_state(const bk_game_t *g, const bk_event_t *events, unsigned count)
{
    fnv_t f = {0x811c9dc5u};
    u32(&f, g->tick);
    u8(&f, (uint32_t)g->phase);
    u32(&f, g->score);
    u32(&f, g->hi_score);
    u8(&f, g->speed);
    u8(&f, g->level);
    u8(&f, g->lives < 0 ? 255u : (uint32_t)g->lives);
    u8(&f, g->over_fill);
    uint8_t cells[BK_CELLS];
    bk_game_cells(g, cells);
    for (int i = 0; i < BK_CELLS; i++) {
        u8(&f, cells[i]);
    }
    uint8_t preview[BK_PREVIEW_CELLS];
    bool has_preview = bk_game_preview(g, preview);
    u8(&f, has_preview ? 1u : 0u);
    if (has_preview) {
        for (int i = 0; i < BK_PREVIEW_CELLS; i++) {
            u8(&f, preview[i]);
        }
    }
    u8(&f, count);
    for (unsigned i = 0; i < count; i++) {
        u8(&f, events[i].type);
        u8(&f, events[i].a);
        u16(&f, events[i].b);
        u32(&f, events[i].c);
    }
    return f.h;
}
