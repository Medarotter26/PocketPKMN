// test_walk.c - PC checks of source/walk_compose.c.
// Usage: test_walk <walker backup>...   (sprite pack: built with -DWBX_SPRITE_PC_DIR)
//   1. every backup holding a walk: extract + compose gives the walk back byte for byte
//   2. text packing in the game's form (checked against the Sentret walk in the user's backups)
//   3. the test route: fields land where the walker reads them; sprites from the pack
// Writes /tmp/walkerbox_test_walk.bin (Sentret + test route) for tools/walk_preview.py.
#include <stdio.h>
#include <string.h>

#include "pw_image.h"
#include "pw_proto.h"
#include "walk_compose.h"
#include "wbx_sprites.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint8_t ee[0x10000], walk[WBX_WALK_SIZE], again[WBX_WALK_SIZE];
static wbx_walk_poke poke;
static wbx_route route;

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

int main(int argc, char **argv)
{
	int walks = 0;
	for (int a = 1; a < argc; a++) {
		FILE *f = fopen(argv[a], "rb");
		size_t n = f ? fread(ee, 1, sizeof ee, f) : 0;
		if (f)
			fclose(f);
		CHECK(n == sizeof ee, "read %s", argv[a]);
		memcpy(walk, ee + 0x8F00, WBX_WALK_SIZE);
		if (rd16(walk) == 0) // walk cleared by a walk end
			continue;
		walks++;
		wbx_walk_extract(walk, &poke, &route);
		wbx_walk_compose(walk, &poke, &route, again);
		CHECK(memcmp(walk, again, WBX_WALK_SIZE) == 0, "%s: round trip differs", argv[a]);
		wbx_walk_compose(NULL, &poke, &route, again);
		int diff = 0;
		for (int i = 0; i < WBX_WALK_SIZE; i++)
			diff += walk[i] != again[i];
		CHECK(diff == 0, "%s: %d byte(s) not covered by any field are non-zero", argv[a], diff);
		CHECK(memcmp(poke.sprite, ee + 0x91BE, sizeof poke.sprite) == 0, "sprite offset");
		CHECK(memcmp(route.large_c, ee + 0x9EFE, WBX_LARGE_SIZE) == 0, "route large offset");
		CHECK(memcmp(route.item_img[9], ee + 0xA8BE + 9 * 0x180, 0x180) == 0, "item image offset");
		// the walk as the game packed it: nickname 3 chars, FFFF, zeros, FFFF in the last slot
		uint16_t nick[11];
		char ascii[12];
		memset(ascii, 'a', 3);
		ascii[3] = 0;
		wbx_text_pack(ascii, nick, 11, 1);
		for (int i = 0; i < 11; i++) {
			int game_end = poke.nickname[i] == 0xFFFF, ours_end = nick[i] == 0xFFFF;
			int game_zero = poke.nickname[i] == 0, ours_zero = nick[i] == 0;
			CHECK(game_end == ours_end && game_zero == ours_zero, "nickname slot %d form: game %04x ours %04x", i,
			      poke.nickname[i], nick[i]);
		}
		uint16_t rname[21];
		int len = 0;
		while (len < 21 && route.name[len] != 0xFFFF)
			len++;
		char rascii[32];
		pw_text_to_ascii(route.name, 21, rascii, sizeof rascii);
		wbx_text_pack(rascii, rname, 21, 0);
		for (int i = len + 1; i < 21; i++)
			CHECK(route.name[i] == 0 && rname[i] == 0, "route name slot %d after the end", i);
		CHECK(route.name[len] == 0xFFFF && rname[strlen(rascii)] == 0xFFFF, "route name end mark");
	}
	CHECK(walks > 0, "no backup with a walk given");
	printf("round trip: %d walk(s) from backups rebuilt byte for byte\n", walks);

	// test route on top of the last backup's walking Pokemon
	int from_pack = wbx_route_test(&route);
	CHECK(from_pack == 3, "test route sprites from the pack: %d of 3", from_pack);
	wbx_walk_compose(NULL, &poke, &route, walk);
	const uint8_t *ri = walk;
	const uint16_t want_species[3] = { 506, 519, 504 };
	for (int i = 0; i < 3; i++) {
		pw_poke_summary s;
		pw_poke_summary_parse(ri + 0x52 + 16 * i, &s);
		CHECK(s.species == want_species[i], "route poke %d species %u", i, s.species);
		static uint8_t spr[WBX_SPRITE_SIZE];
		wbx_sprite_load(want_species[i], spr);
		CHECK(memcmp(walk + WBX_WO_ROUTE_SMALL + i * 0x180, spr, 0x180) == 0, "route poke %d small sprite", i);
		if (i == 2)
			CHECK(memcmp(walk + WBX_WO_ROUTE_LARGE, spr + 0x180, 0x600) == 0, "route poke C large sprite");
	}
	CHECK(rd16(ri + 0x82) == 3000 && rd16(ri + 0x84) == 500 && rd16(ri + 0x86) == 0, "route poke steps");
	CHECK(ri[0x88] == 50 && ri[0x89] == 75 && ri[0x8A] == 100, "route poke percent");
	CHECK(rd16(ri + 0x8C) == 50 && rd16(ri + 0x8C + 18) == 17, "items");
	CHECK(rd16(ri + 0xA0) == 2500 && rd16(ri + 0xA0 + 18) == 0 && ri[0xB4] == 20 && ri[0xBD] == 100, "item odds");
	char name[32];
	pw_text_to_ascii((const uint16_t *)(const void *)route.name, 21, name, sizeof name);
	CHECK(strcmp(name, "Test Path") == 0, "route name '%s'", name);
	uint16_t back[21];
	for (int i = 0; i < 21; i++)
		back[i] = rd16(ri + 0x28 + 2 * i);
	pw_text_to_ascii(back, 21, name, sizeof name);
	CHECK(strcmp(name, "Test Path") == 0, "route name in RouteInfo '%s'", name);
	// labels: every image has black text and light grey shadow, nothing else
	uint8_t sh[96 * 16];
	pw_img_decode(route.item_img[8], 96, 16, sh);
	int count[4] = { 0 };
	for (int i = 0; i < 96 * 16; i++)
		count[sh[i] & 3]++;
	CHECK(count[3] > 0 && count[1] > 0 && count[2] == 0, "label shades %d/%d/%d/%d", count[0], count[1], count[2],
	      count[3]);
	CHECK(wbx_label_image("Super Potion", 96, 16, route.item_img[0]) == 0, "longest item name fits 96 px");
	CHECK(wbx_label_image("Escape Rope", 96, 16, route.item_img[0]) == 0, "Escape Rope fits");
	printf("widest 10-char nickname \"MMMMMMMMMM\": %d px of 80 (%s)\n", pw_text_width("MMMMMMMMMM") + 2,
	       wbx_label_image("MMMMMMMMMM", 80, 16, route.item_img[0]) == 0 ? "fits" : "cut");

	FILE *o = fopen("/tmp/walkerbox_test_walk.bin", "wb");
	if (o) {
		fwrite(walk, 1, WBX_WALK_SIZE, o);
		fclose(o);
	}
	printf("%s (%d failure(s))\n", fails ? "FAILED" : "ALL WALK CHECKS PASSED", fails);
	return fails != 0;
}
