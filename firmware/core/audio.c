#include "brick/audio.h"

/* Duration in ticks (60 Hz) from milliseconds, at least one tick. */
#define T(ms) ((uint16_t)(((ms) * BK_TICK_HZ + 500) / 1000 > 0 ? ((ms) * BK_TICK_HZ + 500) / 1000 : 1))

typedef struct {
    uint16_t hz; /* 0 = rest */
    uint16_t ticks;
} note_t;

typedef struct {
    const note_t *notes;
    uint8_t count;
    uint8_t priority;
} phrase_t;

#define PHRASE(prio, ...)                                                                                                   \
    {                                                                                                                       \
        (const note_t[]){__VA_ARGS__}, sizeof((note_t[]){__VA_ARGS__}) / sizeof(note_t), prio                               \
    }

/* Placeholder cues (see audio.h). Indexed by bk_cue_t. */
// clang-format off
static const phrase_t PHRASES[BK_CUE_COUNT] = {
    [BK_CUE_NONE] = {NULL, 0, 0},
    [BK_CUE_SELECT] = PHRASE(1, {1000, T(45)}),
    [BK_CUE_CONFIRM] = PHRASE(3, {660, T(60)}, {880, T(60)}, {1320, T(100)}),
    [BK_CUE_BACK] = PHRASE(1, {880, T(50)}, {660, T(70)}),
    [BK_CUE_MOVE] = PHRASE(0, {600, T(25)}),
    [BK_CUE_ROTATE] = PHRASE(1, {1300, T(35)}),
    [BK_CUE_LOCK] = PHRASE(2, {220, T(60)}),
    [BK_CUE_DROP] = PHRASE(2, {330, T(40)}, {220, T(60)}),
    [BK_CUE_LINE] = PHRASE(3, {660, T(60)}, {880, T(60)}, {990, T(90)}),
    [BK_CUE_TETRIS] = PHRASE(4, {660, T(60)}, {880, T(60)}, {1320, T(60)}, {1760, T(160)}),
    [BK_CUE_LEVEL_UP] = PHRASE(3, {880, T(70)}, {1100, T(70)}, {1320, T(70)}, {1760, T(140)}),
    [BK_CUE_PAUSE] = PHRASE(2, {1000, T(60)}, {700, T(90)}),
    [BK_CUE_RESUME] = PHRASE(2, {700, T(60)}, {1000, T(90)}),
    [BK_CUE_GAME_OVER] = PHRASE(5, {440, T(150)}, {392, T(150)}, {330, T(150)}, {262, T(400)}),
    [BK_CUE_BLOCKED] = PHRASE(2, {200, T(120)}),
};
// clang-format on

static void send(bk_audio_t *a, uint16_t hz)
{
    if (a->port != NULL && a->port->set_tone != NULL && hz != a->last_hz) {
        a->port->set_tone(a->port->ctx, hz);
    }
    a->last_hz = hz;
}

static void start_note(bk_audio_t *a)
{
    const phrase_t *p = &PHRASES[a->cue];
    a->ticks_left = p->notes[a->note].ticks;
    send(a, p->notes[a->note].hz);
}

static void stop(bk_audio_t *a)
{
    a->cue = BK_CUE_NONE;
    a->note = 0;
    a->ticks_left = 0;
    send(a, 0);
}

void bk_audio_init(bk_audio_t *a, const bk_audio_port_t *port, bool enabled)
{
    a->port = port;
    a->enabled = enabled;
    a->cue = BK_CUE_NONE;
    a->note = 0;
    a->ticks_left = 0;
    a->last_hz = 0;
}

void bk_audio_set_enabled(bk_audio_t *a, bool enabled)
{
    a->enabled = enabled;
    if (!enabled) {
        stop(a);
    }
}

void bk_audio_play(bk_audio_t *a, bk_cue_t cue)
{
    if (!a->enabled || cue <= BK_CUE_NONE || cue >= BK_CUE_COUNT) {
        return;
    }
    if (a->cue != BK_CUE_NONE && PHRASES[cue].priority < PHRASES[a->cue].priority) {
        return;
    }
    a->cue = cue;
    a->note = 0;
    start_note(a);
}

void bk_audio_tick(bk_audio_t *a)
{
    if (a->cue == BK_CUE_NONE) {
        return;
    }
    if (a->ticks_left > 0) {
        a->ticks_left--;
    }
    if (a->ticks_left > 0) {
        return;
    }
    a->note++;
    if (a->note >= PHRASES[a->cue].count) {
        stop(a);
    } else {
        start_note(a);
    }
}

bool bk_audio_busy(const bk_audio_t *a)
{
    return a->cue != BK_CUE_NONE;
}

bk_cue_t bk_audio_cue_for_event(const bk_event_t *e, bool in_menu)
{
    switch ((bk_event_type_t)e->type) {
    case BK_EV_STARTED:
        return BK_CUE_CONFIRM;
    case BK_EV_MOVE:
        return in_menu ? BK_CUE_SELECT : BK_CUE_MOVE;
    case BK_EV_ROTATE:
        return BK_CUE_ROTATE;
    case BK_EV_LOCK:
        return BK_CUE_LOCK;
    case BK_EV_DROP:
        return BK_CUE_DROP;
    case BK_EV_LINES:
        return e->a >= 4 ? BK_CUE_TETRIS : BK_CUE_LINE;
    case BK_EV_LEVEL_UP:
        return BK_CUE_LEVEL_UP;
    case BK_EV_GAME_OVER:
        return BK_CUE_GAME_OVER;
    case BK_EV_PAUSED:
        return BK_CUE_PAUSE;
    case BK_EV_RESUMED:
        return BK_CUE_RESUME;
    case BK_EV_BLOCKED:
        return BK_CUE_BLOCKED;
    case BK_EV_UI_CONFIRM:
        return BK_CUE_CONFIRM;
    case BK_EV_UI_BACK:
        return BK_CUE_BACK;
    default:
        return BK_CUE_NONE; /* events of games that are not ported yet (shot, hit, explode, ...) get cues with their game */
    }
}

unsigned bk_audio_cue_note_count(bk_cue_t cue)
{
    return cue < BK_CUE_COUNT ? PHRASES[cue].count : 0u;
}

unsigned bk_audio_cue_priority(bk_cue_t cue)
{
    return cue < BK_CUE_COUNT ? PHRASES[cue].priority : 0u;
}

unsigned bk_audio_cue_ticks(bk_cue_t cue)
{
    unsigned total = 0;
    if (cue < BK_CUE_COUNT) {
        for (unsigned i = 0; i < PHRASES[cue].count; i++) {
            total += PHRASES[cue].notes[i].ticks;
        }
    }
    return total;
}
