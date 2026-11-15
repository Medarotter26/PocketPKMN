// ui_send.h - the send screen (mock_4_send, v0.41): course + EV stat, before the walk is sent.
#ifndef WBX_UI_SEND_H
#define WBX_UI_SEND_H

#include <3ds.h>

#include "routes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ui_send_info {
	const u8 *sprite;      // the walking Pokemon's walker sprite (small x2 + large x2, wbx_walk_poke.sprite)
	char line1[96];        // "Oshawott  Lv.12 \xC2\xB7 from Pokemon X (Nate)" (debug log only since v0.52)
	char mon[32], game[64], trainer[24]; // v0.52: the top info box's columns ("Oshawott  Lv.12", "Pokemon X", "Nate")
	int ev_allowed;        // 0 for Gen 1/2 (no EVs)
	const route_file *rf;  // the game's courses, or NULL -> "Test Path" only
	int gen;               // the game's generation (route sprites: Gen 1/2 set for gen 2)
	// v0.46: has this Pokemon been caught on this course before (from the ledger)? Unseen ones show the "?" picture.
	int (*seen)(const char *course, int species);
	// v0.52: the game's progress for course unlocks; progress_known = 0 -> nothing is locked
	int progress_known, badges, hof;
	unsigned watts; // watts PocketPKMN has banked so far (for "hof watts=N" courses)
} ui_send_info;

// Returns 1 = send (course: index into rf, or -1 for Test Path; ev: -1 off, 0 HP .. 5 Spe), 0 = back.
int ui_send_run(const ui_send_info *in, int *course, int *ev);

#ifdef __cplusplus
}
#endif

#endif
