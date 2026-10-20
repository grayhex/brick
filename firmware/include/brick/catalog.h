/*
 * The games that are part of the product, in menu order. This is a product decision kept out of the core library
 * (brickcore knows how to run a game, not which games ship): platform front-ends and the firmware image use it, the
 * unit tests build their own small catalogs.
 */
#ifndef BRICK_CATALOG_H
#define BRICK_CATALOG_H

#include "brick/console.h"

BK_EXTERN_C_BEGIN

/* Entries in menu order; *count receives their number. The array has static storage. */
const bk_catalog_entry_t *bk_catalog(size_t *count);

BK_EXTERN_C_END

#endif /* BRICK_CATALOG_H */
