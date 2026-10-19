/*
 * Brick core: common constants.
 *
 * The core is platform neutral: no dynamic allocation, no global mutable state, no I/O,
 * no dependency on a display, MCU or board. Everything it needs from the outside world
 * goes through the abstractions in hal.h.
 *
 * Constants mirror the reference implementation (ApolloBrickGames, src/brick/types.ts).
 */
#ifndef BRICK_DEFS_H
#define BRICK_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define BK_EXTERN_C_BEGIN extern "C" {
#define BK_EXTERN_C_END   }
#else
#define BK_EXTERN_C_BEGIN
#define BK_EXTERN_C_END
#endif

BK_EXTERN_C_BEGIN

/* Logical game matrix: 10 columns x 20 rows of square cells (CONFIRMED, ADR-016). */
#define BK_COLS  10
#define BK_ROWS  20
#define BK_CELLS (BK_COLS * BK_ROWS)

/* "NEXT" preview matrix. */
#define BK_PREVIEW_COLS  4
#define BK_PREVIEW_ROWS  4
#define BK_PREVIEW_CELLS (BK_PREVIEW_COLS * BK_PREVIEW_ROWS)

/* Fixed simulation rate. One bk_game_step() call is one tick; games are deterministic per tick. */
#define BK_TICK_HZ 60

/*
 * Cell values produced by games. The *logical* frame stays strictly two-level (ADR-003):
 * BK_CELL_BLINK is an attribute ("lit, but toggles with the blink phase"); the compositor
 * resolves it to ink / no ink using a tick-derived phase, so no third level ever reaches the display.
 */
#define BK_CELL_OFF   0
#define BK_CELL_ON    1
#define BK_CELL_BLINK 2
/* Half period of the blink phase in ticks (reference: BLINK_HALF_TICKS = 15, i.e. 2 Hz toggling). */
#define BK_BLINK_HALF_TICKS 15

/* Game speed and level as in the classic toy: 1..10 (the menu shows value - 1). */
#define BK_MIN_SPEED 1
#define BK_MAX_SPEED 10
#define BK_MIN_LEVEL 1
#define BK_MAX_LEVEL 10

#define BK_MAX_SCORE    9999999u
#define BK_DEFAULT_SEED 1u

#define BK_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

BK_EXTERN_C_END

#endif /* BRICK_DEFS_H */
