/* Test/dev helpers to look at a logical frame: ASCII (golden files) and PBM (-> PNG via tools/frames/pbm_to_png.py). */
#ifndef BK_FRAME_DUMP_H
#define BK_FRAME_DUMP_H

#include <stdio.h>

#include "brick/frame.h"

/* '#' = ink, '.' = light; one text line per frame row. Returns the number of bytes written (excluding NUL). */
size_t bk_frame_to_ascii(const bk_frame_t *f, char *buf, size_t cap);
int bk_frame_write_pbm(const bk_frame_t *f, const char *path);

#endif
