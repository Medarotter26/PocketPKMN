// test_sprites.c - PC checks of the sprite pack loader (source/wbx_sprites.c).
// Usage: test_sprites <pack dir> <walker backup>   (pack dir = .../3ds/walkerbox/sprites)
// The loader reads WBX_SPRITE_DIR; this test is built with that pointed at the PC pack.
#include <stdio.h>
#include <string.h>

#include "pw_image.h"
#include "wbx_sprites.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(int argc, char **argv)
{
	if (argc < 2) {
		printf("usage: test_sprites <walker backup>\n");
		return 2;
	}
	static uint8_t a[WBX_SPRITE_SIZE], b[WBX_SPRITE_SIZE], ee[0x10000];
	FILE *f = fopen(argv[1], "rb");
	CHECK(f && fread(ee, 1, sizeof ee, f) == sizeof ee, "read %s", argv[1]);
	if (f)
		fclose(f);

	// placeholder: deterministic, has black ("?") and dark grey (box) in both frames of both sizes, frames differ
	wbx_sprite_placeholder(a);
	wbx_sprite_placeholder(b);
	CHECK(memcmp(a, b, sizeof a) == 0, "placeholder not deterministic");
	const struct { int off, w, h; } fr[4] = { { 0, 32, 24 }, { 0xC0, 32, 24 }, { 0x180, 64, 48 }, { 0x480, 64, 48 } };
	for (int i = 0; i < 4; i++) {
		uint8_t sh[64 * 48];
		int count[4] = { 0 };
		pw_img_decode(a + fr[i].off, fr[i].w, fr[i].h, sh);
		for (int p = 0; p < fr[i].w * fr[i].h; p++)
			count[sh[p] & 3]++;
		CHECK(count[3] > 0 && count[2] > 0, "placeholder frame %d: black %d dark %d", i, count[3], count[2]);
	}
	CHECK(memcmp(a, a + 0xC0, 0xC0) != 0, "placeholder small frames equal");
	CHECK(memcmp(a + 0x180, a + 0x480, 0x300) != 0, "placeholder large frames equal");

	// out of range / missing -> placeholder, 0
	CHECK(wbx_sprite_load(0, b) == 0 && memcmp(a, b, sizeof a) == 0, "dex 0 not placeholder");
	CHECK(wbx_sprite_load(808, b) == 0 && memcmp(a, b, sizeof a) == 0, "dex 808 not placeholder");

	// the pack: #161 equals the walker backup's walking Sentret (0x91BE-0x993D); every file loads
	CHECK(wbx_sprite_load(161, b) == 1, "161 not in pack");
	CHECK(memcmp(b, ee + 0x91BE, WBX_SPRITE_SIZE) == 0, "161 differs from the walker backup");
	int loaded = 0;
	for (int d = 1; d <= WBX_SPRITE_LAST; d++)
		loaded += wbx_sprite_load(d, b);
	CHECK(loaded == WBX_SPRITE_LAST, "%d of %d sprites loaded", loaded, WBX_SPRITE_LAST);

	printf("%s (%d failure(s))\n", fails ? "FAILED" : "ALL SPRITE CHECKS PASSED", fails);
	return fails != 0;
}
