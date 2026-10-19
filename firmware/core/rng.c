#include "brick/rng.h"

void bk_rng_seed(bk_rng_t *rng, uint32_t seed)
{
    rng->state = seed;
}

/* mulberry32; see rng.h. All arithmetic is modulo 2^32, exactly like Math.imul / >>> 0 in the reference. */
uint32_t bk_rng_next_u32(bk_rng_t *rng)
{
    uint32_t next = rng->state + 0x6d2b79f5u;
    rng->state = next;
    uint32_t t = (next ^ (next >> 15)) * (next | 1u);
    t = (t + (t ^ (t >> 7)) * (t | 61u)) ^ t;
    return t ^ (t >> 14);
}

uint32_t bk_rng_int(bk_rng_t *rng, uint32_t max_exclusive)
{
    if (max_exclusive <= 1u) {
        return 0u;
    }
    uint32_t r = bk_rng_next_u32(rng);
    /* floor(r / 2^32 * max) */
    return (uint32_t)(((uint64_t)r * max_exclusive) >> 32);
}

bool bk_rng_chance(bk_rng_t *rng, uint32_t num, uint32_t den)
{
    uint32_t r = bk_rng_next_u32(rng);
    /* r / 2^32 < num / den  <=>  r * den < num * 2^32 */
    return ((uint64_t)r * den) < ((uint64_t)num << 32);
}
