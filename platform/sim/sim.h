/*
 * PC simulator driver: the console of firmware/ wired to an "invisible" board. It owns the HAL (buttons, panel, speaker,
 * front light, profile storage in RAM) and exposes the state a front-end needs: the picture the panel was last given,
 * the tone, the light level. It does not know about windows, terminals or browsers; those are thin front-ends
 * (platform/sim/term, platform/sim/web) that only call this API. No heap, no OS calls: the same file builds natively
 * and for WebAssembly.
 *
 * Determinism: the simulation advances only through bk_sim_tick() (60 Hz of simulated time), button input is a mask
 * sampled once per tick, and the entropy is cfg.seed, so the same config + the same trace gives the same pictures.
 *
 * Trace file (record / replay), little-endian:
 *     "BKT1" | u16 width | u16 height | u8 cell | u8 max_fps | u32 seed | u32 ticks | ticks x u8 button mask
 * width/height 0 = reference design grid of `cell` dots, otherwise a layout fitted to that native panel size. The
 * profile storage is wiped when a trace starts, so a replay begins from the same factory state as the recording did
 * (bk_sim_reset with wipe_storage = true before bk_sim_record_start).
 */
#ifndef BRICK_SIM_H
#define BRICK_SIM_H

#include "brick/console.h"

BK_EXTERN_C_BEGIN

#ifndef BK_SIM_TRACE_MAX
#define BK_SIM_TRACE_MAX 72000 /* ticks: 20 minutes at 60 Hz */
#endif
#define BK_SIM_TRACE_HEADER 18

typedef struct {
    unsigned cell;  /* reference grid (even, 8..16) when width or height is 0 */
    unsigned width; /* native panel size to fit (bk_layout_fit) when both are non-zero */
    unsigned height;
    uint8_t max_fps; /* highest rate at which the "panel" accepts new pictures, 0 = every tick (ADR-030) */
    uint32_t seed;   /* entropy returned to the console: seeds the games */
} bk_sim_config_t;

typedef struct {
    bk_console_t console;
    bk_hal_t hal;
    bk_sim_config_t cfg;

    bk_buttons_t buttons;   /* what the front-end currently holds down */
    bk_buttons_t last_read; /* what the console read in the last tick */

    uint32_t frame_seq; /* pictures given to the panel since the last reset */
    uint16_t tone_hz;   /* current square-wave frequency, 0 = silent */
    uint32_t tone_seq;  /* number of tone changes since the last reset */
    uint8_t light;      /* front light level 0..BK_LIGHT_MAX */
    bool ble;           /* the radio the console asked for (setting BLE); there is no radio in the simulator */

    uint8_t slot[BK_STORAGE_SLOTS][BK_STORAGE_SLOT_MAX];
    size_t slot_len[BK_STORAGE_SLOTS];
    uint32_t storage_writes; /* never reset: lets a front-end persist the profile when it changes */

    uint8_t trace[BK_SIM_TRACE_MAX];
    size_t trace_len;
    uint8_t io[BK_SIM_TRACE_HEADER + BK_SIM_TRACE_MAX]; /* staging area for front-ends that cannot pass pointers */
    bool recording;
    bool trace_overflow;
    bool replaying;
    size_t replay_pos;
} bk_sim_t;

/* Returns false if the configuration cannot be used (odd cell size, panel that is too small or larger than the frame
 * buffer BK_FRAME_MAX_W x BK_FRAME_MAX_H, ...); the simulator is then unusable until a valid bk_sim_init. */
bool bk_sim_init(bk_sim_t *s, const bk_sim_config_t *cfg);

/* Reboot the console with the current config (tick 0, splash). Profile storage survives unless wipe_storage. Stops a
 * recording or replay in progress. */
bool bk_sim_reset(bk_sim_t *s, bool wipe_storage);

/* New config = new "device": reboots; storage survives. */
bool bk_sim_configure(bk_sim_t *s, const bk_sim_config_t *cfg);

/* Panel refresh limit can change at any time without rebooting. */
void bk_sim_set_max_fps(bk_sim_t *s, uint8_t max_fps);

void bk_sim_set_buttons(bk_sim_t *s, bk_buttons_t held);

/* Advance one 60 Hz tick. */
void bk_sim_tick(bk_sim_t *s);

/* The picture the panel has been given last, NULL until the first present. */
const bk_frame_t *bk_sim_frame(const bk_sim_t *s);

/* FNV-1a over the shown picture (size and bits); 0 if nothing was shown. */
uint32_t bk_sim_frame_hash(const bk_sim_t *s);

/* ---- record / replay ---- */
bool bk_sim_record_start(bk_sim_t *s); /* from the current tick on; the trace holds one button mask per tick */
void bk_sim_record_stop(bk_sim_t *s);
size_t bk_sim_trace_save(const bk_sim_t *s, uint8_t *out, size_t cap); /* bytes written, 0 if cap is too small */
/* Parses a trace file image, applies its config, reboots with wiped storage and starts replaying it. `in` may be
 * s->io. On error nothing changes. */
bool bk_sim_trace_load(bk_sim_t *s, const uint8_t *in, size_t len);
bool bk_sim_replay_done(const bk_sim_t *s);
void bk_sim_replay_stop(bk_sim_t *s); /* hand the buttons back to the front-end */

BK_EXTERN_C_END

#endif /* BRICK_SIM_H */
