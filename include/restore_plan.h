// Walker -> Restore: which 128-byte blocks of a backup file may be written to the walker, and in what order
// (v0.68, review addendum finding B). Plain C with no 3DS calls, so tests/test_restore.c can run it on the PC.
#pragma once
#include <stdint.h>

#define RP_EEPROM_SIZE   0x10000
#define RP_CHUNK         0x80
#define RP_FROM          0x0280  // below this the walker keeps its own identity / steps header: never written
#define RP_CHECKED_FROM  0x8C80  // the blocks that hold the checksum table (0x8CB0) and the data it covers
#define RP_CHECKED_TO    0x8F00
#define RP_UNIQUE_ID     0x0083  // the walker's unique ID, 0x28 bytes
#define RP_NAME_FROM     0x1200  // v0.75: the blocks with the trainer-name picture (0x1250-0x138F): never restored
#define RP_NAME_TO       0x1400
#define RP_MAX_CHUNKS    ((RP_EEPROM_SIZE - RP_FROM) / RP_CHUNK)

enum {
	RP_OK = 0,              // write `order[0..count-1]` (count may be 0: the walker already matches)
	RP_REFUSE_FILE_BLOCK,   // the file's checksummed block is not valid
	RP_REFUSE_OTHER_WALKER, // the file is from another walker (or from before this walker was reset)
	RP_REFUSE_BOTH_BAD,     // neither the walker's nor the file's block is valid (not reached: file checked first)
};

typedef struct {
	int result;
	int repair;              // 1: the walker's own checksummed block is broken and the file's valid one goes back
	unsigned count;          // blocks to write
	uint16_t order[RP_MAX_CHUNKS]; // their addresses, in writing order
} restore_plan;

// 1 when every one of the 16 checksum-table entries at 0x8CB0 matches and at least one covers bytes. An entry may
// only cover 0x8CF0-0x8ECF (offset from 0x8CF0, end not past 0x8ED0), so a table that passes never covers the
// menu pictures 0x0280-0x8C7F.
int rp_checked_block_ok(const uint8_t *ee);

// Covered range of a table that passes (for the log / tests): lowest address and one past the highest.
void rp_checked_range(const uint8_t *ee, unsigned *lo, unsigned *hi);

// 1 when the 128-byte block at a may be restored: menu pictures 0x0280-0x8C7F except 0x1200-0x13FF (the
// trainer-name picture, v0.75), event log 0xCF00-0xD47F, peers 0xDC00-0xFFFF (v0.70). Never the walk areas, the team
// data or the checked block (see restore_plan.c). v0.80 (advisor B3): so a Set up write cut off inside 0x1200-0x13FF
// can only be repaired by running Set up again, not by Restore.
int rp_restorable(unsigned a);

// file = the backup, walker = the walker as just read (two full reads that agreed).
// Rules: the file's block must be valid; the file must be this walker's (same unique ID); every differing block
// that rp_restorable allows is written; the checksummed blocks 0x8C80-0x8EFF are written ONLY when the walker's
// own block is broken (repair) - then last of all, data blocks first and the table block (0x8C80) at the very end.
void rp_make_plan(const uint8_t *file, const uint8_t *walker, restore_plan *p);
