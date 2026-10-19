#include "brick/input.h"

bool bk_repeat_fire(uint32_t *counter, uint8_t key, const bk_input_t *in, uint32_t delay, uint32_t interval)
{
    if (((in->held | in->pressed) & key) == 0u) {
        *counter = 0u;
        return false;
    }
    if ((in->pressed & key) != 0u) {
        *counter = 0u;
        return true;
    }
    uint32_t count = *counter + 1u;
    if (count == 0u) { /* saturate instead of wrapping after 2^32 ticks */
        count = UINT32_MAX;
    }
    *counter = count;
    return count >= delay && (count - delay) % interval == 0u;
}
