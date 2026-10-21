/*
 * Sound: maps game and menu events to short speaker cues and plays them on the single audio voice of the HAL.
 *
 * Speaker only (ADR-007): no Bluetooth audio, no samples. A cue is a list of (frequency, duration) notes; the
 * sequencer advances once per game tick (60 Hz), so timing is deterministic and testable.
 *
 * The cue tones below are PLACEHOLDERS for the prototype: simple original phrases, NOT transcribed from any
 * firmware or from the donor. Final sound design and acoustic tuning are OPEN (needs the real speaker and cavity).
 */
#ifndef BRICK_AUDIO_H
#define BRICK_AUDIO_H

#include "brick/event.h"
#include "brick/hal.h"

BK_EXTERN_C_BEGIN

typedef enum {
    BK_CUE_NONE = 0,
    BK_CUE_SELECT,  /* menu navigation tick */
    BK_CUE_CONFIRM, /* start / OK */
    BK_CUE_BACK,
    BK_CUE_MOVE,
    BK_CUE_ROTATE,
    BK_CUE_LOCK,
    BK_CUE_DROP,
    BK_CUE_LINE,   /* 1..3 rows */
    BK_CUE_TETRIS, /* 4 rows */
    BK_CUE_LEVEL_UP,
    BK_CUE_PAUSE,
    BK_CUE_RESUME,
    BK_CUE_GAME_OVER,
    BK_CUE_BLOCKED,
    BK_CUE_SHOT,      /* the player fires (Tank): short, repeats every 14 ticks while the key is held */
    BK_CUE_HIT,       /* a bullet hit something: a wall cell, a bullet */
    BK_CUE_EXPLODE,   /* an enemy is destroyed */
    BK_CUE_CRASH,     /* the player is hit / crashes: the start of the explosion */
    BK_CUE_LIFE_LOST, /* the explosion is over and a life is gone */
    BK_CUE_BONUS,     /* an extra life, a fruit eaten */
    BK_CUE_WIN,       /* a level is won: the start of the clear scene */
    BK_CUE_COUNT
} bk_cue_t;

typedef struct {
    const bk_audio_port_t *port; /* may be NULL */
    bool enabled;                /* software mute (settings); the volume wheel is analogue and invisible here */
    bk_cue_t cue;                /* currently playing, BK_CUE_NONE if idle */
    uint8_t note;                /* index within the cue */
    uint16_t ticks_left;         /* ticks left in the current note */
    uint16_t last_hz;            /* last frequency sent to the port, to avoid redundant calls */
} bk_audio_t;

void bk_audio_init(bk_audio_t *a, const bk_audio_port_t *port, bool enabled);
void bk_audio_set_enabled(bk_audio_t *a, bool enabled); /* disabling silences at once */
void bk_audio_play(bk_audio_t *a, bk_cue_t cue);        /* a lower-priority cue never interrupts a higher one */
void bk_audio_tick(bk_audio_t *a);                      /* call once per tick */
bool bk_audio_busy(const bk_audio_t *a);

/* Which cue a game event produces (BK_CUE_NONE: silent). `in_menu` selects the menu variants. */
bk_cue_t bk_audio_cue_for_event(const bk_event_t *e, bool in_menu);

/* Test access to the phrase table. */
unsigned bk_audio_cue_note_count(bk_cue_t cue);
unsigned bk_audio_cue_priority(bk_cue_t cue);
unsigned bk_audio_cue_ticks(bk_cue_t cue); /* total duration in ticks */

BK_EXTERN_C_END

#endif /* BRICK_AUDIO_H */
