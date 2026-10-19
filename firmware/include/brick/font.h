/*
 * Built-in bitmap fonts. Latin only on purpose: the product is a classic Brick Game, and the PRD does not require
 * localisation. (A Cyrillic font would need a 5x7 face; revisit when a language list is decided.)
 *
 *  - 3x5 text font: A-Z, 0-9 and a few symbols, scalable by an integer factor.
 *  - 4x7 big glyphs for the game number in the menu (digits and letters), 3x5 small digits for the mode number.
 */
#ifndef BRICK_FONT_H
#define BRICK_FONT_H

#include "brick/frame.h"

BK_EXTERN_C_BEGIN

#define BK_GLYPH_W 3
#define BK_GLYPH_H 5

/* Pixel pitch of one character at `scale` (glyph width + one dot gap). */
static inline int bk_text_pitch(int scale)
{
    return (BK_GLYPH_W + 1) * scale;
}

int bk_text_width(const char *s, int scale);
int bk_text_height(int scale);

/* Draw upper-case ASCII text with ink dots. Lower case is folded to upper; unknown characters are blank. */
void bk_text_draw(bk_frame_t *f, int x, int y, int scale, const char *s);

/* One glyph as 5 rows of 3 bits (bit 2 = leftmost); returns false if the character has no glyph. */
bool bk_font_glyph(char c, uint8_t rows[BK_GLYPH_H]);

/* Menu sprites on the cell matrix: '#' = lit cell. */
#define BK_BIG_W 4
#define BK_BIG_H 7
const char *const *bk_big_glyph(char c);  /* 7 rows of 4 chars, or NULL */
const char *const *bk_small_digit(int d); /* 5 rows of 3 chars, d = 0..9 */

BK_EXTERN_C_END

#endif /* BRICK_FONT_H */
