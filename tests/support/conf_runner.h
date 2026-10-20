/*
 * Game-independent replay of conformance fixtures (docs/CONFORMANCE.md): for every scenario the exact inputs of the
 * reference run are replayed into the C game, and after EVERY tick a digest of the complete observable state must equal
 * the one the reference produced. A game plugs in how to build its lab (the game plus setup helpers) and how to apply
 * the setup operations its fixture uses.
 */
#ifndef BK_CONF_RUNNER_H
#define BK_CONF_RUNNER_H

#include "bk_digest.h"
#include "bk_test.h"
#include "brick/game.h"
#include "mini_json.h"

typedef struct {
    const char *game;                                      /* "game" member of the fixture */
    size_t lab_size;                                       /* bytes the lab needs; the runner allocates and zeroes it */
    void (*init)(void *lab, const bk_game_options_t *opt); /* create AND start, like the reference plainLab */
    bk_game_t *(*game_of)(void *lab);
    /* Applies one setup operation of the fixture trace. Events raised by it (outside a step) are pushed to `pending` and
     * delivered with the next plain step, like the queue of the reference. Returns false for an unknown operation. */
    bool (*apply_op)(void *lab, const jv_t *op, bk_event_queue_t *pending);
} conf_game_t;

/* Replays all scenarios of `root`; returns how many diverged (0 = all good). Failures are also recorded in bk_test_*. */
unsigned conf_run_scenarios(const conf_game_t *cg, const jv_t *root, unsigned min_scenarios);

/* Parses the fixture at `path`, checks format and game name, and prints the reference commit. NULL on failure. */
jv_t *conf_load(const char *path, const char *game);

#endif
