/* Generated bitmap glyphs: Atkinson Hyperlegible Next and Noto Sans CJK SC, both OFL-1.1.
 * Font license: licenses/Atkinson-OFL.txt; see THIRD_PARTY_NOTICES.md. */
#ifndef HOME_FONT_H
#define HOME_FONT_H
#include <stdint.h>
typedef struct { uint32_t codepoint, offset; uint8_t width, height;
                 int8_t left, top; uint8_t advance; } home_glyph_t;
typedef struct { uint8_t size; uint16_t first, count; } home_font_t;
extern const uint8_t home_font_bits[];
extern const home_glyph_t home_glyphs[];
extern const home_font_t home_fonts[8];
/* Simplified Chinese (Noto Sans CJK SC, OFL-1.1): GB 2312 level 1 + punctuation, sizes 30/22/16/12. */
extern const home_font_t home_cjk_fonts[4];
#endif
