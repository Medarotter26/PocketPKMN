// pw_image.h - Pokewalker 2bpp image codec + text rendering (portable C99).
//
// Walker image format: the image is cut into 8-pixel-tall horizontal stripes,
// top to bottom. Each stripe is stored left to right, 2 bytes per column:
// byte 0 = more significant bitplane, byte 1 = less significant bitplane.
// Bit 0 (LSB) of each byte is the TOP pixel of the stripe, bit 7 the bottom.
// Pixel value = (msb_bit << 1) | lsb_bit. Size = w * (h / 8) * 2 bytes.
//
// Shades (checked against real EEPROM dumps: the digit images at 0x0280 are
// mostly value 0, and the walker shows them as dark text on a light screen):
//   0 = white (background), 1 = light grey, 2 = dark grey, 3 = black.
// The value is stored as-is, the encoder does no inversion.
#ifndef PW_IMAGE_H
#define PW_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PW_SHADE_WHITE 0
#define PW_SHADE_LIGHT 1
#define PW_SHADE_DARK  2
#define PW_SHADE_BLACK 3

// w * (h/8) * 2; h must be a multiple of 8 (returns 0 otherwise).
size_t pw_img_size(int w, int h);

// shades: row-major w*h, values 0..3 (higher bits ignored).
void pw_img_encode(const uint8_t *shades, int w, int h, uint8_t *out);
void pw_img_decode(const uint8_t *in, int w, int h, uint8_t *shades);

// Render UTF-8 text (ASCII + é/É + "\n" for a second line when h >= 32) into a
// w x h walker image, black text on white, vertically centred per 16-px line,
// left aligned with a 1-px margin. Unknown characters are drawn as '?'.
// If the image has more 16-px lines than the text, the text block is centred
// vertically in the image. Returns 0 if it fit, -1 if it was cut off (still
// renders what fits).
int pw_text_image(const char *utf8, int w, int h, uint8_t *out);

// Width in pixels the text would take on one line (no margin, no trailing
// gap). For text with "\n" it is the width of the widest line.
int pw_text_width(const char *utf8);

// ---- font data (source/pw_font.c, generated from art/font.txt) -----------
// Every glyph is PW_FONT_ROWS rows tall. Row 0-1: accent zone above capitals,
// rows 2-10: cap height (9 px, baseline under row 10), rows 11-13: descender.
// Each row is a bitmask, bit 7 = leftmost pixel; width <= 8. Advance = w + 1.
// In a 16-px line, glyph row 0 is drawn at line row 1 (caps on rows 3-11).
#define PW_FONT_ROWS     14
#define PW_FONT_LINE_H   16
#define PW_FONT_Y_OFFSET 1
#define PW_FONT_COUNT    97   // 0x20..0x7E, then e-acute (95), E-acute (96)
#define PW_FONT_E_ACUTE  95
#define PW_FONT_CE_ACUTE 96

typedef struct {
    uint8_t w;
    uint8_t rows[PW_FONT_ROWS];
} pw_glyph_t;

extern const pw_glyph_t pw_font_glyphs[PW_FONT_COUNT];

#ifdef __cplusplus
}
#endif

#endif
