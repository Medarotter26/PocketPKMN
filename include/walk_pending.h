// A walk's results that are not fully delivered yet (v0.68, review finding 4 + addendum finding C).
// File APP_DIR/pending_walk.bin, written right after the walk-end link closes - before anything else is done -
// and updated after every single step (watts, gains, each catch, each item, the return to the game), so a stop
// at any moment loses nothing and does nothing twice. Removed when every step is done.
// Plain C with no 3DS calls: tests/test_pending.c runs it on the PC.
#pragma once
#include <stdint.h>

#define WP_MAX_CATCHES 3
#define WP_MAX_ITEMS   13
#define WP_ENTRY_SIZE  0x200 // one PocketPKMN bank entry (the walked Pokemon + its origin record)

enum { // the walk end itself
	WP_ENDED = 1, // the walker answered the end command
	WP_MAYBE = 2, // the end command went out, its answer was lost: confirmed (or dropped) at the next End walk
};

enum { // each step
	WP_TODO = 0,
	WP_STARTED = 1, // marked just before the step ran; found after a stop = it may have happened: never repeated
	WP_DONE = 2,
};

typedef struct wp_catch { // = storage_catch (include/storage.h), fixed-size fields
	int32_t species, level;
	uint32_t moves[4];
	int32_t female, shiny, form;
} wp_catch;

typedef struct wp_record {
	uint32_t magic, version, seq, state;
	uint32_t walker;          // walker_key() of the walker
	uint32_t route;           // fnv1a of the walk's route summary (0xBE bytes)
	uint32_t steps_total;     // the walker's total steps at the end
	int32_t bank_box, bank_slot;
	uint32_t bank_fp;         // the walked Pokemon's fingerprint when it was sent
	uint32_t walk_steps;
	int32_t ev;               // EV stat of the walk, -1 off
	int32_t items_gen2;       // the course's items use Gen 2 numbers
	uint32_t watts;
	uint32_t game_lo, game_hi; // the walked Pokemon's game (title id)
	int32_t one_slot;         // walked from the walker slot: catches / items / the return go to its game
	char nick[16];
	uint8_t entry[WP_ENTRY_SIZE]; // the walked Pokemon's bank entry at the end (catches are made from its record)
	int32_t ncatch, nitem;
	wp_catch catches[WP_MAX_CATCHES];
	uint32_t item_id[WP_MAX_ITEMS];
	int32_t item_gen2[WP_MAX_ITEMS];
	uint8_t st_watts, st_gains, st_return, pad0;
	uint8_t st_catch[WP_MAX_CATCHES];
	uint8_t st_item[WP_MAX_ITEMS];
	uint32_t check; // fnv1a of everything before it
} wp_record;

// Writes rec (seq + 1) as base: base.tmp first, then base -> base.bak, base.tmp -> base, base.bak removed.
// Returns 0, or -1 (the copies on the card are left as they were).
int wp_save(const char *base, wp_record *rec);

// Reads the newest valid copy of base / base.tmp / base.bak (highest seq). 1 = found, 0 = none.
int wp_load(const char *base, wp_record *rec);

// Removes base, base.tmp and base.bak. Returns 0 when none is left.
int wp_remove(const char *base);

// 1 when every step is DONE (or was STARTED before a stop and is therefore not repeated).
int wp_all_done(const wp_record *rec);

// After a load: every STARTED step becomes DONE (it may have happened; doing it again could double it). Returns how
// many were changed (the caller tells the user and logs them).
int wp_settle_started(wp_record *rec);

void wp_init(wp_record *rec);
