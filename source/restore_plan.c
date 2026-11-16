// Walker -> Restore plan (v0.68, review addendum finding B). See include/restore_plan.h.
//
// Why the checksummed block is handled apart: the walker's firmware checks the 16 sums of the table at 0x8CB0
// at random times and hangs if one is wrong (dmitry.gr Pokewalker write-up, function 0x36F2; docs/PAIRING.md
// section 5). The table and the data it covers sit in four separate 128-byte writes, so no order makes a change
// of that block safe against a cut. It is therefore never changed on a walker whose own block is valid (all real
// walkers seen so far have byte-identical blocks), and only rewritten to repair a walker whose block is already
// broken - where a cut leaves it no worse than before.
#include "restore_plan.h"
#include <string.h>

#define RP_TABLE 0x8CB0
#define RP_DATA  0x8CF0
#define RP_END   0x8ED0

int rp_checked_block_ok(const uint8_t *ee)
{
	int valid = 0;
	for (int i = 0; i < 16; i++) {
		const uint8_t *d = ee + RP_TABLE + 4 * i;
		unsigned off = (unsigned)(d[0] | (d[1] << 8)), len = d[2];
		if (RP_DATA + off + len > RP_END)
			return 0;
		uint8_t sum = 0;
		for (unsigned k = 0; k < len; k++)
			sum = (uint8_t)(sum + ee[RP_DATA + off + k]);
		if (sum != d[3])
			return 0;
		valid += len > 0;
	}
	return valid > 0;
}

void rp_checked_range(const uint8_t *ee, unsigned *lo, unsigned *hi)
{
	*lo = RP_EEPROM_SIZE;
	*hi = 0;
	for (int i = 0; i < 16; i++) {
		const uint8_t *d = ee + RP_TABLE + 4 * i;
		unsigned a = RP_DATA + (unsigned)(d[0] | (d[1] << 8)), len = d[2];
		if (!len)
			continue;
		if (a < *lo)
			*lo = a;
		if (a + len > *hi)
			*hi = a + len;
	}
}

// v0.70 (owner: "Restore never writes the walk areas ... it restores menu pictures, event log and peers only"):
// the walk areas belong to the walker's CURRENT walk, which its header below 0x0280 (never restored) describes;
// writing an older walk back could let its catches, items and gains be collected a second time.
// Restored (128-byte blocks): menu pictures 0x0280-0x8C7F, event log 0xCF00-0xD47F, peers 0xDC00-0xFFFF.
// Never: the checked block 0x8C80-0x8EFF (only the repair below), route + walking Pokemon 0x8F00-0xB7FF,
// special route / event Pokemon and item 0xB800-0xCBFF, team data 0xCC00-0xCE7F, watts copy + catches + items of the
// walk 0xCE80-0xCEFF, walk-start staging 0xD480-0xDBFF.
int rp_restorable(unsigned a)
{
	return (a >= RP_FROM && a < RP_CHECKED_FROM) || (a >= 0xCF00 && a < 0xD480) || (a >= 0xDC00 && a < RP_EEPROM_SIZE);
}

void rp_make_plan(const uint8_t *file, const uint8_t *walker, restore_plan *p)
{
	memset(p, 0, sizeof(*p));
	if (!rp_checked_block_ok(file)) {
		p->result = rp_checked_block_ok(walker) ? RP_REFUSE_FILE_BLOCK : RP_REFUSE_BOTH_BAD;
		return;
	}
	if (memcmp(file + RP_UNIQUE_ID, walker + RP_UNIQUE_ID, 0x28) != 0) {
		p->result = RP_REFUSE_OTHER_WALKER;
		return;
	}
	for (unsigned a = RP_FROM; a < RP_EEPROM_SIZE; a += RP_CHUNK)
		if (rp_restorable(a) && memcmp(file + a, walker + a, RP_CHUNK) != 0)
			p->order[p->count++] = (uint16_t)a;
	if (!rp_checked_block_ok(walker)) {
		// repair: data blocks first, the table block 0x8C80 last
		unsigned before = p->count;
		for (unsigned a = RP_CHECKED_FROM + RP_CHUNK; a < RP_CHECKED_TO; a += RP_CHUNK)
			if (memcmp(file + a, walker + a, RP_CHUNK) != 0)
				p->order[p->count++] = (uint16_t)a;
		if (memcmp(file + RP_CHECKED_FROM, walker + RP_CHECKED_FROM, RP_CHUNK) != 0)
			p->order[p->count++] = RP_CHECKED_FROM;
		p->repair = p->count > before;
	}
	p->result = RP_OK;
}
