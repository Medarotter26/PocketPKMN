// walk_compose.c - walk-start upload builder (see include/walk_compose.h). Portable C99.
#include "walk_compose.h"

#include <string.h>

#include "pw_image.h"
#include "pw_proto.h"
#include "wbx_sprites.h"

// RouteInfo field offsets (docs/RESEARCH.md section 3)
#define RI_SUMMARY     0x00
#define RI_NICKNAME    0x10
#define RI_FRIENDSHIP  0x26
#define RI_IMAGE_IDX   0x27
#define RI_ROUTE_NAME  0x28
#define RI_ROUTE_POKE  0x52
#define RI_POKE_STEPS  0x82
#define RI_POKE_PCT    0x88
#define RI_ITEMS       0x8C
#define RI_ITEM_STEPS  0xA0
#define RI_ITEM_PCT    0xB4

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static void put_text(uint8_t *p, const uint16_t *t, int n)
{
	for (int i = 0; i < n; i++)
		put16(p + 2 * i, t[i]);
}

static void get_text(const uint8_t *p, uint16_t *t, int n)
{
	for (int i = 0; i < n; i++)
		t[i] = get16(p + 2 * i);
}

void wbx_walk_compose(const uint8_t *base, const wbx_walk_poke *poke, const wbx_route *route,
                      uint8_t out[WBX_WALK_SIZE])
{
	if (base)
		memcpy(out, base, WBX_WALK_SIZE);
	else
		memset(out, 0, WBX_WALK_SIZE);
	uint8_t *ri = out + WBX_WO_ROUTE_INFO;
	memcpy(ri + RI_SUMMARY, poke->summary, 16);
	put_text(ri + RI_NICKNAME, poke->nickname, 11);
	ri[RI_FRIENDSHIP] = poke->friendship;
	ri[RI_IMAGE_IDX] = route->image_index;
	put_text(ri + RI_ROUTE_NAME, route->name, 21);
	for (int i = 0; i < 3; i++) {
		memcpy(ri + RI_ROUTE_POKE + 16 * i, route->poke[i].summary, 16);
		put16(ri + RI_POKE_STEPS + 2 * i, route->poke[i].min_steps);
		ri[RI_POKE_PCT + i] = route->poke[i].percent;
		memcpy(out + WBX_WO_ROUTE_SMALL + i * WBX_SMALL_SIZE, route->poke[i].small, WBX_SMALL_SIZE);
		memcpy(out + WBX_WO_ROUTE_NAMES + i * WBX_NAME_IMG_SIZE, route->poke[i].name_img, WBX_NAME_IMG_SIZE);
	}
	for (int i = 0; i < 10; i++) {
		put16(ri + RI_ITEMS + 2 * i, route->item[i]);
		put16(ri + RI_ITEM_STEPS + 2 * i, route->item_steps[i]);
		ri[RI_ITEM_PCT + i] = route->item_percent[i];
		memcpy(out + WBX_WO_ITEM_NAMES + i * WBX_ITEM_IMG_SIZE, route->item_img[i], WBX_ITEM_IMG_SIZE);
	}
	memcpy(out + WBX_WO_ROUTE_IMAGE, route->image, sizeof route->image);
	memcpy(out + WBX_WO_ROUTE_NAME, route->name_img, WBX_NAME_IMG_SIZE);
	memcpy(out + WBX_WO_SPRITE, poke->sprite, sizeof poke->sprite);
	memcpy(out + WBX_WO_POKE_NAME, poke->name_img, WBX_NAME_IMG_SIZE);
	memcpy(out + WBX_WO_ROUTE_LARGE, route->large_c, WBX_LARGE_SIZE);
}

void wbx_walk_extract(const uint8_t walk[WBX_WALK_SIZE], wbx_walk_poke *poke, wbx_route *route)
{
	const uint8_t *ri = walk + WBX_WO_ROUTE_INFO;
	memcpy(poke->summary, ri + RI_SUMMARY, 16);
	get_text(ri + RI_NICKNAME, poke->nickname, 11);
	poke->display_name[0] = 0; // v0.82: only the stored text is on the walker
	poke->friendship = ri[RI_FRIENDSHIP];
	route->image_index = ri[RI_IMAGE_IDX];
	get_text(ri + RI_ROUTE_NAME, route->name, 21);
	for (int i = 0; i < 3; i++) {
		memcpy(route->poke[i].summary, ri + RI_ROUTE_POKE + 16 * i, 16);
		route->poke[i].min_steps = get16(ri + RI_POKE_STEPS + 2 * i);
		route->poke[i].percent = ri[RI_POKE_PCT + i];
		memcpy(route->poke[i].small, walk + WBX_WO_ROUTE_SMALL + i * WBX_SMALL_SIZE, WBX_SMALL_SIZE);
		memcpy(route->poke[i].name_img, walk + WBX_WO_ROUTE_NAMES + i * WBX_NAME_IMG_SIZE, WBX_NAME_IMG_SIZE);
	}
	for (int i = 0; i < 10; i++) {
		route->item[i] = get16(ri + RI_ITEMS + 2 * i);
		route->item_steps[i] = get16(ri + RI_ITEM_STEPS + 2 * i);
		route->item_percent[i] = ri[RI_ITEM_PCT + i];
		memcpy(route->item_img[i], walk + WBX_WO_ITEM_NAMES + i * WBX_ITEM_IMG_SIZE, WBX_ITEM_IMG_SIZE);
	}
	memcpy(route->image, walk + WBX_WO_ROUTE_IMAGE, sizeof route->image);
	memcpy(route->name_img, walk + WBX_WO_ROUTE_NAME, WBX_NAME_IMG_SIZE);
	memcpy(poke->sprite, walk + WBX_WO_SPRITE, sizeof poke->sprite);
	memcpy(poke->name_img, walk + WBX_WO_POKE_NAME, WBX_NAME_IMG_SIZE);
	memcpy(route->large_c, walk + WBX_WO_ROUTE_LARGE, WBX_LARGE_SIZE);
}

void wbx_summary_pack(uint8_t out[16], uint16_t species, uint16_t item, const uint16_t moves[4], uint8_t level,
                      uint8_t variant, uint8_t flags)
{
	memset(out, 0, 16);
	put16(out + 0, species);
	put16(out + 2, item);
	for (int i = 0; i < 4; i++)
		put16(out + 4 + 2 * i, moves[i]);
	out[12] = level;
	out[13] = variant;
	out[14] = flags;
}

int wbx_text_pack(const char *utf8, uint16_t *out, int slots, int last_ffff)
{
	uint16_t tmp[32];
	int max = slots - 1 < 32 ? slots - 1 : 32; // room for the 0xFFFF end mark
	int n = (int)pw_text_from_ascii(utf8, tmp, (size_t)max);
	for (int i = 0; i < slots; i++)
		out[i] = i < n ? tmp[i] : i == n ? 0xFFFF : 0;
	if (last_ffff)
		out[slots - 1] = 0xFFFF;
	return n;
}

int wbx_label_image(const char *utf8, int w, int h, uint8_t *out)
{
	uint8_t sh[96 * 16], res[96 * 16];
	if (w * h > (int)sizeof sh)
		return -1;
	int fit = pw_text_image(utf8, w, h, out);
	pw_img_decode(out, w, h, sh);
	memcpy(res, sh, (size_t)(w * h));
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			if (sh[y * w + x] != PW_SHADE_BLACK)
				continue;
			static const int d[3][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 } };
			for (int k = 0; k < 3; k++) {
				int nx = x + d[k][0], ny = y + d[k][1];
				if (nx < w && ny < h && res[ny * w + nx] == PW_SHADE_WHITE)
					res[ny * w + nx] = PW_SHADE_LIGHT;
			}
		}
	pw_img_encode(res, w, h, out);
	return fit;
}

// --- the WalkerBox test route -----------------------------------------------------------------------

// Own drawing (WalkerBox art): a tree, a signpost, grass and a path. '.' white, '-' light grey,
// '+' dark grey, '#' black.
static const char *const k_test_route_image[24] = {
	"................................",
	".........................---....",
	"........................-...-...",
	"........................-...-...",
	".....###.................---....",
	"...##+++##......................",
	"..#+++++++#.....................",
	"..#++-++++#.....................",
	".#+++++-+++#....................",
	".#++-++++++#........######......",
	"..#+++++-+#.........#----#......",
	"...##+++##..........#-##-#......",
	".....#+#............######......",
	".....#+#...............#........",
	".....#+#...............#........",
	".....#+#....-.-........#....-.-.",
	"..-.-#+#-.-..-....-.-..#-.-..-..",
	"################################",
	"--------------------------------",
	"-+------------+-------+-------+-",
	"--------------------------------",
	"-------+-----------+----------+-",
	"--------------------------------",
	"--------------------------------",
};

typedef struct {
	uint16_t species;
	uint8_t level;
	uint16_t moves[4];
	uint16_t min_steps;
	uint8_t percent;
	const char *name;
} test_poke;

// Steps / percent follow the structure of the user's walker's own route (3000/500/0, 50/75/100).
static const test_poke k_test_pokes[3] = {
	{ 506, 8, { 43, 33, 316, 0 }, 3000, 50, "Lillipup" }, // Leer, Tackle, Odor Sleuth
	{ 519, 5, { 16, 45, 0, 0 }, 500, 75, "Pidove" },      // Gust, Growl
	{ 504, 5, { 33, 43, 0, 0 }, 0, 100, "Patrat" },       // Tackle, Leer
};

static const struct {
	uint16_t item, steps;
	uint8_t percent;
	const char *name;
} k_test_items[10] = {
	{ 50, 2500, 20, "Rare Candy" }, { 38, 2000, 20, "Ether" },       { 3, 1000, 30, "Great Ball" },
	{ 78, 900, 30, "Escape Rope" }, { 26, 800, 30, "Super Potion" }, { 79, 700, 40, "Repel" },
	{ 155, 600, 50, "Oran Berry" }, { 18, 500, 50, "Antidote" },     { 4, 300, 50, "Pok\xc3\xa9 Ball" },
	{ 17, 0, 100, "Potion" },
};

int wbx_route_test(wbx_route *r)
{
	memset(r, 0, sizeof *r);
	r->image_index = 0; // field
	wbx_text_pack("Test Path", r->name, 21, 0);
	wbx_label_image("Test Path", 80, 16, r->name_img);
	uint8_t sh[32 * 24];
	for (int y = 0; y < 24; y++)
		for (int x = 0; x < 32; x++) {
			char c = k_test_route_image[y][x];
			sh[y * 32 + x] = c == '#' ? 3 : c == '+' ? 2 : c == '-' ? 1 : 0;
		}
	pw_img_encode(sh, 32, 24, r->image);
	int from_pack = 0;
	for (int i = 0; i < 3; i++) {
		const test_poke *t = &k_test_pokes[i];
		static uint8_t spr[WBX_SPRITE_SIZE];
		from_pack += wbx_sprite_load(t->species, spr);
		wbx_summary_pack(r->poke[i].summary, t->species, 0, t->moves, t->level, 0, 0);
		r->poke[i].min_steps = t->min_steps;
		r->poke[i].percent = t->percent;
		memcpy(r->poke[i].small, spr, WBX_SMALL_SIZE);
		wbx_label_image(t->name, 80, 16, r->poke[i].name_img);
		if (i == 2)
			memcpy(r->large_c, spr + WBX_SPRITE_SMALL_SIZE, WBX_LARGE_SIZE);
	}
	for (int i = 0; i < 10; i++) {
		r->item[i] = k_test_items[i].item;
		r->item_steps[i] = k_test_items[i].steps;
		r->item_percent[i] = k_test_items[i].percent;
		wbx_label_image(k_test_items[i].name, 96, 16, r->item_img[i]);
	}
	return from_pack;
}
