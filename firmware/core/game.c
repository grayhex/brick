/*
 * Game base class: phases, score, speed, cutscenes and the game-over animation.
 * Port of ApolloBrickGames src/brick/base.ts (BrickBase). Closures of the original become
 * (scene kind, elapsed) pairs handled by the concrete game's vtable.
 */
#include "brick/game.h"

#include <string.h>

/* Game-over flood: 127 ms per row at 60 Hz = 7.62 ticks, then a 30 tick hold. Integer arithmetic, exact. */
#define OVER_ROW_X1000 (BK_GAME_OVER_ROW_MS * BK_TICK_HZ) /* ticks per row * 1000 */

static uint32_t clamp_u32(int64_t v, uint32_t lo, uint32_t hi)
{
    if (v < (int64_t)lo) {
        return lo;
    }
    if (v > (int64_t)hi) {
        return hi;
    }
    return (uint32_t)v;
}

void bk_game_base_init(bk_game_t *g, const bk_game_vtable_t *vt, const bk_game_options_t *opt, uint16_t mode_count)
{
    memset(g, 0, sizeof *g);
    g->vt = vt;
    bk_rng_seed(&g->rng, opt->seed);
    g->start_speed = (uint8_t)clamp_u32(opt->speed, BK_MIN_SPEED, BK_MAX_SPEED);
    g->start_level = (uint8_t)clamp_u32(opt->level, BK_MIN_LEVEL, BK_MAX_LEVEL);
    g->speed = g->start_speed;
    g->level = g->start_level;
    g->hi_score = clamp_u32(opt->hi_score, 0u, BK_MAX_SCORE);
    g->mode_count = mode_count == 0u ? 1u : mode_count;
    g->mode = (uint16_t)(opt->mode % g->mode_count);
    g->phase = BK_PHASE_READY;
    g->lives = -1;
}

/* ---------------------------------------------------------------- internals */

static void fresh_game(bk_game_t *g)
{
    g->score = 0;
    g->speed = g->start_speed;
    g->level = g->start_level;
    g->lives = -1;
    g->tick = 0;
    g->over_fill = 0;
    g->dying = false;
    g->over_timer = 0;
    g->scene_on = false;
    g->overlay_on = false;
    g->extra_lives = 0;
    g->vt->reset(g);
}

static void scene_frame(bk_game_t *g, uint8_t kind, uint16_t elapsed)
{
    if (g->vt->scene_frame != NULL) {
        g->vt->scene_frame(g, kind, elapsed);
    }
}

static void step_scene(bk_game_t *g)
{
    g->scene_elapsed++;
    if (g->scene_elapsed < g->scene_total) {
        scene_frame(g, g->scene_kind, g->scene_elapsed);
        return;
    }
    uint8_t kind = g->scene_kind;
    g->scene_on = false;
    g->overlay_on = false;
    if (g->vt->scene_done != NULL) {
        g->vt->scene_done(g, kind);
    }
}

static void step_dying(bk_game_t *g)
{
    g->over_timer++;
    uint32_t rows = ((uint32_t)g->over_timer * 1000u) / OVER_ROW_X1000;
    g->over_fill = (uint8_t)(rows > BK_ROWS ? BK_ROWS : rows);
    if ((uint32_t)g->over_timer * 1000u >= BK_ROWS * OVER_ROW_X1000 + BK_GAME_OVER_HOLD_TICKS * 1000u) {
        g->dying = false;
        g->phase = BK_PHASE_OVER;
    }
}

/* ---------------------------------------------------------------- shell API */

void bk_game_start(bk_game_t *g, bk_event_queue_t *q)
{
    g->out = q;
    if (g->phase == BK_PHASE_OVER) {
        fresh_game(g);
    } else if (g->phase != BK_PHASE_READY) {
        g->out = NULL;
        return;
    }
    g->phase = BK_PHASE_PLAYING;
    bk_game_emit(g, BK_EV_STARTED, 0, 0, 0);
    if (g->vt->on_start != NULL) {
        g->vt->on_start(g);
    }
    g->out = NULL;
}

void bk_game_pause(bk_game_t *g, bk_event_queue_t *q)
{
    if (g->phase != BK_PHASE_PLAYING) {
        return;
    }
    g->phase = BK_PHASE_PAUSED;
    if (q != NULL) {
        bk_events_push(q, BK_EV_PAUSED, 0, 0, 0);
    }
}

void bk_game_resume(bk_game_t *g, bk_event_queue_t *q)
{
    if (g->phase != BK_PHASE_PAUSED) {
        return;
    }
    g->phase = BK_PHASE_PLAYING;
    if (q != NULL) {
        bk_events_push(q, BK_EV_RESUMED, 0, 0, 0);
    }
}

void bk_game_restart(bk_game_t *g)
{
    g->out = NULL;
    fresh_game(g);
    g->phase = BK_PHASE_READY;
}

void bk_game_step(bk_game_t *g, const bk_input_t *in, bk_event_queue_t *q)
{
    if (g->phase != BK_PHASE_PLAYING) {
        return;
    }
    g->out = q;
    g->tick++;
    if (g->dying) {
        step_dying(g);
    } else if (g->scene_on) {
        step_scene(g);
    } else {
        g->vt->update(g, in);
    }
    g->out = NULL;
}

void bk_game_cells(const bk_game_t *g, uint8_t cells[BK_CELLS])
{
    if (g->overlay_on) {
        memcpy(cells, g->overlay, BK_CELLS);
    } else {
        g->vt->draw(g, cells);
    }
}

bool bk_game_preview(const bk_game_t *g, uint8_t cells[BK_PREVIEW_CELLS])
{
    memset(cells, 0, BK_PREVIEW_CELLS);
    if (g->vt->draw_preview != NULL && g->vt->draw_preview(g, cells)) {
        return true;
    }
    if (g->lives < 0) {
        return false;
    }
    /* Lives indicator, as the donor: each life is a pair of cells in columns 1 and 2, top to bottom. */
    int lives = g->lives > 4 ? 4 : g->lives;
    for (int i = 0; i < lives; i++) {
        cells[i * BK_PREVIEW_COLS + 1] = BK_CELL_ON;
        cells[i * BK_PREVIEW_COLS + 2] = BK_CELL_ON;
    }
    return true;
}

/* ---------------------------------------------------------------- helpers for games */

void bk_game_emit(bk_game_t *g, bk_event_type_t type, uint8_t a, uint16_t b, uint32_t c)
{
    if (g->out != NULL) {
        bk_events_push(g->out, type, a, b, c);
    }
}

void bk_game_set_score(bk_game_t *g, uint32_t value)
{
    g->score = value > BK_MAX_SCORE ? BK_MAX_SCORE : value;
    if (g->score > g->hi_score) {
        g->hi_score = g->score;
    }
    if (g->scores_to_extra_life > 0u && g->lives >= 0) {
        /* BrickBase.setScore: an extra life per multiple passed, or a bonus when the four lives are full */
        uint32_t earned = g->score / g->scores_to_extra_life;
        while (earned > g->extra_lives) {
            g->extra_lives++;
            bk_game_emit(g, BK_EV_BONUS, 0, 0, 0);
            if (g->lives < BK_MAX_LIVES) {
                g->lives++;
            } else {
                uint64_t bonus = (uint64_t)g->score + g->scores_to_extra_life / 10u;
                g->score = bonus > BK_MAX_SCORE ? BK_MAX_SCORE : (uint32_t)bonus;
            }
        }
        if (g->score > g->hi_score) {
            g->hi_score = g->score;
        }
    }
}

void bk_game_add_score(bk_game_t *g, uint32_t points)
{
    uint64_t sum = (uint64_t)g->score + points;
    bk_game_set_score(g, sum > BK_MAX_SCORE ? BK_MAX_SCORE : (uint32_t)sum);
}

void bk_game_set_speed(bk_game_t *g, int value)
{
    if (value < BK_MIN_SPEED) {
        g->speed = BK_MAX_SPEED;
    } else if (value > BK_MAX_SPEED) {
        g->speed = BK_MIN_SPEED;
    } else {
        g->speed = (uint8_t)value;
    }
}

void bk_game_game_over(bk_game_t *g)
{
    if (g->dying) {
        return;
    }
    g->scene_on = false;
    g->overlay_on = false;
    g->dying = true;
    g->over_timer = 0;
    g->over_fill = 0;
    if (g->score > g->hi_score) {
        g->hi_score = g->score;
    }
    bk_game_emit(g, BK_EV_GAME_OVER, 0, 0, 0);
}

void bk_game_cutscene(bk_game_t *g, uint8_t kind, uint32_t total_ticks)
{
    g->scene_on = true;
    g->scene_kind = kind;
    g->scene_total = (uint16_t)(total_ticks < 1u ? 1u : total_ticks);
    g->scene_elapsed = 0;
    scene_frame(g, kind, 0); /* the first frame is visible in the same tick */
}

bool bk_game_busy(const bk_game_t *g)
{
    return g->scene_on || g->dying;
}

uint8_t *bk_game_overlay(bk_game_t *g)
{
    g->overlay_on = true;
    return g->overlay;
}

/* ---------------------------------------------------------------- lives and clear scenes */

void bk_game_set_lives(bk_game_t *g, int count)
{
    g->lives = (int8_t)(count < 0 ? 0 : (count > BK_MAX_LIVES ? BK_MAX_LIVES : count));
}

void bk_game_next_level(bk_game_t *g)
{
    int next = g->level + 1;
    if (next > BK_MAX_LEVEL) {
        g->level = BK_MIN_LEVEL;
        bk_game_set_speed(g, g->speed + 1);
    } else {
        g->level = (uint8_t)next;
    }
    bk_game_emit(g, BK_EV_LEVEL_UP, g->speed, 0, 0);
}

/* ceil() for 0 <= x < 2^31 without libm: the core keeps fmod() as its only libm call (ADR-024). */
static int ceil_small(double x)
{
    int i = (int)x;
    return (double)i < x ? i + 1 : i;
}

void bk_game_paint_clear(bk_game_t *g, uint32_t total, uint16_t elapsed)
{
    uint8_t *out = bk_game_overlay(g);
    memset(out, 0, BK_CELLS);
    /* Same double arithmetic and order as the reference: ceil(((elapsed + 1) / half) * rows). */
    double half = (double)(total < 2u ? 2u : total) / 2.0;
    if ((double)elapsed < half) {
        int rows = ceil_small(((double)elapsed + 1.0) / half * (double)BK_ROWS);
        int n = rows > BK_ROWS ? BK_ROWS : rows;
        memset(out + (BK_ROWS - n) * BK_COLS, BK_CELL_ON, (size_t)n * BK_COLS);
    } else {
        int cleared = ceil_small(((double)elapsed + 1.0 - half) / half * (double)BK_ROWS);
        int n = cleared > BK_ROWS ? BK_ROWS : cleared;
        memset(out + n * BK_COLS, BK_CELL_ON, (size_t)(BK_ROWS - n) * BK_COLS);
    }
}

void bk_game_lose_life(bk_game_t *g, uint8_t scene_kind)
{
    bk_game_lose_life_ticks(g, scene_kind, BK_CLEAR_SCENE_TICKS);
}

void bk_game_lose_life_ticks(bk_game_t *g, uint8_t scene_kind, uint32_t ticks)
{
    int left = (g->lives < 0 ? 1 : g->lives) - 1;
    bk_game_set_lives(g, left < 0 ? 0 : left);
    bk_game_emit(g, BK_EV_LIFE_LOST, 0, 0, 0);
    if (g->lives == 0) {
        bk_game_game_over(g);
    } else {
        bk_game_cutscene(g, scene_kind, ticks);
    }
}

void bk_game_win_level(bk_game_t *g, uint8_t scene_kind)
{
    bk_game_add_score(g, 100);
    bk_game_emit(g, BK_EV_WIN, 0, 0, 0);
    bk_game_cutscene(g, scene_kind, BK_CLEAR_SCENE_TICKS);
}

/* Explosion waves of the donor (Game.kaboom), 5 x 5, '#' lit. */
#define KABOOM_SIZE 5
static const char *const KABOOM_WAVES[7][KABOOM_SIZE] = {
    {".....", ".###.", ".#.#.", ".###.", "....."}, {"#####", "#...#", "#...#", "#...#", "#####"},
    {"#.#.#", ".....", "#...#", ".....", "#.#.#"}, {"#.#.#", ".###.", "##.##", ".###.", "#.#.#"},
    {".....", ".###.", ".#.#.", ".###.", "....."}, {".....", ".....", "..#..", ".....", "....."},
    {".....", ".....", ".....", ".....", "....."},
};

void bk_game_paint_kaboom(bk_game_t *g, const uint8_t base[BK_CELLS], int x, int y, uint16_t elapsed)
{
    int half = (KABOOM_SIZE - 1) / 2;
    int cx = x < half ? half : (x > BK_COLS - 1 - half ? BK_COLS - 1 - half : x);
    int cy = y < half ? half : (y > BK_ROWS - 1 - half ? BK_ROWS - 1 - half : y);
    const char *const *wave = KABOOM_WAVES[(elapsed / BK_KABOOM_WAVE_TICKS) % 7u];
    uint8_t *out = bk_game_overlay(g);
    memcpy(out, base, BK_CELLS);
    for (int dy = 0; dy < KABOOM_SIZE; dy++) {
        for (int dx = 0; dx < KABOOM_SIZE; dx++) {
            out[(cy - half + dy) * BK_COLS + (cx - half + dx)] = wave[dy][dx] == '#' ? BK_CELL_ON : BK_CELL_OFF;
        }
    }
}
