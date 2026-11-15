// test_routes.c - loads romfs/routes/gsc.txt and xy.txt with routes.c and builds every course's walker route for
// every version: 6 Pokemon / 10 items per course, a walker route from it (pictures, names, sprites), and the
// built walk round-trips through wbx_walk_compose / wbx_walk_extract.
#include <stdio.h>
#include <string.h>

#include "pw_image.h"
#include "routes.h"
#include "wbx_sprites.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static int count_shade(const uint8_t *img, int bytes)
{
	int n = 0;
	for (int i = 0; i < bytes; i++)
		n += img[i] != 0;
	return n;
}

static void one(const char *file, const char *ver, int gen)
{
	static route_file rf;
	int n = routes_load(file, &rf);
	CHECK(n > 0, "%s: loaded %d courses", file, n);
	int built = 0;
	for (int i = 0; i < rf.ncourse; i++) {
		const route_course *c = &rf.course[i];
		CHECK(c->npoke == 6, "%s %s: %d pokes", file, c->name, c->npoke);
		CHECK(c->nitem >= 10, "%s %s: %d items", file, c->name, c->nitem);
		for (unsigned seed = 0; seed < 8; seed++) {
			static wbx_route r;
			CHECK(routes_build(c, ver, gen, seed, &r) == 0, "%s %s (%s): build", file, c->name, ver);
			CHECK(count_shade(r.image, sizeof r.image) > 8, "%s %s: picture missing", file, c->name);
			for (int s = 0; s < 3; s++) {
				CHECK(r.poke[s].percent == (s == 0 ? 50 : s == 1 ? 75 : 100), "%s %s: slot %d percent", file, c->name, s);
				CHECK(count_shade(r.poke[s].name_img, sizeof r.poke[s].name_img) > 4, "%s %s: name image %d", file, c->name, s);
			}
			for (int k = 0; k < 10; k++)
				CHECK(r.item[k] != 0 && count_shade(r.item_img[k], sizeof r.item_img[k]) > 4, "%s %s: item %d", file, c->name, k);
			static uint8_t blob[WBX_WALK_SIZE], blob2[WBX_WALK_SIZE];
			static wbx_walk_poke wp, wp2;
			static wbx_route r2;
			memset(&wp, 0, sizeof wp);
			wbx_walk_compose(NULL, &wp, &r, blob);
			wbx_walk_extract(blob, &wp2, &r2);
			wbx_walk_compose(NULL, &wp2, &r2, blob2);
			CHECK(!memcmp(blob, blob2, sizeof blob), "%s %s: compose/extract round trip", file, c->name);
			built++;
		}
		// name widths (the walker shows 80 px names, 96 px items)
		for (int k = 0; k < c->npoke; k++)
			CHECK(pw_text_width(c->poke[k].name) + 2 <= 80, "%s: %s too wide", file, c->poke[k].name);
		CHECK(pw_text_width(c->name) + 2 <= 80, "%s: course %s too wide", file, c->name);
	}
	printf("%s (%s): %d courses, %d routes built\n", file, ver, rf.ncourse, built);
}

int main(void)
{
	one("gsc", "all", 2);
	one("xy", "x", 6);
	one("xy", "y", 6);
	const char *v;
	CHECK(routes_file_for(0x0004000000055D00ULL, 6, &v) && !strcmp(v, "x"), "X -> xy/x");
	CHECK(routes_file_for(0x0004000000055E00ULL, 6, &v) && !strcmp(v, "y"), "Y -> xy/y");
	CHECK(routes_file_for(0x0004000000172800ULL, 2, &v) && !strcmp(routes_file_for(0x0004000000172800ULL, 2, &v), "gsc"), "Crystal -> gsc");
	CHECK(!routes_file_for(0x0004000000171100ULL, 1, &v), "Blue -> no routes");
	CHECK(!routes_file_for(0x000400000011C500ULL, 6, &v), "Alpha Sapphire -> no routes");
	printf(fails ? "ROUTE TESTS: %d FAILED\n" : "route tests passed (%d failures)\n", fails);
	return fails != 0;
}
