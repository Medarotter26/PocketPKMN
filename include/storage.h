// WalkerBox storage: Pokemon from the games on this 3DS (uses PKSM-Core, GPL-3.0).
#pragma once

#include <3ds.h>

#include "walk_compose.h"

#ifdef __cplusplus
extern "C" {
#endif

// Milestone S2: find Gen 1/2 Virtual Console saves on the SD card, show their
// boxes and party, and move Pokemon between a game's boxes and the WalkerBox
// bank (sdmc:/3ds/walkerbox/bank.wbk). The game save is backed up before the
// first write of a session.
void storage_browse(PrintConsole *top, PrintConsole *bottom);

// --- one-slot flow (user 2026-11-15: storage holds only the Pokemon on a walk) -----------------------
// The walker slot = WalkerBox bank box 1 slot 1 (box 0, slot 0 here).
#define STORAGE_WALK_BOX  0
#define STORAGE_WALK_SLOT 0

// Choose a game (a game with no routes shows "Not yet compatible") and a Pokemon in its boxes; it moves into
// the walker slot with the same safety steps as Storage (game backup first, safety check, bank written and read
// back before the game). Returns 1 when the walker slot holds the Pokemon to send, 0 if cancelled/refused.
int storage_pick_for_walk(PrintConsole *top, PrintConsole *bottom);

// After a walk: move the Pokemon in the walker slot back into the game it came from (its origin record), first
// free box slot from box 1. Returns 1 = back in its game, 0 = it waits in the slot (game not on this 3DS, every box
// full, no origin record), -1 = error; msg says which.
int storage_return_to_game(char *msg, int msg_size);

// --- walks (Send to walker) ----------------------------------------------------------
// A Pokemon on a walk never leaves the bank: its entry gets the "walking" mark (entry byte +1, bit 0; 0 in
// every older bank file), which blocks withdrawing it until the walk ends.

typedef struct storage_walk_item {
	int box, slot, gen, walking;
	char label[40]; // "Species Lv N" (console font)
} storage_walk_item;

// Bank Pokemon that are not eggs, in bank order. Returns the count, or -1 if the Pokemon data (romfs) or
// the bank cannot be loaded.
int storage_walk_list(storage_walk_item *items, int max);

// Walk data for one bank Pokemon (summary, nickname, name image, sprite). Returns 0, or -1 with why.
int storage_walk_prepare(int box, int slot, struct wbx_walk_poke *out, int *sprite_from_pack, char *why,
                         int why_size);

// Origin record of a bank Pokemon (written at deposit since v0.28): the trainer of the save it came from.
// Returns 1 and fills the fields, 0 if the entry has no record (deposited before v0.28).
int storage_walk_origin(int box, int slot, char *trainer_name, int name_size, unsigned *tid, unsigned *sid,
                        unsigned long long *title_id);

// W4: the walk is over - apply the gains (EXP, friendship, EVs; see include/walk_poke.hpp) to the Pokemon in
// this slot (only if it is still the one with this fingerprint), clear its walking mark, save the bank and read
// it back. ev_stat: -1 off, 0 HP, 1 Atk, 2 Def, 3 SpA, 4 SpD, 5 Spe. Returns 0 (res filled, *new_fp = the
// changed Pokemon's fingerprint), or -1 with why (nothing changed).
typedef struct storage_walk_result {
	unsigned steps, level_before, level_after, exp_before, exp_after;
	int friendship; // 0 for Gen 1
	unsigned friend_before, friend_after;
	int ev_stat;
	unsigned ev_before, ev_after;
} storage_walk_result;
int storage_walk_return(int box, int slot, unsigned long fingerprint, unsigned steps, int ev_stat,
                        storage_walk_result *res, unsigned long *new_fp, char *why, int why_size);

// The game's EV cap for a bank Pokemon's generation (0 = Gen 1/2: no EV walking).
int storage_walk_ev_cap(int box, int slot);

// FNV-1a of the stored Pokemon's bytes (0 for an empty slot): ties a walk to exactly this Pokemon.
unsigned long storage_walk_fingerprint(int box, int slot);

// Sets (walking = 1) or clears the mark, only if the slot still holds the Pokemon with this fingerprint;
// saves the bank and reads it back. Returns 0, or -1 with why.
int storage_walk_mark(int box, int slot, int walking, unsigned long fingerprint, char *why, int why_size);

// v0.50 (audit S2): a message about the bank for the user (recovered after a cut-off save), once; NULL if none.
const char *storage_take_bank_note(void);
// v0.50: English species name ("" if unknown). Not for IR time (reads romfs).
void storage_species_name(int species, char *out, int out_size);
// v0.50: item name ("" if unknown); gen2 = 1 for Gen 2 item ids (GSC courses), else national ids. Not for IR time.
void storage_item_name(int gen2, int id, char *out, int out_size);
// v0.50 (audit B2): the first bank slot marked walking: 1 with box/slot/fingerprint, 0 if none.
int storage_walking_slot(int *box, int *slot, unsigned long *fingerprint);

// --- v0.37: the one-slot pick as data for the GPU screens (game select, box view) ---------------------------
// Same scan, same checks and same deposit as storage_pick_for_walk (which stays for the old Storage menu):
// storage_pick_commit = pickInGame's A branch (empty / egg refused, readyToMove = save layout supported + Gen 2
// switch + game backup, then depositOne into the walker slot), the game backed up on the first box read of a game.
typedef struct storage_game_info {
	unsigned long long title_id;
	char name[64];    // HOME menu name (PKSM's version name if none)
	char trainer[24]; // the save's trainer
	unsigned tid;     // ID as the game shows it
	int gen;          // 1..7
	int kind;         // 0 Virtual Console (SD), 1 game card, 2 GBA (inject)
	int has_routes;   // 0 -> "Not yet compatible"
	int boxes, per_box;
} storage_game_info;

// Scans the saves (also loads the Pokemon data and the bank). Keeps them until storage_games_release.
// Returns the count (0 = none found) or -1 with why.
int storage_games_scan(storage_game_info *out, int max, char *why, int why_size);
void storage_games_release(void);

typedef struct storage_box_mon {
	int present, species, form, level, gender, egg, shiny; // gender 0 male, 1 female, 2 none
	char nickname[24], species_name[24], nature[16], ability[24];
	unsigned friendship, exp_into_level, exp_level_span, exp_to_next; // span 0 at Lv 100
	int has_evs, evs[6]; // Gen 3+
	char ot[24];
	unsigned ot_tid;
} storage_box_mon;

// Reads one box of a scanned game (read only). out gets per_box entries. Returns the box count, or -1.
int storage_game_box(int game, int box, storage_box_mon *out, int max, char *box_name, int name_size);

// Moves that Pokemon into the walker slot. Returns 1 when it is there, 0 refused / failed (msg says why).
int storage_pick_commit(int game, int box, int slot, char *msg, int msg_size);

#ifdef __cplusplus
}
#endif
