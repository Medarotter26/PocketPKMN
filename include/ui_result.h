// ui_result.h - the End walk result screen (v0.41). main.c fills it while the walk ends (the text view keeps the same
// lines for the debug view); shown after the link is closed.
#ifndef WBX_UI_RESULT_H
#define WBX_UI_RESULT_H

#include <3ds.h>

#include "walk_compose.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_result_info {
	int ok;                 // the walk ended (else the text view is shown as before)
	char nick[48]; // v0.82: the display name (UTF-8, up to 12 characters) - was 16
	u8 sprite[WBX_SMALL_SIZE + WBX_LARGE_SIZE]; // the walking Pokemon's walker sprite (from the walk data)
	unsigned watts, steps;
	int gains;              // 1 = the fields below are set (it went back into PocketPKMN's slot)
	unsigned level_before, level_after, exp_gain, friend_before, friend_after;
	int has_friendship, ev_stat;
	unsigned ev_before, ev_after;
	char back[160];         // "Back in Pokemon X (box 1, slot 3)." / waiting message
	int ncaught;
	unsigned caught_species[3], caught_level[3];
	const u8 *caught_name[3];  // 80x16 name pictures from the route (NULL if not on the route)
	const u8 *caught_small[3]; // 32x24 x2 small sprites from the route
	char caught_text[3][24];   // v0.50: species names as text (filled after the link, "" = unknown)
	char caught_msg[400];      // v0.53: where the catches went (one line each), "" = not delivered
	char sent_line[2][96];     // v0.53: "The Pokemon it caught are in its game's boxes." / "The items ... in the PC."
	int nitems;
	unsigned item[13];
	const u8 *item_name[13];   // 96x16 name pictures from the route (NULL if not on it)
	char item_text[13][24];    // v0.50: item names as text (filled after the link, "" = unknown)
} ui_result_info;

void ui_result_run(const ui_result_info *r);

#ifdef __cplusplus
}
#endif

#endif
