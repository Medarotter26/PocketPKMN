// test_image.c - tests for pw_image (codec + text) and pw_menu_art.
// Build/run: tests/run_image.sh  (gcc, -fsanitize=address,undefined)
// Usage: test_image [eeprom1.bin eeprom2.bin ...]
#include "pw_image.h"
#include "pw_menu_art.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0, g_checks = 0;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; \
    printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rng = 12345u;
static uint32_t rnd(void) { rng = rng * 1103515245u + 12345u; return rng >> 8; }

// ---------------------------------------------------------------- codec
static void test_codec_synthetic(void)
{
    CHECK(pw_img_size(96, 16) == 384, "size 96x16");
    CHECK(pw_img_size(8, 8) == 16, "size 8x8");
    CHECK(pw_img_size(64, 48) == 768, "size 64x48");
    CHECK(pw_img_size(8, 12) == 0, "size with h not multiple of 8");

    // known vectors
    uint8_t sh[8 * 16], out[32], back[8 * 16];
    memset(sh, 0, sizeof sh);
    sh[0] = 3;                 // (0,0) black -> both planes bit 0
    sh[7 * 8 + 1] = 2;         // (1,7) dark  -> msb plane bit 7
    sh[8 * 8 + 2] = 1;         // (2,8) light -> second stripe, lsb plane bit 0
    pw_img_encode(sh, 8, 16, out);
    CHECK(out[0] == 0x01 && out[1] == 0x01, "black pixel -> %02X %02X", out[0], out[1]);
    CHECK(out[2] == 0x80 && out[3] == 0x00, "dark pixel -> %02X %02X", out[2], out[3]);
    CHECK(out[20] == 0x00 && out[21] == 0x01, "light pixel -> %02X %02X", out[20], out[21]);
    pw_img_decode(out, 8, 16, back);
    CHECK(memcmp(sh, back, sizeof sh) == 0, "known vector round trip");

    // random round trips both ways
    static const int dims[][2] = {{8, 8}, {16, 16}, {24, 16}, {32, 24}, {96, 16}, {96, 32},
                                  {64, 48}, {80, 16}, {16, 32}, {1, 8}, {3, 40}};
    for (size_t d = 0; d < sizeof dims / sizeof dims[0]; d++) {
        int w = dims[d][0], h = dims[d][1];
        size_t n = (size_t)w * (size_t)h, sz = pw_img_size(w, h);
        uint8_t *a = malloc(n), *b = malloc(n), *e = malloc(sz), *e2 = malloc(sz);
        for (int it = 0; it < 20; it++) {
            for (size_t i = 0; i < n; i++) a[i] = (uint8_t)(rnd() & 3u);
            pw_img_encode(a, w, h, e);
            pw_img_decode(e, w, h, b);
            CHECK(memcmp(a, b, n) == 0, "shades round trip %dx%d", w, h);
            for (size_t i = 0; i < sz; i++) e[i] = (uint8_t)rnd();
            pw_img_decode(e, w, h, b);
            pw_img_encode(b, w, h, e2);
            CHECK(memcmp(e, e2, sz) == 0, "bytes round trip %dx%d", w, h);
        }
        free(a); free(b); free(e); free(e2);
    }
}

// ------------------------------------------------------------ real dumps
static int read_file(const char *path, uint8_t *buf, size_t n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    size_t got = fread(buf, 1, n, f);
    fclose(f);
    return got == n ? 0 : -1;
}

static void roundtrip_region(const uint8_t *eep, unsigned addr, int w, int h, const char *what)
{
    size_t sz = pw_img_size(w, h);
    uint8_t *sh = malloc((size_t)w * (size_t)h), *e = malloc(sz);
    pw_img_decode(eep + addr, w, h, sh);
    pw_img_encode(sh, w, h, e);
    CHECK(memcmp(e, eep + addr, sz) == 0, "dump round trip %s 0x%04X", what, addr);
    free(sh); free(e);
}

static int test_codec_dump(const char *path)
{
    static uint8_t eep[65536];
    if (read_file(path, eep, sizeof eep) != 0) {
        CHECK(0, "cannot read %s", path);
        return 0;
    }
    CHECK(memcmp(eep, "nintendo", 8) == 0, "%s has the 'nintendo' marker", path);
    int n = 0;
    // every menu slot of the real walker, at the map's sizes
    for (int i = 0; i < PW_MENU_SLOT_COUNT; i++, n++) {
        const pw_menu_slot_t *s = &pw_menu_slots[i];
        roundtrip_region(eep, s->addr, s->w, s->h, pw_menu_slot_names[i]);
    }
    roundtrip_region(eep, 0x8FBE, 32, 24, "route image");
    roundtrip_region(eep, 0x907E, 80, 16, "route name");
    roundtrip_region(eep, 0x91BE, 32, 24, "small sprite f0");
    roundtrip_region(eep, 0x927E, 32, 24, "small sprite f1");
    roundtrip_region(eep, 0x933E, 64, 48, "large sprite f0");
    roundtrip_region(eep, 0x963E, 64, 48, "large sprite f1");
    roundtrip_region(eep, 0x993E, 80, 16, "pokemon name");
    n += 7;

    // white = 0: the digit images are mostly background
    uint8_t sh[8 * 16];
    int count[4] = {0, 0, 0, 0};
    for (int d = 0; d < 13; d++) {
        pw_img_decode(eep + 0x0280 + d * 32, 8, 16, sh);
        for (int i = 0; i < 8 * 16; i++) count[sh[i]]++;
    }
    CHECK(count[0] > count[1] + count[2] + count[3], "shade 0 is the background in %s", path);
    printf("  %s: %d images round-tripped; digit shades 0/1/2/3 = %d/%d/%d/%d\n",
           path, n, count[0], count[1], count[2], count[3]);
    return n;
}

// ------------------------------------------------------------------ text
static int glyph_w(char c) { return pw_font_glyphs[(unsigned char)c - 0x20].w; }

static const pw_menu_slot_t *slot_at(unsigned addr)
{
    for (int i = 0; i < PW_MENU_SLOT_COUNT; i++)
        if (pw_menu_slots[i].addr == addr) return &pw_menu_slots[i];
    return NULL;
}

static void test_text(void)
{
    CHECK(pw_text_width("") == 0, "empty width");
    CHECK(pw_text_width("i") == glyph_w('i'), "width i");
    CHECK(pw_text_width("ab") == glyph_w('a') + 1 + glyph_w('b'), "width ab");
    CHECK(pw_text_width("ab\nabcd") == pw_text_width("abcd"), "width = widest line");
    CHECK(pw_text_width("\xC3\xA9") == pw_font_glyphs[PW_FONT_E_ACUTE].w, "width e-acute");
    CHECK(pw_text_width("\xC3\x89") == pw_font_glyphs[PW_FONT_CE_ACUTE].w, "width E-acute");
    CHECK(pw_text_width("\xE2\x82\xAC") == glyph_w('?'), "unknown char drawn as ?");
    // v0.81 (names step 1, owner: "Go 12"): the new glyphs, aliases, and what stays '?'
    CHECK(PW_FONT_COUNT == 144, "font count");
    for (int i = 0; i < PW_FONT_COUNT - PW_FONT_E_ACUTE; i++)
        CHECK(pw_glyph_for(pw_font_extra_cp[i]) == PW_FONT_E_ACUTE + i, "extra glyph %d", i);
    CHECK(pw_glyph_for(0xE9) == PW_FONT_E_ACUTE && pw_glyph_for(0xC9) == PW_FONT_CE_ACUTE, "e-acute index kept");
    CHECK(pw_text_width("\xC3\x9F") == pw_font_glyphs[pw_glyph_for(0xDF)].w, "width sharp s");
    CHECK(pw_text_width("\xE2\x99\x82") == pw_font_glyphs[pw_glyph_for(0x2642)].w, "width male");
    CHECK(pw_text_width("\xE2\x80\x99") == glyph_w('\''), "U+2019 drawn as '");
    CHECK(pw_text_width("\xE2\x80\xA4") == glyph_w('.'), "U+2024 drawn as .");
    CHECK(pw_glyph_for(0xE08E) == pw_glyph_for(0x2642) && pw_glyph_for(0xE08F) == pw_glyph_for(0x2640), "E08E/E08F");
    CHECK(pw_glyph_for(0x3042) == -1 && pw_glyph_for(0x0A) == -1 && pw_glyph_for(0xAC00) == -1, "no glyph");
    CHECK(pw_text_width("\xC1\xA1") == glyph_w('?'), "overlong 2-byte form stays ?");
    CHECK(pw_text_width("\xE0\x81\xA1") == glyph_w('?'), "overlong 3-byte form stays ?");
    CHECK(pw_text_width("\xC3") == glyph_w('?'), "cut sequence stays ?");
    CHECK(pw_text_width("\xE3\x81\x82") == glyph_w('?'), "kana stays ?");

    // font sanity: every glyph 1..8 wide, nothing outside its width
    for (int g = 0; g < PW_FONT_COUNT; g++) {
        const pw_glyph_t *gl = &pw_font_glyphs[g];
        CHECK(gl->w >= 1 && gl->w <= 8, "glyph %d width %d", g, gl->w);
        uint8_t mask = (uint8_t)(0xFFu << (8 - gl->w));
        int ink = 0;
        for (int r = 0; r < PW_FONT_ROWS; r++) {
            CHECK((gl->rows[r] & (uint8_t)~mask) == 0, "glyph %d row %d outside width", g, r);
            ink |= gl->rows[r];
        }
        CHECK((g == 0) == (ink == 0), "glyph %d ink (only space is blank)", g);
    }
    // cap height 9: 'H' uses rows 2..10 exactly
    for (int r = 0; r < PW_FONT_ROWS; r++)
        CHECK((pw_font_glyphs['H' - 0x20].rows[r] != 0) == (r >= 2 && r <= 10), "H row %d", r);

    uint8_t img[384 * 2], sh[96 * 32];
    // fits, black on white, 1-px left margin, centred: caps on rows 3..11
    CHECK(pw_text_image("Connecting...", 96, 16, img) == 0, "Connecting... fits");
    pw_img_decode(img, 96, 16, sh);
    int ink_col0 = 0, bad_shade = 0, top = 99, bottom = -1, ink = 0;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 96; x++) {
            uint8_t v = sh[y * 96 + x];
            if (v != 0 && v != 3) bad_shade++;
            if (v) {
                ink++;
                if (x == 0) ink_col0++;
                if (y < top) top = y;
                if (y > bottom) bottom = y;
            }
        }
    CHECK(ink > 0 && bad_shade == 0 && ink_col0 == 0, "text shades/margin");
    CHECK(top == 3 && bottom == 14, "Connecting... rows %d..%d (want 3..14 with g descender)", top, bottom);
    int rightmost = -1;
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 96; x++)
            if (sh[y * 96 + x] && x > rightmost) rightmost = x;
    CHECK(rightmost == pw_text_width("Connecting..."), "right edge %d", rightmost);

    // clipping
    CHECK(pw_text_image("WWWWWWWWWWWWWWWW", 96, 16, img) == -1, "too wide -> -1");
    pw_img_decode(img, 96, 16, sh);
    ink = 0;
    for (int i = 0; i < 96 * 16; i++) ink += sh[i] != 0;
    CHECK(ink > 0, "clipped text still rendered");
    CHECK(pw_text_image("one\ntwo", 96, 16, img) == -1, "two lines in 16 px -> -1");
    CHECK(pw_text_image("one\ntwo", 96, 32, img) == 0, "two lines in 32 px fit");
    CHECK(pw_text_image("a\nb\nc", 96, 32, img) == -1, "three lines in 32 px -> -1");
    // a single line in a 32-px image is centred vertically
    CHECK(pw_text_image("H", 8, 32, img) == 0, "H in 8x32");
    pw_img_decode(img, 8, 32, sh);
    top = 99; bottom = -1;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 8; x++)
            if (sh[y * 8 + x]) { if (y < top) top = y; if (y > bottom) bottom = y; }
    CHECK(top == 8 + 3 && bottom == 8 + 11, "H centred in 32 px: rows %d..%d", top, bottom);
    // exact fit boundary: width w+1 fits in w+1+... (margin 1)
    int tw = pw_text_width("Hi");
    CHECK(pw_text_image("Hi", tw + 1, 16, img) == 0, "exact fit");
    CHECK(pw_text_image("Hi", tw, 16, img) == -1, "one px short");

    // C renderer == Python generator for the text slots (same font, same rules)
    static const struct { unsigned addr; int w, h; const char *s; } same[] = {
        {0x2530, 96, 16, "Connecting..."},
        {0x4330, 96, 16, "Need more Watts."},
        {0x44B0, 96, 16, "No Pok\xC3\xA9mon held!"},
        {0x2830, 96, 32, "Cannot complete\nthis connection."},
        {0x80B0, 96, 16, "is looking this way."},
        {0x8B30, 80, 16, "Switch?"},
        {0x1BD0, 32, 16, "Left:"},
    };
    for (size_t i = 0; i < sizeof same / sizeof same[0]; i++) {
        const pw_menu_slot_t *s = slot_at(same[i].addr);
        CHECK(s && s->data && s->w == same[i].w && s->h == same[i].h, "slot 0x%04X", same[i].addr);
        if (!s || !s->data) continue;
        CHECK(pw_text_image(same[i].s, same[i].w, same[i].h, img) == 0, "fits 0x%04X", same[i].addr);
        CHECK(memcmp(img, s->data, pw_img_size(same[i].w, same[i].h)) == 0,
              "C text == generated art at 0x%04X", same[i].addr);
    }
}

// ------------------------------------------------------------- menu art
// Slot list from Dmitry Grinberg's EEPROM map, typed in independently of
// tools/pwimg.py: address, width, height.
static const struct { uint16_t addr; uint8_t w, h; } MAP[] = {
    {0x0280,8,16},{0x02A0,8,16},{0x02C0,8,16},{0x02E0,8,16},{0x0300,8,16},{0x0320,8,16},
    {0x0340,8,16},{0x0360,8,16},{0x0380,8,16},{0x03A0,8,16},{0x03C0,8,16},{0x03E0,8,16},
    {0x0400,8,16},                                   // digits 0-9 : - /
    {0x0420,16,16},                                  // watt
    {0x0460,8,8},{0x0470,8,8},{0x0488,8,8},{0x0498,8,8},{0x04A8,8,8},
    {0x04B8,8,8},{0x04C8,8,8},{0x04D8,8,8},{0x04E8,8,8},  // card faces
    {0x04F8,8,8},{0x0508,8,8},{0x0518,8,8},{0x0528,8,8},{0x0538,8,8},{0x0548,8,8},
    {0x0558,8,8},{0x0568,8,8},{0x0578,8,8},{0x0588,8,8},{0x0598,8,8},{0x05A8,8,8},
    {0x05B8,8,16},{0x05D8,8,16},{0x05F8,8,16},
    {0x0638,8,8},{0x0648,4,8},                       // more-message mark + 8-byte mask
    {0x0650,8,8},{0x0660,8,8},
    {0x0670,24,16},{0x06D0,24,16},{0x0730,24,16},{0x0790,24,16},{0x07F0,24,16},{0x0850,24,16},
    {0x08B0,24,16},
    {0x0910,80,16},{0x0A50,80,16},{0x0B90,80,16},{0x0CD0,80,16},{0x0E10,80,16},{0x0F50,80,16},
    {0x1090,16,16},{0x10D0,16,16},{0x1110,16,16},{0x1150,16,16},{0x1190,16,16},{0x11D0,16,16},
    {0x1210,16,16},{0x1250,80,16},{0x1390,16,16},
    {0x13D0,40,16},{0x1470,32,16},{0x14F0,40,16},{0x1590,64,16},
    {0x1690,40,16},{0x1730,40,16},{0x17D0,24,16},{0x1830,24,16},{0x1890,24,16},
    {0x18F0,8,16},{0x1910,32,24},{0x19D0,32,24},{0x1A90,32,24},{0x1B50,16,16},{0x1B90,16,16},
    {0x1BD0,32,16},{0x1C50,16,24},{0x1CB0,32,24},
    {0x1D70,16,16},{0x1DB0,16,16},{0x1DF0,16,16},{0x1E30,16,16},
    {0x1E70,16,32},{0x1EF0,16,32},{0x1F70,32,24},{0x2030,8,8},{0x2040,8,8},
    {0x2050,96,32},{0x2350,32,32},{0x2450,8,16},{0x2470,8,8},{0x2480,8,8},{0x2490,40,16},
    {0x2530,96,16},{0x26B0,96,16},{0x2830,96,32},{0x2B30,96,16},{0x2CB0,96,32},{0x2FB0,96,32},
    {0x32B0,96,32},{0x35B0,96,32},{0x38B0,96,16},{0x3A30,96,16},{0x3BB0,96,16},{0x3D30,96,16},
    {0x3EB0,96,16},{0x4030,96,16},{0x41B0,96,16},{0x4330,96,16},{0x44B0,96,16},{0x4630,96,16},
    {0x47B0,96,16},{0x4930,96,16},{0x4AB0,96,16},{0x4C30,96,16},{0x4DB0,96,16},{0x4F30,96,16},
    {0x50B0,96,16},{0x5230,96,16},{0x53B0,96,16},{0x5530,96,16},{0x56B0,96,16},{0x5830,96,16},
    {0x59B0,96,16},{0x5B30,96,16},{0x5CB0,96,16},{0x5E30,96,16},{0x5FB0,96,16},{0x6130,96,16},
    {0x62B0,96,16},{0x6430,96,16},{0x65B0,96,16},{0x6730,96,16},{0x68B0,96,16},{0x6A30,96,16},
    {0x6BB0,96,16},{0x6D30,96,16},{0x6EB0,96,16},{0x7030,96,16},{0x71B0,96,16},{0x7330,96,16},
    {0x74B0,96,16},{0x7630,96,16},{0x77B0,96,16},{0x7930,96,16},{0x7AB0,96,16},{0x7C30,96,16},
    {0x7DB0,96,16},{0x7F30,96,16},{0x80B0,96,16},{0x8230,96,16},{0x83B0,96,16},{0x8530,96,16},
    {0x86B0,96,16},{0x8830,96,16},{0x89B0,96,16},
    {0x8B30,80,16},                                  // "Switch?"
};

static void test_menu_art(void)
{
    const int nmap = (int)(sizeof MAP / sizeof MAP[0]);
    CHECK(PW_MENU_SLOT_COUNT == nmap, "slot count %d, map has %d", PW_MENU_SLOT_COUNT, nmap);
    for (int i = 0; i < PW_MENU_SLOT_COUNT && i < nmap; i++) {
        const pw_menu_slot_t *s = &pw_menu_slots[i];
        CHECK(s->addr == MAP[i].addr && s->w == MAP[i].w && s->h == MAP[i].h,
              "slot %d (%s) is 0x%04X %dx%d, map says 0x%04X %dx%d", i, pw_menu_slot_names[i],
              s->addr, s->w, s->h, MAP[i].addr, MAP[i].w, MAP[i].h);
        size_t n = pw_menu_slot_size(s);
        CHECK(n == pw_img_size(s->w, s->h) && n > 0, "slot %d size", i);
        CHECK(s->addr >= PW_MENU_ART_START && s->addr + n <= PW_MENU_ART_END,
              "slot %s inside 0x0280-0x8C6F", pw_menu_slot_names[i]);
        if (i > 0) {
            const pw_menu_slot_t *p = &pw_menu_slots[i - 1];
            CHECK(p->addr + pw_menu_slot_size(p) <= s->addr, "slot %s overlaps the one before",
                  pw_menu_slot_names[i]);
        }
    }
    CHECK(pw_menu_slots[0].addr == 0x0280, "first slot 0x0280");
    const pw_menu_slot_t *last = &pw_menu_slots[PW_MENU_SLOT_COUNT - 1];
    CHECK(last->addr + pw_menu_slot_size(last) == 0x8C70, "last slot ends at 0x8C6F");
    // trainer name stays white (filled at run time)
    const pw_menu_slot_t *tn = slot_at(0x1250);
    CHECK(tn && tn->data == NULL, "trainer name slot left white");

    // fill writes only inside 0x0280-0x8C6F; try two different backgrounds
    static uint8_t eep[65536], eep2[65536];
    memset(eep, 0xA5, sizeof eep);
    memset(eep2, 0x5A, sizeof eep2);
    pw_menu_art_fill(eep);
    pw_menu_art_fill(eep2);
    int outside = 0;
    for (unsigned a = 0; a < 65536; a++) {
        int in = a >= 0x0280 && a < 0x8C70;
        if (!in && (eep[a] != 0xA5 || eep2[a] != 0x5A)) outside++;
        if (in && eep[a] != eep2[a]) outside++;    // inside: fully determined by the art
    }
    CHECK(outside == 0, "fill touched %d bytes it should not", outside);
    for (int i = 0; i < PW_MENU_SLOT_COUNT; i++) {
        const pw_menu_slot_t *s = &pw_menu_slots[i];
        size_t n = pw_menu_slot_size(s);
        if (s->data) {
            CHECK(memcmp(eep + s->addr, s->data, n) == 0, "slot %s written", pw_menu_slot_names[i]);
        } else {
            for (size_t k = 0; k < n; k++)
                if (eep[s->addr + k]) { CHECK(0, "white slot %s not white", pw_menu_slot_names[i]); break; }
        }
    }
    for (unsigned a = 0x8C70; a < 0x8F00; a++)
        if (eep[a] != 0xA5) { CHECK(0, "0x%04X touched", a); break; }
}

int main(int argc, char **argv)
{
    test_codec_synthetic();
    for (int i = 1; i < argc; i++)
        test_codec_dump(argv[i]);
    test_text();
    test_menu_art();
    printf("test_image: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
