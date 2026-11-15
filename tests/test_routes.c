// test_routes.c - loads romfs/routes/gsc.txt, xy.txt and sm.txt with routes.c and builds every course's walker route for
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
	CHECK(rf.rejected == 0, "%s: %d line(s) rejected by the parser (audit M7)", file, rf.rejected);
	// audit M9: the parser's limits - names / descriptions / pictures must fit (a longer one would be cut silently)
	for (int i = 0; i < rf.ncourse; i++) {
		const route_course *c = &rf.course[i];
		CHECK(strlen(c->name) < sizeof(c->name) - 1, "%s: course name at the limit: %s", file, c->name);
		CHECK(strlen(c->desc) < sizeof(c->desc) - 1, "%s %s: description at the limit (cut?)", file, c->name);
		CHECK(strlen(c->picture) < sizeof(c->picture) - 1, "%s %s: picture name at the limit", file, c->name);
		CHECK(c->npoke == 6 && c->nitem <= 20, "%s %s: %d Pokemon / %d items", file, c->name, c->npoke, c->nitem);
		char pic[128];
		snprintf(pic, sizeof(pic), ROUTES_DIR "/pictures/%s.txt", c->picture);
		FILE *pf = fopen(pic, "r");
		CHECK(pf != NULL, "%s %s: picture file %s missing", file, c->name, pic);
		if (pf)
			fclose(pf);
	}
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
	// v0.52: course unlocks
	CHECK(routes_unlocked("start", 0, 0, 0), "start is always open");
	CHECK(!routes_unlocked("badges=3", 2, 0, 0) && routes_unlocked("badges=3", 3, 0, 0) &&
	      routes_unlocked("badges=3", 16, 0, 0), "badges=3 opens at 3");
	CHECK(!routes_unlocked("hof", 8, 0, 99999) && routes_unlocked("hof", 8, 1, 0), "hof needs the Hall of Fame");
	CHECK(!routes_unlocked("hof watts=5000", 16, 0, 99999) && !routes_unlocked("hof watts=5000", 16, 1, 4999) &&
	      routes_unlocked("hof watts=5000", 16, 1, 5000), "hof watts=5000 needs both");
	CHECK(routes_unlocked("something new", 0, 0, 0), "unknown unlock text stays open");
	// v0.55: Sun / Moon / Ultra Sun / Ultra Moon
	one("sm", "sn", 7);
	one("sm", "mn", 7);
	one("sm", "us", 7);
	one("sm", "um", 7);
	CHECK(routes_file_for(0x0004000000175E00ULL, 7, &v) && !strcmp(v, "mn"), "Moon -> sm/mn");
	CHECK(routes_file_for(0x0004000000164800ULL, 7, &v) && !strcmp(v, "sn"), "Sun -> sm/sn");
	CHECK(routes_file_for(0x00040000001B5000ULL, 7, &v) && !strcmp(v, "us"), "Ultra Sun -> sm/us");
	CHECK(routes_file_for(0x00040000001B5100ULL, 7, &v) && !strcmp(v, "um"), "Ultra Moon -> sm/um");
	CHECK(!routes_unlocked("trials=3", 2, 0, 0) && routes_unlocked("trials=3", 3, 0, 0), "trials=3 opens at 3");
	{
		char u[48];
		routes_unlock_text("trials=6", u, sizeof u);
		CHECK(!strcmp(u, "After Olivia's grand trial"), "trials=6 text: %s", u);
		static route_file rf;
		routes_load("sm", &rf);
		int forms = 0;
		for (int i = 0; i < rf.ncourse; i++)
			for (int k = 0; k < rf.course[i].npoke; k++)
				forms += rf.course[i].poke[k].form != 0;
		CHECK(forms == 6, "sm: %d Pokemon with a form (expected 6: 5 Alolan + Blue-Striped Basculin)", forms);
	}
	printf(fails ? "ROUTE TESTS: %d FAILED\n" : "route tests passed (%d failures)\n", fails);
	return fails != 0;
}
