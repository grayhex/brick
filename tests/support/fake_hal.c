#include "fake_hal.h"

#include <string.h>

static bk_buttons_t f_read(void *ctx)
{
    return ((fake_hal_t *)ctx)->buttons;
}

static void f_present(void *ctx, const bk_frame_t *frame)
{
    fake_hal_t *f = (fake_hal_t *)ctx;
    f->presents++;
    f->last_frame.width = frame->width;
    f->last_frame.height = frame->height;
    f->last_frame.stride = frame->stride;
    memcpy(f->last_frame.bits, frame->bits, bk_frame_bytes(frame));
}

static void f_tone(void *ctx, uint16_t hz)
{
    fake_hal_t *f = (fake_hal_t *)ctx;
    if (f->tone_count < FAKE_TONE_LOG) {
        f->tones[f->tone_count++] = hz;
    }
    f->tone_now = hz;
}

static int f_storage_read(void *ctx, unsigned slot, uint8_t *buf, size_t cap, size_t *len)
{
    fake_hal_t *f = (fake_hal_t *)ctx;
    size_t n = f->slot_len[slot] < cap ? f->slot_len[slot] : cap;
    memcpy(buf, f->slot[slot], n);
    *len = n;
    return 0;
}

static int f_storage_write(void *ctx, unsigned slot, const uint8_t *buf, size_t len)
{
    fake_hal_t *f = (fake_hal_t *)ctx;
    if (f->fail_writes) {
        return -1;
    }
    memcpy(f->slot[slot], buf, len);
    f->slot_len[slot] = len;
    f->writes++;
    return 0;
}

static void f_light(void *ctx, uint8_t level)
{
    fake_hal_t *f = (fake_hal_t *)ctx;
    f->light = level;
    f->light_calls++;
}

static uint32_t f_entropy(void *ctx)
{
    return ((fake_hal_t *)ctx)->entropy_value;
}

void fake_hal_init(fake_hal_t *f)
{
    memset(f, 0, sizeof *f);
    f->entropy_value = 1234;
    f->hal.input.ctx = f;
    f->hal.input.read_buttons = f_read;
    f->hal.display.ctx = f;
    f->hal.display.present = f_present;
    f->hal.audio.ctx = f;
    f->hal.audio.set_tone = f_tone;
    f->hal.storage.ctx = f;
    f->hal.storage.read = f_storage_read;
    f->hal.storage.write = f_storage_write;
    f->hal.system.ctx = f;
    f->hal.system.set_frontlight = f_light;
    f->hal.system.entropy = f_entropy;
}

void fake_hal_clear_tones(fake_hal_t *f)
{
    f->tone_count = 0;
}
