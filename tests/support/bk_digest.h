/*
 * State digest used by the conformance harness: FNV-1a 32 over the complete observable state of one tick.
 * The byte layout is specified in docs/CONFORMANCE.md and mirrored by tools/conformance/export_tetris.ts.
 */
#ifndef BK_DIGEST_H
#define BK_DIGEST_H

#include "brick/game.h"

uint32_t bk_digest_state(const bk_game_t *g, const bk_event_t *events, unsigned count);

/* Fold to the 16 bits stored per tick in fixtures. */
static inline uint32_t bk_digest_fold16(uint32_t h)
{
    return (h ^ (h >> 16)) & 0xFFFFu;
}

#endif /* BK_DIGEST_H */
