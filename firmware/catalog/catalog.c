#include "brick/catalog.h"

#include "brick/games/racing.h"
#include "brick/games/snake.h"
#include "brick/games/tetris.h"

/* Add ported games here; the glyph is what the menu shows big ("1".."9", "10".., "A".. - one or two characters). */
static const bk_catalog_entry_t CATALOG[] = {
    {&bk_tetris_def, "1"},
    {&bk_snake_def, "2"},
    {&bk_racing_def, "3"},
};

const bk_catalog_entry_t *bk_catalog(size_t *count)
{
    if (count != NULL) {
        *count = sizeof CATALOG / sizeof CATALOG[0];
    }
    return CATALOG;
}
