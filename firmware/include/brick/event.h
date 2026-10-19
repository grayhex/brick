/*
 * Game / shell events. A game never plays sound itself; it emits events, and the shell maps them to
 * speaker cues (audio.h). Event order inside a tick is part of the conformance contract.
 */
#ifndef BRICK_EVENT_H
#define BRICK_EVENT_H

#include "brick/defs.h"

BK_EXTERN_C_BEGIN

/* Numeric values are part of the conformance fixture format (tools/conformance). Do not renumber. */
typedef enum {
    BK_EV_NONE = 0,
    BK_EV_STARTED = 1,
    BK_EV_MOVE = 2,
    BK_EV_ROTATE = 3,
    BK_EV_LOCK = 4,
    BK_EV_DROP = 5,
    BK_EV_SHOT = 6,
    BK_EV_HIT = 7,
    BK_EV_EXPLODE = 8,
    BK_EV_CRASH = 9,
    BK_EV_LIFE_LOST = 10,
    BK_EV_BONUS = 11,
    BK_EV_WIN = 12,
    BK_EV_BLOCKED = 13,
    BK_EV_TONE = 14,     /* a = pad (0..3), b = duration in ticks */
    BK_EV_LINES = 15,    /* a = rows removed by this landing, c = bit mask of screen rows (bit 0 = top row) */
    BK_EV_LEVEL_UP = 16, /* a = new speed */
    BK_EV_GAME_OVER = 17,
    BK_EV_PAUSED = 18,
    BK_EV_RESUMED = 19,
    /* Shell-only events (never emitted by games, not part of conformance fixtures). */
    BK_EV_UI_CONFIRM = 32,
    BK_EV_UI_BACK = 33
} bk_event_type_t;

typedef struct {
    uint8_t type; /* bk_event_type_t */
    uint8_t a;
    uint16_t b;
    uint32_t c;
} bk_event_t;

/* Capacity is generous: the busiest reference tick emits fewer than 8 events (checked by the fuzz tests). */
#define BK_EVENT_CAP 16

typedef struct {
    bk_event_t items[BK_EVENT_CAP];
    uint8_t count;
    uint8_t dropped; /* events lost to overflow; must stay 0, asserted by tests */
} bk_event_queue_t;

static inline void bk_events_clear(bk_event_queue_t *q)
{
    q->count = 0;
    q->dropped = 0;
}

bool bk_events_push(bk_event_queue_t *q, bk_event_type_t type, uint8_t a, uint16_t b, uint32_t c);

BK_EXTERN_C_END

#endif /* BRICK_EVENT_H */
