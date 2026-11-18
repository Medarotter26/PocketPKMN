// test_sprite_gen.c - wbx_sprite_load_for: Gen 1/2 games use the Gen 1/2 set's LARGE pictures (romfs/sprites/gen12,
// Unown by letter) with the pack's (HGSS) SMALL pictures (v0.45, user); v0.88: Gen 3 games the same way with the Gen 3
// set (romfs/sprites/gen3, Unown A-Z ! ?); everything else the pack; a missing Gen 1/2 / Gen 3 file falls back to the
// pack. v0.90: Gen 5-7 games use romfs/sprites/forms (form 0 overrides, Pyroar female); Gen 4 never.
#define _DEFAULT_SOURCE
#include <dirent.h>
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
	snprintf(q, sizeof q, WBX_SPRITE3_DIR "/025.bin"); // v0.88: the Gen 3 set
	CHECK(wbx_sprite_load_for(25, 0, 3, b) == 1 && same_mixed(q, p, b), "Pikachu gen 3 = pack small + gen3/025.bin large");
	for (int g = 4; g <= 7; g++) // Gen 4-7 games never use the Gen 3 set
		CHECK(wbx_sprite_load_for(25, 0, g, b) == 1 && same_file(p, b), "Pikachu gen 4-7 = pack");
	snprintf(p, sizeof p, WBX_SPRITE_DIR "/201.bin");
	static const struct { int form; const char *file; } un3[] = { { 0, "201" }, { 1, "201B" }, { 25, "201Z" },
	                                                            { 26, "201EM" }, { 27, "201QM" } };
	for (size_t i = 0; i < sizeof un3 / sizeof un3[0]; i++) {
		snprintf(q, sizeof q, WBX_SPRITE3_DIR "/%s.bin", un3[i].file);
		CHECK(wbx_sprite_load_for(201, un3[i].form, 3, b) == 1 && same_mixed(q, p, b),
		      "Unown gen 3 = pack 201.bin small + gen3 letter large");
	}
	snprintf(q, sizeof q, WBX_SPRITE3_DIR "/201.bin"); // a form out of range: Unown A
	CHECK(wbx_sprite_load_for(201, 28, 3, b) == 1 && same_mixed(q, p, b), "Unown gen 3 form 28 = A");
	snprintf(p, sizeof p, WBX_SPRITE3_DIR "/386.bin");
	snprintf(q, sizeof q, WBX_SPRITE_DIR "/386.bin");
	CHECK(wbx_sprite_load_for(386, 1, 3, b) == 1 && same_mixed(p, q, b), "Deoxys gen 3 form 1 = gen3/386.bin (Normal)");
	snprintf(p, sizeof p, WBX_SPRITE_DIR "/387.bin");
	CHECK(wbx_sprite_load_for(387, 0, 3, b) == 1 && same_file(p, b), "#387 gen 3 = pack");
	int n3 = 0;
	for (int d = 1; d <= 386; d++) {
		snprintf(p, sizeof p, WBX_SPRITE3_DIR "/%03d.bin", d);
		snprintf(q, sizeof q, WBX_SPRITE_DIR "/%03d.bin", d);
		n3 += wbx_sprite_load_for(d, 0, 3, b) == 1 && same_mixed(p, q, b);
	}
	CHECK(n3 == 386, "all 386 Gen 3 files used");
	printf("gen3 files used for #1-386: %d\n", n3);
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
	printf("gen12 files used for #1-251: %d\n", n12);

	// v0.90: the forms folder for Gen 5-7 games (was Gen 7 only), form 0 overrides, the female picture
	static const struct { int dex, form, gen, female; const char *file; } fx[] = {
		{ 666, 0, 6, 0, "666_f0" }, { 666, 0, 7, 0, "666_f0" }, { 666, 6, 6, 0, NULL }, { 666, 6, 7, 0, NULL },
		{ 666, 19, 6, 0, "666_f19" }, { 666, 19, 7, 0, "666_f19" },
		{ 716, 0, 6, 0, "716_f0" }, { 716, 0, 7, 0, "716_f0" }, { 716, 1, 6, 0, NULL }, { 716, 1, 7, 0, NULL },
		{ 774, 3, 7, 0, NULL }, { 774, 9, 7, 0, "774_f9" },
		{ 718, 0, 6, 0, NULL }, { 718, 1, 6, 0, "718_f1" }, { 718, 2, 6, 0, "718_f2" }, { 718, 1, 7, 0, "718_f1" },
		{ 718, 2, 7, 0, "718_f2" }, { 718, 3, 7, 0, NULL },
		{ 773, 17, 7, 0, "773_f17" }, { 773, 0, 7, 0, NULL },
		{ 678, 1, 6, 1, "678_f1" }, { 678, 1, 7, 1, "678_f1" }, { 678, 0, 6, 0, NULL },
		{ 668, 0, 6, 1, "668_female" }, { 668, 0, 7, 1, "668_female" }, { 668, 0, 6, 0, NULL }, { 668, 0, 7, 0, NULL },
		{ 668, 0, 5, 1, NULL }, // no Pyroar in Gen 5; females only from Gen 6+ games
		{ 586, 2, 5, 0, "586_f2" }, { 649, 4, 5, 0, "649_f4" }, { 646, 2, 5, 0, "646_f2" }, { 586, 2, 6, 0, "586_f2" },
		{ 649, 4, 4, 0, NULL }, { 386, 1, 4, 0, NULL }, // Gen 4 games: never the forms folder
		{ 19, 1, 7, 0, "019_f1" }, { 105, 1, 7, 0, "105_f1" }, // Alolan forms in Gen 7 as before
		{ 25, 3, 6, 0, NULL }, { 25, 3, 7, 0, NULL }, // no file -> pack
	};
	for (size_t i = 0; i < sizeof fx / sizeof fx[0]; i++) {
		if (fx[i].file)
			snprintf(p, sizeof p, WBX_SPRITE_FORMS_DIR "/%s.bin", fx[i].file);
		else
			snprintf(p, sizeof p, WBX_SPRITE_DIR "/%03d.bin", fx[i].dex);
		int ok = wbx_sprite_load_for_g(fx[i].dex, fx[i].form, fx[i].gen, fx[i].female, b) == 1 && same_file(p, b);
		if (!ok)
			printf("  #%d form %d gen %d female %d: expected %s\n", fx[i].dex, fx[i].form, fx[i].gen, fx[i].female, p);
		CHECK(ok, "forms/female case");
	}
	// every file in the forms folder loads byte-identical for its dex/form in Gen 5, 6 and 7 games, never in Gen 4
	DIR *dir = opendir(WBX_SPRITE_FORMS_DIR);
	CHECK(dir != NULL, "forms folder opens");
	int nforms = 0, nfemale = 0;
	struct dirent *e;
	while (dir && (e = readdir(dir)) != NULL) {
		int dex, form;
		char tail[16];
		if (sscanf(e->d_name, "%3d_female%15s", &dex, tail) == 2 && !strcmp(tail, ".bin")) {
			snprintf(p, sizeof p, WBX_SPRITE_FORMS_DIR "/%s", e->d_name);
			for (int g = 6; g <= 7; g++)
				CHECK(wbx_sprite_load_for_g(dex, 0, g, 1, b) == 1 && same_file(p, b), "female file loads (gen 6/7)");
			nfemale++;
		} else if (sscanf(e->d_name, "%3d_f%d%15s", &dex, &form, tail) == 3 && !strcmp(tail, ".bin")) {
			snprintf(p, sizeof p, WBX_SPRITE_FORMS_DIR "/%s", e->d_name);
			for (int g = 5; g <= 7; g++) {
				int ok = wbx_sprite_load_for(dex, form, g, b) == 1 && same_file(p, b);
				if (!ok)
					printf("  %s not used in gen %d\n", e->d_name, g);
				CHECK(ok, "form file loads (gen 5-7)");
			}
			snprintf(q, sizeof q, WBX_SPRITE_DIR "/%03d.bin", dex);
			// (the pack's own picture; 649_f1 Genesect Douse is byte-identical to the pack's 649.bin)
			CHECK(wbx_sprite_load_for(dex, form, 4, b) == 1 && same_file(q, b), "form file never used in gen 4");
			nforms++;
		}
	}
	if (dir)
		closedir(dir);
	CHECK(nforms == 108 && nfemale == 1, "108 form files + 1 female file in the forms folder");
	printf("forms folder: %d form files, %d female file\n", nforms, nfemale);
	// gen 4 games: every dex and form 0-31 = the pack (#387+; #1-386 checked for the pack too: no gen3 in gen 4)
	int n4 = 0;
	for (int d = 1; d <= WBX_SPRITE_LAST; d++) {
		snprintf(q, sizeof q, WBX_SPRITE_DIR "/%03d.bin", d);
		for (int f = 0; f < 32; f++)
			n4 += wbx_sprite_load_for_g(d, f, 4, f & 1, b) == 1 && same_file(q, b);
	}
	CHECK(n4 == WBX_SPRITE_LAST * 32, "gen 4: always the pack");
	// the female flag changes nothing but Pyroar in Gen 6/7 games (every dex, every gen, form 0)
	static unsigned char m0[WBX_SPRITE_SIZE];
	int nfd = 0;
	for (int g = 1; g <= 7; g++)
		for (int d = 1; d <= WBX_SPRITE_LAST; d++) {
			wbx_sprite_load_for_g(d, 0, g, 0, m0);
			wbx_sprite_load_for_g(d, 0, g, 1, b);
			nfd += memcmp(m0, b, sizeof m0) != 0;
			CHECK(!memcmp(m0, b, sizeof m0) || (d == 668 && g >= 6), "female differs only for Pyroar gen 6/7");
		}
	CHECK(nfd == 2, "Pyroar female differs in gen 6 and 7");
	printf("gen 4 pack checks: %d; female differs: %d; %s\n", n4, nfd,
	       fails ? "SPRITE GEN TESTS FAILED" : "sprite gen tests passed");
	return fails != 0;
}
