/*
 * The looped gameplay clips of the menu (brick/clip.h, ADR-041): the player and the data of every shipped game.
 * The art itself is reviewed by eye (docs/img, UI_MENU.md section 4); these checks keep it well-formed: a real
 * animation that loops without a jump, at a rate a Memory LCD can follow, never an empty or flooded picture.
 */
#include <string.h>

#include "bk_test.h"
#include "brick/clip.h"
#include "brick/games/racing.h"
#include "brick/games/snake.h"
#include "brick/games/tank.h"
#include "brick/games/tetris.h"
#include "stub_game.h"

static const bk_game_def_t *const GAMES[] = {&bk_tetris_def, &bk_snake_def, &bk_racing_def, &bk_tank_def, &stub_def_multi};

#define MAX_STEP_CELLS          24 /* cells that may change between two neighbouring pictures: a line burning out is 20 */
#define MAX_PICTURES_PER_SECOND 10 /* the fastest panel candidate (Sharp LS032B7DD02 quotes fSCS 1..10 Hz) */

static unsigned lit_cells(const bk_clip_t *clip, uint16_t frame)
{
    uint8_t cells[BK_CLIP_CELLS];
    bk_clip_paint(clip, frame, cells);
    unsigned n = 0;
    for (int i = 0; i < BK_CLIP_CELLS; i++) {
        n += cells[i] == BK_CELL_ON ? 1u : 0u;
    }
    return n;
}

static unsigned changed_cells(const bk_clip_t *clip, uint16_t a, uint16_t b)
{
    uint8_t ca[BK_CLIP_CELLS];
    uint8_t cb[BK_CLIP_CELLS];
    bk_clip_paint(clip, a, ca);
    bk_clip_paint(clip, b, cb);
    unsigned n = 0;
    for (int i = 0; i < BK_CLIP_CELLS; i++) {
        n += ca[i] != cb[i] ? 1u : 0u;
    }
    return n;
}

static void test_shipped_clips_are_well_formed_animations(void)
{
    for (size_t g = 0; g < sizeof GAMES / sizeof GAMES[0]; g++) {
        const bk_clip_t *clip = GAMES[g]->preview;
        CHECK(clip != NULL);
        if (clip == NULL) {
            continue;
        }
        CHECK(bk_clip_valid(clip));
        CHECK(clip->poster < clip->frame_count);

        unsigned pictures_per_second = BK_TICK_HZ / clip->frame_ticks;
        CHECK(pictures_per_second >= 1 && pictures_per_second <= MAX_PICTURES_PER_SECOND);
        unsigned loop_ticks = (unsigned)clip->frame_count * clip->frame_ticks;
        if (GAMES[g] != &stub_def_multi) {  /* the stub is four frames long on purpose */
            CHECK(clip->frame_count >= 20); /* a story, not a blink */
            CHECK(loop_ticks >= 2 * BK_TICK_HZ && loop_ticks <= 8 * BK_TICK_HZ); /* 2..8 s before it repeats */
        }

        for (uint16_t f = 0; f < clip->frame_count; f++) {
            unsigned lit = lit_cells(clip, f);
            CHECK(lit >= 1 && lit <= BK_CLIP_CELLS * 7 / 10);         /* never an empty or a flooded picture */
            uint16_t next = (uint16_t)((f + 1u) % clip->frame_count); /* the last one runs into the first: the loop point */
            unsigned step = changed_cells(clip, f, next);
            if (step == 0 || step > MAX_STEP_CELLS) {
                fprintf(stderr, "  clip of %s: frame %u -> %u changes %u cells\n", GAMES[g]->id, (unsigned)f, (unsigned)next,
                        step);
            }
            CHECK(step > 0);               /* every picture is new: no stalls */
            CHECK(step <= MAX_STEP_CELLS); /* and the next one is a step of the action, not a jump */
        }
        if (GAMES[g] != &stub_def_multi) {
            CHECK(lit_cells(clip, clip->poster) >= 5); /* the still picture of DEMO off shows a scene, not a speck */
        }
    }
}

static void test_the_player_follows_the_ticks_and_wraps(void)
{
    const bk_clip_t *c = &stub_clip;
    CHECK_EQ(bk_clip_frame_at(c, 0), 0);
    CHECK_EQ(bk_clip_frame_at(c, STUB_CLIP_TICKS - 1), 0);
    CHECK_EQ(bk_clip_frame_at(c, STUB_CLIP_TICKS), 1);
    CHECK_EQ(bk_clip_frame_at(c, 3 * STUB_CLIP_TICKS), 3);
    CHECK_EQ(bk_clip_frame_at(c, 4 * STUB_CLIP_TICKS), 0); /* the loop wraps */
    CHECK_EQ(bk_clip_frame_at(c, 4 * STUB_CLIP_TICKS * 1000 + 2 * STUB_CLIP_TICKS), 2);
    CHECK_EQ(bk_clip_frame_at(NULL, 99), 0);

    uint8_t cells[BK_CLIP_CELLS];
    bk_clip_paint(c, 1, cells);
    CHECK_EQ(cells[4 * BK_CLIP_COLS + 1], BK_CELL_ON); /* the dot of frame 1 is in column 1 of the last row */
    CHECK_EQ(lit_cells(c, 1), 1);
    bk_clip_paint(c, 5, cells); /* a frame number past the end wraps like the ticks do */
    CHECK_EQ(cells[4 * BK_CLIP_COLS + 1], BK_CELL_ON);
    bk_clip_paint(NULL, 0, cells);
    CHECK_EQ(lit_cells(c, 0) + lit_cells(c, 2), 2);
    for (int i = 0; i < BK_CLIP_CELLS; i++) {
        CHECK_EQ(cells[i], BK_CELL_OFF); /* no clip: an empty area, not stale cells */
    }
}

static void test_a_broken_clip_is_recognised(void)
{
    static const char good[][BK_CLIP_CELLS + 1] = {
        ".........."
        ".........."
        ".........."
        ".........."
        "#.........",
        ".........."
        ".........."
        ".........."
        ".........."
        ".#........",
    };
    static const char short_frame[][BK_CLIP_CELLS + 1] = {
        ".........."
        ".........."
        ".........."
        ".........."
        "#.........",
        ".........."
        ".........."
        ".........."
        ".........."
        ".#.......",
    };
    static const char bad_char[][BK_CLIP_CELLS + 1] = {
        ".........."
        ".........."
        ".........."
        ".........."
        "#.........",
        ".........."
        ".........."
        ".........."
        ".........."
        ".#.....x..",
    };
    bk_clip_t c = {2, 10, 1, good};
    CHECK(bk_clip_valid(&c));
    c.frame_count = 1; /* a one-frame loop is not an animation */
    CHECK(!bk_clip_valid(&c));
    c.frame_count = 2;
    c.frame_ticks = 0;
    CHECK(!bk_clip_valid(&c));
    c.frame_ticks = 10;
    c.poster = 2; /* past the end */
    CHECK(!bk_clip_valid(&c));
    c.poster = 1;
    c.frames = short_frame;
    CHECK(!bk_clip_valid(&c));
    c.frames = bad_char;
    CHECK(!bk_clip_valid(&c));
    c.frames = NULL;
    CHECK(!bk_clip_valid(&c));
    CHECK(!bk_clip_valid(NULL));
}

int main(void)
{
    TEST_RUN(test_shipped_clips_are_well_formed_animations);
    TEST_RUN(test_the_player_follows_the_ticks_and_wraps);
    TEST_RUN(test_a_broken_clip_is_recognised);
    return bk_test_finish("clips");
}
