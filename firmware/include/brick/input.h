/*
 * Input abstractions.
 *
 * Two layers:
 *  - bk_button_t : physical controls of the console (D-pad, one ACTION, three FUNCTION keys).
 *                  Meaning of the function keys is a shell concern (see console.h) and is OPEN in the PRD.
 *  - bk_key_t    : logical keys a *game* sees. Games never see function keys. FIRE exists in the engine
 *                  (hard drop, shooting) but has no physical button on the console; the shell decides
 *                  whether and how it is mapped (PRD section 5, one action button).
 */
#ifndef BRICK_INPUT_H
#define BRICK_INPUT_H

#include "brick/defs.h"

BK_EXTERN_C_BEGIN

/* Physical buttons, bit mask (matches the sketch in docs/GAME_PORTING.md). */
typedef enum {
    BK_BTN_UP = 1u << 0,
    BK_BTN_DOWN = 1u << 1,
    BK_BTN_LEFT = 1u << 2,
    BK_BTN_RIGHT = 1u << 3,
    BK_BTN_ACTION = 1u << 4,
    BK_BTN_FN1 = 1u << 5,
    BK_BTN_FN2 = 1u << 6,
    BK_BTN_FN3 = 1u << 7
} bk_button_t;

typedef uint16_t bk_buttons_t;

/* Logical game keys, bit mask. */
typedef enum {
    BK_KEY_LEFT = 1u << 0,
    BK_KEY_RIGHT = 1u << 1,
    BK_KEY_UP = 1u << 2,
    BK_KEY_DOWN = 1u << 3,
    BK_KEY_ROTATE = 1u << 4, /* the big action button */
    BK_KEY_FIRE = 1u << 5    /* engine-level only, see above */
} bk_key_t;

#define BK_KEY_ALL 0x3Fu

/* One tick of game input. Invariant: pressed is a subset of held. */
typedef struct {
    uint8_t pressed; /* rising edge in this tick */
    uint8_t held;    /* currently down (includes pressed) */
} bk_input_t;

static inline bk_input_t bk_input_make(uint8_t pressed, uint8_t held)
{
    bk_input_t in;
    in.pressed = pressed;
    in.held = (uint8_t)(held | pressed);
    return in;
}

/* Edge detector: feed the held mask every tick, get a normalised bk_input_t. */
typedef struct {
    uint16_t prev;
} bk_edge_t;

static inline uint16_t bk_edge_update(bk_edge_t *e, uint16_t held)
{
    uint16_t pressed = (uint16_t)(held & (uint16_t)~e->prev);
    e->prev = held;
    return pressed;
}

/*
 * Auto-repeat of a held key, same semantics as the reference Repeater: fires on the rising edge, then after
 * `delay` ticks and every `interval` ticks while held. The counter is per key, zero while the key is up.
 * Calling it in a tick without a prior edge call (e.g. the key was pressed during a cutscene) starts at 1.
 */
bool bk_repeat_fire(uint32_t *counter, uint8_t key, const bk_input_t *in, uint32_t delay, uint32_t interval);

BK_EXTERN_C_END

#endif /* BRICK_INPUT_H */
