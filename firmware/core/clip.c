#include "brick/clip.h"

#include <string.h>

uint16_t bk_clip_frame_at(const bk_clip_t *clip, uint32_t tick)
{
    if (clip == NULL || clip->frame_count == 0u) {
        return 0;
    }
    uint32_t step = clip->frame_ticks > 0u ? tick / clip->frame_ticks : 0u;
    return (uint16_t)(step % clip->frame_count);
}

void bk_clip_paint(const bk_clip_t *clip, uint16_t frame, uint8_t cells[BK_CLIP_CELLS])
{
    memset(cells, BK_CELL_OFF, BK_CLIP_CELLS);
    if (clip == NULL || clip->frame_count == 0u || clip->frames == NULL) {
        return;
    }
    const char *src = clip->frames[frame % clip->frame_count];
    for (int i = 0; i < BK_CLIP_CELLS; i++) {
        cells[i] = src[i] == '#' ? BK_CELL_ON : BK_CELL_OFF;
    }
}

bool bk_clip_valid(const bk_clip_t *clip)
{
    if (clip == NULL || clip->frames == NULL || clip->frame_count < 2u || clip->frame_ticks == 0u ||
        clip->poster >= clip->frame_count) {
        return false;
    }
    for (unsigned f = 0; f < clip->frame_count; f++) {
        if (strlen(clip->frames[f]) != BK_CLIP_CELLS) {
            return false;
        }
        for (int i = 0; i < BK_CLIP_CELLS; i++) {
            if (clip->frames[f][i] != '#' && clip->frames[f][i] != '.') {
                return false;
            }
        }
    }
    return true;
}
