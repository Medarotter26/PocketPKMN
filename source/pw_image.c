// pw_image.c - Pokewalker 2bpp image codec + text rendering (portable C99).
// See include/pw_image.h for the format.
#include "pw_image.h"

#include <string.h>

size_t pw_img_size(int w, int h)
{
    if (w <= 0 || h <= 0 || (h % 8) != 0)
        return 0;
    return (size_t)w * (size_t)(h / 8) * 2u;
}

void pw_img_encode(const uint8_t *shades, int w, int h, uint8_t *out)
{
    int stripes = h / 8;
    for (int s = 0; s < stripes; s++) {
        for (int x = 0; x < w; x++) {
            uint8_t hi = 0, lo = 0;
            for (int b = 0; b < 8; b++) {
                uint8_t v = shades[(size_t)(s * 8 + b) * (size_t)w + (size_t)x] & 3u;
                hi |= (uint8_t)(((v >> 1) & 1u) << b);
                lo |= (uint8_t)((v & 1u) << b);
            }
            size_t o = ((size_t)s * (size_t)w + (size_t)x) * 2u;
            out[o] = hi;
            out[o + 1] = lo;
        }
    }
}

void pw_img_decode(const uint8_t *in, int w, int h, uint8_t *shades)
{
    int stripes = h / 8;
    for (int s = 0; s < stripes; s++) {
        for (int x = 0; x < w; x++) {
            size_t o = ((size_t)s * (size_t)w + (size_t)x) * 2u;
            uint8_t hi = in[o], lo = in[o + 1];
            for (int b = 0; b < 8; b++) {
                shades[(size_t)(s * 8 + b) * (size_t)w + (size_t)x] =
                    (uint8_t)((((hi >> b) & 1u) << 1) | ((lo >> b) & 1u));
            }
        }
    }
}

// Next code point from UTF-8 as a glyph index, or -1 for '\n', -2 at the end.
static int next_glyph(const char **pp)
{
    const unsigned char *p = (const unsigned char *)*pp;
    unsigned c = *p;
    if (c == 0)
        return -2;
    if (c == '\n') {
        *pp = (const char *)(p + 1);
        return -1;
    }
    if (c < 0x80) {
        *pp = (const char *)(p + 1);
        if (c >= 0x20 && c <= 0x7E)
            return (int)(c - 0x20);
        return '?' - 0x20;
    }
    // multi-byte sequence: work out its length, skip it as one character
    int len = 1;
    if ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;
    for (int i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80) { len = i; break; }
    }
    *pp = (const char *)(p + len);
    if (len == 2 && c == 0xC3) {
        if (p[1] == 0xA9) return PW_FONT_E_ACUTE;
        if (p[1] == 0x89) return PW_FONT_CE_ACUTE;
    }
    return '?' - 0x20;
}

int pw_text_width(const char *utf8)
{
    int best = 0, cur = 0;
    const char *p = utf8;
    for (;;) {
        int g = next_glyph(&p);
        if (g < 0) {
            if (cur > 0) cur -= 1;       // no gap after the last glyph
            if (cur > best) best = cur;
            if (g == -2) break;
            cur = 0;
            continue;
        }
        cur += pw_font_glyphs[g].w + 1;
    }
    return best;
}

static void put_black(uint8_t *out, int w, int h, int x, int y)
{
    if (x < 0 || y < 0 || x >= w || y >= h)
        return;
    size_t o = ((size_t)(y / 8) * (size_t)w + (size_t)x) * 2u;
    uint8_t bit = (uint8_t)(1u << (y % 8));
    out[o] |= bit;
    out[o + 1] |= bit;
}

int pw_text_image(const char *utf8, int w, int h, uint8_t *out)
{
    size_t size = pw_img_size(w, h);
    if (size == 0)
        return -1;
    memset(out, 0, size);

    // count lines
    int lines = 1;
    for (const char *q = utf8; *q; q++)
        if (*q == '\n') lines++;

    int slots = h / PW_FONT_LINE_H;
    int fit = 0;
    int y0 = 0;
    if (slots < 1) {
        // image shorter than one line: centre the line, it will be clipped
        y0 = (h - PW_FONT_LINE_H) / 2;
        fit = -1;
    } else if (lines <= slots) {
        y0 = (h - lines * PW_FONT_LINE_H) / 2;
    } else {
        fit = -1;
    }

    const char *p = utf8;
    int line = 0, x = 1;
    for (;;) {
        int g = next_glyph(&p);
        if (g == -2)
            break;
        if (g == -1) {
            line++;
            x = 1;
            continue;
        }
        const pw_glyph_t *gl = &pw_font_glyphs[g];
        int ly = y0 + line * PW_FONT_LINE_H + PW_FONT_Y_OFFSET;
        if (line >= slots && slots >= 1) {
            fit = -1;      // line has no room at all
        } else {
            if (x + gl->w > w)
                fit = -1;
            for (int r = 0; r < PW_FONT_ROWS; r++) {
                uint8_t bits = gl->rows[r];
                if (!bits) continue;
                for (int c = 0; c < gl->w; c++)
                    if (bits & (0x80u >> c))
                        put_black(out, w, h, x + c, ly + r);
            }
        }
        x += gl->w + 1;
    }
    return fit;
}
