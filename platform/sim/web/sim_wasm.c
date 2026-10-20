/*
 * WebAssembly front-end glue: the simulator driver (sim.c) behind a flat C API that JavaScript can call. One simulator
 * instance lives in this module (platform code may have global state; the core does not). The JavaScript side
 * (index.html) owns time (60 Hz), keyboard / pointer input, the canvas, WebAudio and persistence.
 *
 * Everything the page needs is a number or a pointer into linear memory: the shown picture (bit-packed, MSB first),
 * the profile storage slots, and the staging buffer for trace files.
 */
#include "sim.h"

#define EXPORT(name) __attribute__((export_name(#name)))

static bk_sim_t g_sim;

static bk_sim_config_t config(unsigned cell, unsigned w, unsigned h, unsigned fps, unsigned seed)
{
    bk_sim_config_t c;
    c.cell = cell;
    c.width = w;
    c.height = h;
    c.max_fps = (uint8_t)(fps > 255 ? 255 : fps);
    c.seed = seed;
    return c;
}

/* ---- lifecycle ---- */
EXPORT(bk_w_init) int bk_w_init(unsigned cell, unsigned w, unsigned h, unsigned fps, unsigned seed)
{
    bk_sim_config_t c = config(cell, w, h, fps, seed);
    return bk_sim_init(&g_sim, &c);
}

EXPORT(bk_w_configure) int bk_w_configure(unsigned cell, unsigned w, unsigned h, unsigned fps, unsigned seed)
{
    bk_sim_config_t c = config(cell, w, h, fps, seed);
    return bk_sim_configure(&g_sim, &c);
}

EXPORT(bk_w_reset) int bk_w_reset(int wipe_storage)
{
    return bk_sim_reset(&g_sim, wipe_storage != 0);
}

EXPORT(bk_w_set_max_fps) void bk_w_set_max_fps(unsigned fps)
{
    bk_sim_set_max_fps(&g_sim, (uint8_t)(fps > 255 ? 255 : fps));
}

EXPORT(bk_w_set_buttons) void bk_w_set_buttons(unsigned mask)
{
    bk_sim_set_buttons(&g_sim, (bk_buttons_t)mask);
}

EXPORT(bk_w_tick) void bk_w_tick(void)
{
    bk_sim_tick(&g_sim);
}

/* ---- what the panel / speaker / light are doing ---- */
EXPORT(bk_w_frame_seq) unsigned bk_w_frame_seq(void)
{
    return g_sim.frame_seq;
}
EXPORT(bk_w_frame_width) unsigned bk_w_frame_width(void)
{
    const bk_frame_t *f = bk_sim_frame(&g_sim);
    return f != NULL ? f->width : 0;
}
EXPORT(bk_w_frame_height) unsigned bk_w_frame_height(void)
{
    const bk_frame_t *f = bk_sim_frame(&g_sim);
    return f != NULL ? f->height : 0;
}
EXPORT(bk_w_frame_stride) unsigned bk_w_frame_stride(void)
{
    const bk_frame_t *f = bk_sim_frame(&g_sim);
    return f != NULL ? f->stride : 0;
}
EXPORT(bk_w_frame_bits) const unsigned char *bk_w_frame_bits(void)
{
    const bk_frame_t *f = bk_sim_frame(&g_sim);
    return f != NULL ? f->bits : NULL;
}
EXPORT(bk_w_frame_hash) unsigned bk_w_frame_hash(void)
{
    return bk_sim_frame_hash(&g_sim);
}
EXPORT(bk_w_tone_hz) unsigned bk_w_tone_hz(void)
{
    return g_sim.tone_hz;
}
EXPORT(bk_w_light) unsigned bk_w_light(void)
{
    return g_sim.light;
}
EXPORT(bk_w_ble) unsigned bk_w_ble(void)
{
    return g_sim.ble ? 1u : 0u;
}

/* ---- diagnostics ---- */
EXPORT(bk_w_ticks) unsigned bk_w_ticks(void)
{
    return g_sim.console.ticks;
}
EXPORT(bk_w_presents) unsigned bk_w_presents(void)
{
    return g_sim.console.presents;
}
EXPORT(bk_w_dropped) unsigned bk_w_dropped(void)
{
    return g_sim.console.dropped_frames;
}
EXPORT(bk_w_screen) unsigned bk_w_screen(void)
{
    return (unsigned)g_sim.console.screen;
}
EXPORT(bk_w_cell_dots) unsigned bk_w_cell_dots(void)
{
    return g_sim.console.layout.cell;
}
EXPORT(bk_w_max_frame_w) unsigned bk_w_max_frame_w(void)
{
    return BK_FRAME_MAX_W;
}
EXPORT(bk_w_max_frame_h) unsigned bk_w_max_frame_h(void)
{
    return BK_FRAME_MAX_H;
}

/* ---- profile storage (the page keeps it in localStorage) ---- */
EXPORT(bk_w_storage_writes) unsigned bk_w_storage_writes(void)
{
    return g_sim.storage_writes;
}
EXPORT(bk_w_storage_ptr) unsigned char *bk_w_storage_ptr(unsigned slot)
{
    return slot < BK_STORAGE_SLOTS ? g_sim.slot[slot] : NULL;
}
EXPORT(bk_w_storage_len) unsigned bk_w_storage_len(unsigned slot)
{
    return slot < BK_STORAGE_SLOTS ? (unsigned)g_sim.slot_len[slot] : 0;
}
EXPORT(bk_w_storage_set_len) void bk_w_storage_set_len(unsigned slot, unsigned len)
{
    if (slot < BK_STORAGE_SLOTS && len <= BK_STORAGE_SLOT_MAX) {
        g_sim.slot_len[slot] = len;
    }
}
EXPORT(bk_w_storage_slot_max) unsigned bk_w_storage_slot_max(void)
{
    return BK_STORAGE_SLOT_MAX;
}

/* ---- trace record / replay ---- */
EXPORT(bk_w_io_ptr) unsigned char *bk_w_io_ptr(void)
{
    return g_sim.io;
}
EXPORT(bk_w_io_size) unsigned bk_w_io_size(void)
{
    return (unsigned)sizeof g_sim.io;
}
EXPORT(bk_w_record_start) int bk_w_record_start(void)
{
    return bk_sim_record_start(&g_sim);
}
EXPORT(bk_w_record_stop) void bk_w_record_stop(void)
{
    bk_sim_record_stop(&g_sim);
}
EXPORT(bk_w_recording) int bk_w_recording(void)
{
    return g_sim.recording;
}
EXPORT(bk_w_trace_len) unsigned bk_w_trace_len(void)
{
    return (unsigned)g_sim.trace_len;
}
EXPORT(bk_w_trace_overflow) int bk_w_trace_overflow(void)
{
    return g_sim.trace_overflow;
}
EXPORT(bk_w_trace_save) unsigned bk_w_trace_save(void)
{
    return (unsigned)bk_sim_trace_save(&g_sim, g_sim.io, sizeof g_sim.io);
}
EXPORT(bk_w_trace_load) int bk_w_trace_load(unsigned len)
{
    return len <= sizeof g_sim.io && bk_sim_trace_load(&g_sim, g_sim.io, len);
}
EXPORT(bk_w_replaying) int bk_w_replaying(void)
{
    return g_sim.replaying;
}
EXPORT(bk_w_replay_done) int bk_w_replay_done(void)
{
    return bk_sim_replay_done(&g_sim);
}
EXPORT(bk_w_replay_stop) void bk_w_replay_stop(void)
{
    bk_sim_replay_stop(&g_sim);
}
EXPORT(bk_w_cfg_cell) unsigned bk_w_cfg_cell(void)
{
    return g_sim.cfg.cell;
}
EXPORT(bk_w_cfg_width) unsigned bk_w_cfg_width(void)
{
    return g_sim.cfg.width;
}
EXPORT(bk_w_cfg_height) unsigned bk_w_cfg_height(void)
{
    return g_sim.cfg.height;
}
EXPORT(bk_w_cfg_fps) unsigned bk_w_cfg_fps(void)
{
    return g_sim.cfg.max_fps;
}
EXPORT(bk_w_cfg_seed) unsigned bk_w_cfg_seed(void)
{
    return g_sim.cfg.seed;
}
