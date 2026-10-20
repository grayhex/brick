#include "sim.h"

#include "brick/catalog.h"

#include <string.h>

static bk_buttons_t sim_read_buttons(void *ctx)
{
    bk_sim_t *s = ctx;
    bk_buttons_t b = s->buttons;
    if (s->replaying) {
        b = s->replay_pos < s->trace_len ? s->trace[s->replay_pos] : 0;
    }
    s->last_read = b;
    return b;
}

static void sim_present(void *ctx, const bk_frame_t *frame)
{
    (void)frame; /* the console keeps its own copy in console.shown */
    bk_sim_t *s = ctx;
    s->frame_seq++;
}

static void sim_set_tone(void *ctx, uint16_t hz)
{
    bk_sim_t *s = ctx;
    if (s->tone_hz != hz) {
        s->tone_hz = hz;
        s->tone_seq++;
    }
}

static void sim_set_frontlight(void *ctx, uint8_t level)
{
    bk_sim_t *s = ctx;
    s->light = level;
}

static uint32_t sim_entropy(void *ctx)
{
    const bk_sim_t *s = ctx;
    return s->cfg.seed;
}

static int sim_storage_read(void *ctx, unsigned slot, uint8_t *buf, size_t cap, size_t *len)
{
    bk_sim_t *s = ctx;
    if (slot >= BK_STORAGE_SLOTS) {
        return -1;
    }
    size_t n = s->slot_len[slot] < cap ? s->slot_len[slot] : cap;
    memcpy(buf, s->slot[slot], n);
    *len = s->slot_len[slot];
    return 0;
}

static int sim_storage_write(void *ctx, unsigned slot, const uint8_t *data, size_t len)
{
    bk_sim_t *s = ctx;
    if (slot >= BK_STORAGE_SLOTS || len > BK_STORAGE_SLOT_MAX) {
        return -1;
    }
    memcpy(s->slot[slot], data, len);
    s->slot_len[slot] = len;
    s->storage_writes++;
    return 0;
}

static bool sim_layout(const bk_sim_config_t *cfg, bk_layout_t *l)
{
    if (cfg->width != 0 && cfg->height != 0) {
        return bk_layout_fit(l, cfg->width, cfg->height);
    }
    return bk_layout_init(l, cfg->cell);
}

static void sim_wire(bk_sim_t *s)
{
    memset(&s->hal, 0, sizeof s->hal);
    s->hal.input.ctx = s;
    s->hal.input.read_buttons = sim_read_buttons;
    s->hal.display.ctx = s;
    s->hal.display.present = sim_present;
    s->hal.display.max_fps = s->cfg.max_fps;
    s->hal.audio.ctx = s;
    s->hal.audio.set_tone = sim_set_tone;
    s->hal.storage.ctx = s;
    s->hal.storage.read = sim_storage_read;
    s->hal.storage.write = sim_storage_write;
    s->hal.system.ctx = s;
    s->hal.system.set_frontlight = sim_set_frontlight;
    s->hal.system.entropy = sim_entropy;
}

bool bk_sim_reset(bk_sim_t *s, bool wipe_storage)
{
    bk_layout_t layout;
    if (!sim_layout(&s->cfg, &layout)) {
        return false;
    }
    if (wipe_storage) {
        memset(s->slot, 0, sizeof s->slot);
        memset(s->slot_len, 0, sizeof s->slot_len);
    }
    s->buttons = 0;
    s->last_read = 0;
    s->frame_seq = 0;
    s->tone_hz = 0;
    s->tone_seq = 0;
    s->light = 0;
    s->trace_len = 0;
    s->recording = false;
    s->trace_overflow = false;
    s->replaying = false;
    s->replay_pos = 0;
    sim_wire(s);
    size_t count = 0;
    const bk_catalog_entry_t *catalog = bk_catalog(&count);
    return bk_console_init_layout(&s->console, catalog, count, &s->hal, &layout);
}

bool bk_sim_init(bk_sim_t *s, const bk_sim_config_t *cfg)
{
    memset(s, 0, sizeof *s);
    s->cfg = *cfg;
    return bk_sim_reset(s, true);
}

bool bk_sim_configure(bk_sim_t *s, const bk_sim_config_t *cfg)
{
    bk_layout_t layout;
    if (!sim_layout(cfg, &layout)) {
        return false;
    }
    s->cfg = *cfg;
    return bk_sim_reset(s, false);
}

void bk_sim_set_max_fps(bk_sim_t *s, uint8_t max_fps)
{
    s->cfg.max_fps = max_fps;
    s->hal.display.max_fps = max_fps;
}

void bk_sim_set_buttons(bk_sim_t *s, bk_buttons_t held)
{
    s->buttons = held;
}

void bk_sim_tick(bk_sim_t *s)
{
    bk_console_tick(&s->console);
    if (s->recording) {
        if (s->trace_len < BK_SIM_TRACE_MAX) {
            s->trace[s->trace_len++] = (uint8_t)s->last_read;
        } else {
            s->trace_overflow = true;
        }
    }
    if (s->replaying && s->replay_pos < s->trace_len) {
        s->replay_pos++;
    }
}

const bk_frame_t *bk_sim_frame(const bk_sim_t *s)
{
    return s->console.shown_valid ? &s->console.shown : NULL;
}

uint32_t bk_sim_frame_hash(const bk_sim_t *s)
{
    const bk_frame_t *f = bk_sim_frame(s);
    if (f == NULL) {
        return 0;
    }
    uint32_t h = 2166136261u;
    uint8_t dims[4] = {(uint8_t)f->width, (uint8_t)(f->width >> 8), (uint8_t)f->height, (uint8_t)(f->height >> 8)};
    for (size_t i = 0; i < sizeof dims; i++) {
        h = (h ^ dims[i]) * 16777619u;
    }
    for (size_t i = 0, n = bk_frame_bytes(f); i < n; i++) {
        h = (h ^ f->bits[i]) * 16777619u;
    }
    return h;
}

bool bk_sim_record_start(bk_sim_t *s)
{
    if (s->replaying) {
        return false;
    }
    s->trace_len = 0;
    s->trace_overflow = false;
    s->recording = true;
    return true;
}

void bk_sim_record_stop(bk_sim_t *s)
{
    s->recording = false;
}

static void put16(uint8_t *p, unsigned v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    put16(p, v & 0xffffu);
    put16(p + 2, v >> 16);
}

static unsigned get16(const uint8_t *p)
{
    return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
    return get16(p) | ((uint32_t)get16(p + 2) << 16);
}

size_t bk_sim_trace_save(const bk_sim_t *s, uint8_t *out, size_t cap)
{
    size_t need = BK_SIM_TRACE_HEADER + s->trace_len;
    if (cap < need) {
        return 0;
    }
    memcpy(out, "BKT1", 4);
    put16(out + 4, s->cfg.width);
    put16(out + 6, s->cfg.height);
    out[8] = (uint8_t)s->cfg.cell;
    out[9] = s->cfg.max_fps;
    put32(out + 10, s->cfg.seed);
    put32(out + 14, (uint32_t)s->trace_len);
    memcpy(out + BK_SIM_TRACE_HEADER, s->trace, s->trace_len);
    return need;
}

bool bk_sim_trace_load(bk_sim_t *s, const uint8_t *in, size_t len)
{
    if (len < BK_SIM_TRACE_HEADER || memcmp(in, "BKT1", 4) != 0) {
        return false;
    }
    uint32_t ticks = get32(in + 14);
    if (ticks > BK_SIM_TRACE_MAX || len != BK_SIM_TRACE_HEADER + (size_t)ticks) {
        return false;
    }
    bk_sim_config_t cfg;
    cfg.width = get16(in + 4);
    cfg.height = get16(in + 6);
    cfg.cell = in[8];
    cfg.max_fps = in[9];
    cfg.seed = get32(in + 10);
    bk_layout_t layout;
    if (!sim_layout(&cfg, &layout)) {
        return false;
    }
    memmove(s->trace, in + BK_SIM_TRACE_HEADER, ticks);
    s->cfg = cfg;
    if (!bk_sim_reset(s, true)) {
        return false;
    }
    s->trace_len = ticks;
    s->replaying = true;
    return true;
}

bool bk_sim_replay_done(const bk_sim_t *s)
{
    return s->replaying && s->replay_pos >= s->trace_len;
}

void bk_sim_replay_stop(bk_sim_t *s)
{
    s->replaying = false;
}
