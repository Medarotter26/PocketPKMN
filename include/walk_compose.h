// walk_compose.h - builds the walk-start upload (EEPROM 0x8F00-0xB7FF, staged at 0xD700) from a walking
// Pokemon and a route. Portable C99, used by the app and the PC tests. Layout: docs/RESEARCH.md section 3.
#ifndef WALK_COMPOSE_H
#define WALK_COMPOSE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WBX_WALK_SIZE 0x2900 // 0x8F00-0xB7FF

// Offsets inside the walk blob (add 0x8F00 for the EEPROM address, 0xD700 for the staging address).
#define WBX_WO_ROUTE_INFO   0x0000 // 0xBE
#define WBX_WO_ROUTE_IMAGE  0x00BE // 32x24, 0xC0
#define WBX_WO_ROUTE_NAME   0x017E // 80x16, 0x140
#define WBX_WO_SPRITE       0x02BE // small 32x24 x2 (0x180) + large 64x48 x2 (0x600)
#define WBX_WO_POKE_NAME    0x0A3E // 80x16, 0x140
#define WBX_WO_ROUTE_SMALL  0x0B7E // 3 x 0x180
#define WBX_WO_ROUTE_LARGE  0x0FFE // large sprite of route Pokemon C (index 2), 0x600
#define WBX_WO_ROUTE_NAMES  0x15FE // 3 x 0x140
#define WBX_WO_ITEM_NAMES   0x19BE // 10 x 96x16 (0x180)
#define WBX_WO_TAIL         0x28BE // 0x42 bytes, meaning unknown: kept from the base walk (zeros on the user's)

#define WBX_NAME_IMG_SIZE 0x140 // 80x16
#define WBX_ITEM_IMG_SIZE 0x180 // 96x16
#define WBX_SMALL_SIZE    0x180
#define WBX_LARGE_SIZE    0x600

typedef struct wbx_walk_poke {
	uint8_t summary[16];   // PokemonSummary: species, item, moves[4], level, variant, flags, pad
	uint16_t nickname[11]; // Gen 4 text, packed as the game does (see wbx_text_pack)
	uint8_t friendship;
	uint8_t sprite[WBX_SMALL_SIZE + WBX_LARGE_SIZE];
	uint8_t name_img[WBX_NAME_IMG_SIZE];
} wbx_walk_poke;

typedef struct wbx_route_poke {
	uint8_t summary[16];
	uint16_t min_steps;
	uint8_t percent;
	uint8_t small[WBX_SMALL_SIZE];
	uint8_t name_img[WBX_NAME_IMG_SIZE];
} wbx_route_poke;

typedef struct wbx_route {
	uint8_t image_index; // 0 field ... 7 beach; only stored (event log)
	uint16_t name[21];   // Gen 4 text
	uint8_t image[0xC0];
	uint8_t name_img[WBX_NAME_IMG_SIZE];
	wbx_route_poke poke[3];
	uint8_t large_c[WBX_LARGE_SIZE]; // large sprite of poke[2]
	uint16_t item[10];
	uint16_t item_steps[10];
	uint8_t item_percent[10];
	uint8_t item_img[10][WBX_ITEM_IMG_SIZE];
} wbx_route;

// Builds the 0x2900-byte walk. base = an existing walk (e.g. from a backup) for the bytes no field covers
// (RouteInfo +0x8B, the 0x42-byte tail); NULL = zeros.
void wbx_walk_compose(const uint8_t *base, const wbx_walk_poke *poke, const wbx_route *route,
                      uint8_t out[WBX_WALK_SIZE]);

// The reverse: splits a walk (e.g. 0x8F00 of a backup) into its parts. compose(walk, extract(walk)) == walk.
void wbx_walk_extract(const uint8_t walk[WBX_WALK_SIZE], wbx_walk_poke *poke, wbx_route *route);

// PokemonSummary bytes.
void wbx_summary_pack(uint8_t out[16], uint16_t species, uint16_t item, const uint16_t moves[4], uint8_t level,
                      uint8_t variant, uint8_t flags);
#define WBX_VARIANT_FEMALE 0x20 // variant = form (low 5 bits) | 0x20 female
#define WBX_FLAG_HAS_FORM  0x01
#define WBX_FLAG_SHINY     0x02

// ASCII/UTF-8 -> Gen 4 text the way HGSS stores it: characters, 0xFFFF, zeros; with last_ffff the final slot
// is 0xFFFF too (nicknames). Characters without a Gen 4 code here are left out. Returns the characters kept.
int wbx_text_pack(const char *utf8, uint16_t *out, int slots, int last_ffff);

// Label image like the walker's own name images: black text (WalkerBox font), light grey shadow to the right
// and below. Returns 0 if it fit, -1 if cut.
int wbx_label_image(const char *utf8, int w, int h, uint8_t *out);

// The WalkerBox test route ("Test Path"): own route picture, Lillipup / Pidove / Patrat, ten common items.
// Sprites come from the sprite pack (placeholder if a file is missing). Returns how many of the 3 route
// sprites came from the pack.
int wbx_route_test(wbx_route *r);

#ifdef __cplusplus
}
#endif

#endif
