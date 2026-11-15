// test_sprite_gen.c - wbx_sprite_load_for: Gen 1/2 games use the Gen 1/2 set's LARGE pictures (romfs/sprites/gen12,
// Unown by letter) with the pack's (HGSS) SMALL pictures (v0.45, user); everything else the pack; a missing Gen 1/2
// file falls back to the pack.
#include <stdio.h>
#include <string.h>

#include "wbx_sprites.h"

static int fails;
static int same_file(const char *path, const unsigned char *buf)
{
	unsigned char f[WBX_SPRITE_SIZE];
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return 0;
	size_t n = fread(f, 1, sizeof f, fp);
	fclose(fp);
	return n == sizeof f && !memcmp(f, buf, sizeof f);
}
// buf = small part of the pack file + large part of the gen12 file
static int same_mixed(const char *g12, const char *pack, const unsigned char *buf)
{
	unsigned char a[WBX_SPRITE_SIZE], b[WBX_SPRITE_SIZE];
	FILE *f1 = fopen(g12, "rb"), *f2 = fopen(pack, "rb");
	int ok = f1 && f2 && fread(a, 1, sizeof a, f1) == sizeof a && fread(b, 1, sizeof b, f2) == sizeof b;
	if (f1)
		fclose(f1);
	if (f2)
		fclose(f2);
	return ok && !memcmp(buf, b, WBX_SPRITE_SMALL_SIZE) &&
	       !memcmp(buf + WBX_SPRITE_SMALL_SIZE, a + WBX_SPRITE_SMALL_SIZE, WBX_SPRITE_LARGE_SIZE);
}
#define CHECK(c, m) do { if (!(c)) { fails++; printf("FAIL: %s\n", m); } } while (0)

int main(void)
{
	static unsigned char b[WBX_SPRITE_SIZE];
	char p[300];
	char q[300];
	CHECK(wbx_sprite_load_for(25, 0, 2, b) == 1, "Pikachu gen 2 loads");
	snprintf(p, sizeof p, WBX_SPRITE12_DIR "/025.bin");
	snprintf(q, sizeof q, WBX_SPRITE_DIR "/025.bin");
	CHECK(same_mixed(p, q, b), "Pikachu gen 2 = pack small + gen12/025.bin large");
	CHECK(wbx_sprite_load_for(25, 0, 1, b) == 1 && same_mixed(p, q, b), "Pikachu gen 1 = pack small + gen12 large");
	snprintf(p, sizeof p, WBX_SPRITE_DIR "/025.bin");
	CHECK(wbx_sprite_load_for(25, 0, 4, b) == 1 && same_file(p, b), "Pikachu gen 4 = pack");
	CHECK(wbx_sprite_load_for(25, 0, 3, b) == 1 && same_file(p, b), "Pikachu gen 3 = pack (no Gen 3 set yet)");
	snprintf(p, sizeof p, WBX_SPRITE12_DIR "/201B.bin");
	snprintf(q, sizeof q, WBX_SPRITE_DIR "/201.bin");
	CHECK(wbx_sprite_load_for(201, 1, 2, b) == 1 && same_mixed(p, q, b), "Unown B = pack small + gen12/201B.bin large");
	snprintf(p, sizeof p, WBX_SPRITE12_DIR "/201.bin");
	CHECK(wbx_sprite_load_for(201, 0, 2, b) == 1 && same_mixed(p, q, b), "Unown A = pack small + gen12/201.bin large");
	snprintf(p, sizeof p, WBX_SPRITE_DIR "/252.bin");
	CHECK(wbx_sprite_load_for(252, 0, 2, b) == 1 && same_file(p, b), "#252 (not Gen 1/2) = pack");
	int n12 = 0;
	for (int d = 1; d <= 251; d++) {
		snprintf(p, sizeof p, WBX_SPRITE12_DIR "/%03d.bin", d);
		snprintf(q, sizeof q, WBX_SPRITE_DIR "/%03d.bin", d);
		n12 += wbx_sprite_load_for(d, 0, 2, b) == 1 && same_mixed(p, q, b);
	}
	CHECK(n12 == 251, "all 251 Gen 1/2 files used");
	printf("gen12 files used for #1-251: %d; %s\n", n12, fails ? "SPRITE GEN TESTS FAILED" : "sprite gen tests passed");
	return fails != 0;
}
