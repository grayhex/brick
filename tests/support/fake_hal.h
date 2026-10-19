/* Recording HAL for tests: no I/O, everything observable. */
#ifndef BK_FAKE_HAL_H
#define BK_FAKE_HAL_H

#include "brick/hal.h"

#define FAKE_TONE_LOG 512

typedef struct {
    bk_hal_t hal;

    bk_buttons_t buttons; /* returned by read_buttons */

    unsigned presents;     /* display present calls */
    bk_frame_t last_frame; /* copy of the last presented frame */

    uint16_t tones[FAKE_TONE_LOG]; /* every set_tone call, in order */
    unsigned tone_count;
    uint16_t tone_now;

    uint8_t light;
    unsigned light_calls;

    uint8_t slot[BK_STORAGE_SLOTS][BK_STORAGE_SLOT_MAX];
    size_t slot_len[BK_STORAGE_SLOTS];
    unsigned writes;
    int fail_writes; /* when non-zero storage.write returns an error and stores nothing */

    uint32_t entropy_value;
} fake_hal_t;

void fake_hal_init(fake_hal_t *f);
void fake_hal_clear_tones(fake_hal_t *f);

#endif
