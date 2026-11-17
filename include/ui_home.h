// ui_home.h - Home screen and list screen (v0.32). Data comes from main.c; these only draw and read keys.
#ifndef WBX_UI_HOME_H
#define WBX_UI_HOME_H

#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { UI_SLOT_EMPTY, UI_SLOT_WALKING, UI_SLOT_WAITING };
enum { UI_HOME_MAIN, UI_HOME_TOOLS, UI_HOME_GAMESET, UI_HOME_SETTINGS, UI_HOME_GUARD, UI_HOME_EXIT };

// "Games on this 3DS" (v0.43): the Pokemon games whose saves were found, HOME icon + name. Kept by main.c between
// Home rebuilds and scanned again only after something may have changed the saves.
#define UI_HOME_GAMES 4
typedef struct ui_home_games {
	int scanned;          // 0 = not looked yet
	int n, total;         // shown (<= UI_HOME_GAMES), found
	char name[UI_HOME_GAMES][64];
	ui_title icon[UI_HOME_GAMES];
} ui_home_games;

typedef struct ui_home_data {
	char trainer[48];    // header right: "Trainer Sam · ID 48213"
	int slot;            // UI_SLOT_*
	char headline[64];   // "Pudding is on a walk"
	char route[32], watts[16], training[48], since[16]; // walking
	char line1[64], line2[64];                          // empty / waiting
	char goes_back[96];  // "Goes back to Pokémon X (PLAYER) at walk end" ("" = no line)
	ui_title game;       // origin game (icon)
	ui_image big[2], small;
	int animate;         // the big sprite's two frames differ
	char main_title[32], main_sub[64];
	// The walker slot's Pokemon for the main button's colour box icon (ui_icons.h); species 0 = not known -> the
	// "?" placeholder is shown (never the walker sprite, user). form/gen = as in its save, female = 1 for a female.
	int species, form, female, gen;
	const ui_home_games *games; // NULL = not known
} ui_home_data;

// Runs Home until a choice; sel = selected button (0 main, 1-3 tiles), kept between calls.
int ui_home_run(const ui_home_data *d, int *sel);

// A list of choices (Walker tools, Settings). Returns the index chosen with A, or -1 for B.
int ui_list_run(const char *page, const char *title, const char *const *labels, const char *const *help, int n,
                int *sel);

#ifdef __cplusplus
}
#endif

#endif
