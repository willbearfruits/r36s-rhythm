// Fixed-width bitmap fonts (Terminus), the sizes the mockups use. Glyphs are rows of bits, MSB first.
#pragma once
#include "lib.h"

typedef struct { uint8_t w, h, bpr; const uint8_t *bits; } font_t;
extern const font_t f12, f12b, f14, f14n, f16, f20, f24, f32;
extern const uint16_t font_codes[];      // the characters each font has, in glyph order
extern const int font_ncodes;
int font_glyph(uint32_t code);           // the glyph index of a code point, or the one for '?'
