#include "brick/frame.h"

#include <string.h>

void bk_frame_init(bk_frame_t *f, uint16_t width, uint16_t height)
{
    if (width > BK_FRAME_MAX_W) {
        width = BK_FRAME_MAX_W;
    }
    if (height > BK_FRAME_MAX_H) {
        height = BK_FRAME_MAX_H;
    }
    f->width = width;
    f->height = height;
    f->stride = (uint16_t)((width + 7u) / 8u);
    bk_frame_clear(f);
}

void bk_frame_clear(bk_frame_t *f)
{
    memset(f->bits, 0, bk_frame_bytes(f));
}

size_t bk_frame_bytes(const bk_frame_t *f)
{
    return (size_t)f->stride * f->height;
}

bool bk_frame_get(const bk_frame_t *f, int x, int y)
{
    if (x < 0 || y < 0 || x >= f->width || y >= f->height) {
        return false;
    }
    return (f->bits[(size_t)y * f->stride + (unsigned)x / 8u] & (0x80u >> ((unsigned)x % 8u))) != 0u;
}

void bk_frame_set(bk_frame_t *f, int x, int y, bool ink)
{
    if (x < 0 || y < 0 || x >= f->width || y >= f->height) {
        return;
    }
    uint8_t *byte = &f->bits[(size_t)y * f->stride + (unsigned)x / 8u];
    uint8_t mask = (uint8_t)(0x80u >> ((unsigned)x % 8u));
    if (ink) {
        *byte = (uint8_t)(*byte | mask);
    } else {
        *byte = (uint8_t)(*byte & (uint8_t)~mask);
    }
}

void bk_frame_fill_rect(bk_frame_t *f, int x, int y, int w, int h, bool ink)
{
    for (int yy = y; yy < y + h; yy++) {
        for (int xx = x; xx < x + w; xx++) {
            bk_frame_set(f, xx, yy, ink);
        }
    }
}

bool bk_frame_equal(const bk_frame_t *a, const bk_frame_t *b)
{
    return a->width == b->width && a->height == b->height && memcmp(a->bits, b->bits, bk_frame_bytes(a)) == 0;
}
