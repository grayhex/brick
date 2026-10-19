// The public headers must be usable from C++ (the task allows C/C++ firmware): include them all and use a few entry points.
#include <cstdio>

#include "brick/audio.h"
#include "brick/console.h"
#include "brick/defs.h"
#include "brick/event.h"
#include "brick/font.h"
#include "brick/frame.h"
#include "brick/game.h"
#include "brick/games/tetris.h"
#include "brick/hal.h"
#include "brick/input.h"
#include "brick/render.h"
#include "brick/rng.h"
#include "brick/save.h"

static_assert(BK_COLS == 10 && BK_ROWS == 20, "classic 10 x 20 playfield (ADR-016)");
static_assert(BK_CELLS == 200, "cells");
static_assert(sizeof(bk_event_t) == 8, "events stay small");

int main()
{
    bk_rng_t rng;
    bk_rng_seed(&rng, 1);
    bk_layout_t layout;
    if (!bk_layout_init(&layout, 8)) {
        return 1;
    }

    bk_game_options_t options{};
    options.seed = 1;
    options.speed = 1;
    options.level = 1;
    alignas(8) static unsigned char storage[BK_GAME_STATE_MAX];
    bk_game_t *game = bk_tetris_def.create(storage, &options);
    bk_event_queue_t events;
    bk_events_clear(&events);
    bk_game_start(game, &events);
    const bk_input_t input = bk_input_make(BK_KEY_LEFT, 0);
    bk_game_step(game, &input, &events);

    bk_frame_t frame;
    bk_frame_init(&frame, layout.width, layout.height);
    std::printf("cpp ok: %u x %u dots, first event %u\n", frame.width, frame.height, events.items[0].type);
    return game->phase == BK_PHASE_PLAYING ? 0 : 1;
}
