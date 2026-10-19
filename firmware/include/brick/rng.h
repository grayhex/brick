/*
 * Deterministic RNG (mulberry32), bit-exact with the reference implementation
 * (ApolloBrickGames src/core/rng.ts). The state is a single 32-bit word, so it is trivially serialisable.
 *
 * Integer-only: bk_rng_int() reproduces floor(value * max) of the reference without floating point.
 * (value = r / 2^32 is exact in a double and r * max < 2^53, so the product is exact as well.)
 */
#ifndef BRICK_RNG_H
#define BRICK_RNG_H

#include "brick/defs.h"

BK_EXTERN_C_BEGIN

typedef struct {
    uint32_t state;
} bk_rng_t;

void bk_rng_seed(bk_rng_t *rng, uint32_t seed);

/* Next raw 32-bit output; reference value() == result / 2^32. */
uint32_t bk_rng_next_u32(bk_rng_t *rng);

/* Integer in [0, max_exclusive). For max_exclusive <= 1 returns 0 and does NOT advance the state (as the reference). */
uint32_t bk_rng_int(bk_rng_t *rng, uint32_t max_exclusive);

/* True with probability num/den (reference: next() < p). Consumes exactly one output. */
bool bk_rng_chance(bk_rng_t *rng, uint32_t num, uint32_t den);

BK_EXTERN_C_END

#endif /* BRICK_RNG_H */
