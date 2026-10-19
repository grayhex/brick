/*
 * Hardware abstraction: the ONLY interface between the portable core and a board / PC front-end.
 *
 * The core is written against these four narrow ports and never includes a vendor header:
 *   input    - buttons as a held mask, sampled once per tick
 *   display  - receives a finished 1-bit logical frame (scaling, windowing, margins, controller protocol are the driver's)
 *   audio    - one square-wave voice: set a frequency, 0 = silence (speaker only, ADR-007)
 *   storage  - two redundant slots of persistent bytes (flash / file / RAM); wear levelling is the driver's
 * plus a small "system" port (front light, entropy).
 *
 * All callbacks receive the driver context. Every pointer may be NULL (the core then skips that port), which keeps
 * unit tests and headless runs trivial. Nothing here assumes an MCU, a panel type or a board.
 */
#ifndef BRICK_HAL_H
#define BRICK_HAL_H

#include "brick/frame.h"
#include "brick/input.h"

BK_EXTERN_C_BEGIN

typedef struct {
    void *ctx;
    /* Held buttons right now (bit per bk_button_t). Debouncing is the driver's job. */
    bk_buttons_t (*read_buttons)(void *ctx);
} bk_input_port_t;

typedef struct {
    void *ctx;
    /* Show this frame. Called only when the picture changed, so a slow or power-hungry panel is not refreshed needlessly.
     * The frame is positive polarity (ink = dark) and is only valid for the duration of the call. */
    void (*present)(void *ctx, const bk_frame_t *frame);
} bk_display_port_t;

typedef struct {
    void *ctx;
    /* Output a square wave of `hz` (0 = silent). Volume is the physical control and is not seen by the core. */
    void (*set_tone)(void *ctx, uint16_t hz);
} bk_audio_port_t;

#define BK_STORAGE_SLOTS    2
#define BK_STORAGE_SLOT_MAX 1024 /* bytes per slot the core may write */

typedef struct {
    void *ctx;
    /* Read slot -> buf (up to cap bytes). *len = bytes stored (0 if never written). Returns 0 on success. */
    int (*read)(void *ctx, unsigned slot, uint8_t *buf, size_t cap, size_t *len);
    /* Replace slot content. The driver must make this robust against power loss as far as it can (the core keeps the
     * previous slot intact and validates a CRC on load). Returns 0 on success. */
    int (*write)(void *ctx, unsigned slot, const uint8_t *buf, size_t len);
} bk_storage_port_t;

typedef struct {
    void *ctx;
    /* Front light level: 0 = off, 1..BK_LIGHT_MAX = increasing. Mapping to PWM / hardware is the driver's. */
    void (*set_frontlight)(void *ctx, uint8_t level);
    /* Any 32 bits of entropy for game seeds (timer jitter, ADC noise, RTC...). May be NULL: a fixed seed is used. */
    uint32_t (*entropy)(void *ctx);
} bk_system_port_t;

#define BK_LIGHT_MAX 2

typedef struct {
    bk_input_port_t input;
    bk_display_port_t display;
    bk_audio_port_t audio;
    bk_storage_port_t storage;
    bk_system_port_t system;
} bk_hal_t;

BK_EXTERN_C_END

#endif /* BRICK_HAL_H */
