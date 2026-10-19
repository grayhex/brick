#include "frame_dump.h"

size_t bk_frame_to_ascii(const bk_frame_t *f, char *buf, size_t cap)
{
    size_t n = 0;
    for (int y = 0; y < f->height; y++) {
        for (int x = 0; x < f->width; x++) {
            if (n + 2 >= cap) {
                buf[n] = '\0';
                return n;
            }
            buf[n++] = bk_frame_get(f, x, y) ? '#' : '.';
        }
        buf[n++] = '\n';
    }
    buf[n] = '\0';
    return n;
}

int bk_frame_write_pbm(const bk_frame_t *f, const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    fprintf(fp, "P4\n%u %u\n", f->width, f->height);
    fwrite(f->bits, 1, bk_frame_bytes(f), fp);
    fclose(fp);
    return 0;
}
