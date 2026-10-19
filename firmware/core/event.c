#include "brick/event.h"

bool bk_events_push(bk_event_queue_t *q, bk_event_type_t type, uint8_t a, uint16_t b, uint32_t c)
{
    if (q->count >= BK_EVENT_CAP) {
        if (q->dropped < UINT8_MAX) {
            q->dropped++;
        }
        return false;
    }
    bk_event_t *e = &q->items[q->count++];
    e->type = (uint8_t)type;
    e->a = a;
    e->b = b;
    e->c = c;
    return true;
}
