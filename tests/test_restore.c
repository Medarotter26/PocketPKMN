// PC test of the Walker -> Restore plan (v0.68, review addendum finding B): source/restore_plan.c.
// Arguments: the walker backups to use (all from hw/). For each file F, and each other backup W of the same walker:
//   - F onto itself: nothing to write;
//   - F onto W: the result equals F from 0x0280 up, W below 0x0280, and the checksummed blocks are never written
//     (W's own block is valid);
//   - F with a broken checked block: refused; a blank file (all 0xFF / all 0x00 after "nintendo"): refused;
//   - F as "another walker" (unique ID changed): refused;
//   - W with a broken block + valid F: repair - the checked blocks come last, the table block 0x8C80 at the very
//     end, and the result passes the check;
//   - the table of every file covers only 0x8CF0-0x8ECF (never the pictures 0x0280-0x8C7F).
#include "restore_plan.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint8_t *load(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	uint8_t *b = malloc(RP_EEPROM_SIZE);
	size_t n = fread(b, 1, RP_EEPROM_SIZE, f);
	fclose(f);
	if (n != RP_EEPROM_SIZE) {
		free(b);
		return NULL;
	}
	return b;
}

static void apply(const uint8_t *file, uint8_t *walker, const restore_plan *p)
{
	for (unsigned i = 0; i < p->count; i++)
		memcpy(walker + p->order[i], file + p->order[i], RP_CHUNK);
}

static int in_checked(unsigned a) { return a >= RP_CHECKED_FROM && a < RP_CHECKED_TO; }

int main(int argc, char **argv)
{
	int n = argc - 1;
	if (n < 2) {
		printf("usage: test_restore backup...\n");
		return 2;
	}
	uint8_t **f = calloc((size_t)n, sizeof(*f));
	for (int i = 0; i < n; i++) {
		f[i] = load(argv[i + 1]);
		if (!f[i]) {
			printf("cannot read %s\n", argv[i + 1]);
			return 2;
		}
	}
	static uint8_t w[RP_EEPROM_SIZE], g[RP_EEPROM_SIZE];
	static restore_plan p;
	unsigned pairs = 0, writes = 0, walk_kept = 0;

	// v0.70: the restorable areas, block by block (owner: menu pictures, event log, peers only)
	for (unsigned a = 0; a < RP_EEPROM_SIZE; a += RP_CHUNK) {
		int want = (a >= 0x0280 && a < 0x8C80) || (a >= 0xCF00 && a < 0xD480) || a >= 0xDC00;
		CHECK(rp_restorable(a) == want, "block 0x%04X: restorable %d", a, rp_restorable(a));
	}
	static const unsigned never[] = {0x8C80, 0x8F00, 0xB780, 0xB800, 0xCC00, 0xCE00, 0xCE80, 0xD480, 0xDB80};
	for (unsigned k = 0; k < sizeof(never) / sizeof(never[0]); k++)
		CHECK(!rp_restorable(never[k]), "walk / team / checked block 0x%04X is restorable", never[k]);
	for (int i = 0; i < n; i++) {
		const char *name = argv[i + 1];
		CHECK(rp_checked_block_ok(f[i]), "%s: checked block not valid", name);
		unsigned lo, hi;
		rp_checked_range(f[i], &lo, &hi);
		CHECK(lo >= 0x8CF0 && hi <= 0x8ED0, "%s: table covers 0x%04X-0x%04X", name, lo, hi - 1);

		rp_make_plan(f[i], f[i], &p);
		CHECK(p.result == RP_OK && p.count == 0 && !p.repair, "%s onto itself: result %d, %u block(s)", name,
		      p.result, p.count);

		for (int j = 0; j < n; j++) {
			if (j == i)
				continue;
			memcpy(w, f[j], RP_EEPROM_SIZE);
			rp_make_plan(f[i], w, &p);
			CHECK(p.result == RP_OK && !p.repair, "%s onto %s: result %d", name, argv[j + 1], p.result);
			for (unsigned k = 0; k < p.count; k++)
				CHECK(rp_restorable(p.order[k]), "%s onto %s: writes 0x%04X", name, argv[j + 1], p.order[k]);
			apply(f[i], w, &p);
			CHECK(!memcmp(w, f[j], RP_FROM), "%s onto %s: header changed", name, argv[j + 1]);
			// v0.70: the restorable areas = the backup, everything else (walk areas, team, checked block) = the
			// walker's own bytes
			for (unsigned a = RP_FROM; a < RP_EEPROM_SIZE; a += RP_CHUNK) {
				const uint8_t *want = rp_restorable(a) ? f[i] + a : f[j] + a;
				if (memcmp(w + a, want, RP_CHUNK) != 0) {
					CHECK(0, "%s onto %s: block 0x%04X is wrong", name, argv[j + 1], a);
					break;
				}
				if (!rp_restorable(a) && memcmp(f[i] + a, f[j] + a, RP_CHUNK) != 0)
					walk_kept++;
			}
			pairs++;
			writes += p.count;
		}

		// a broken checked block in the file: every covered byte and every table byte, one at a time
		unsigned broken = 0, refused = 0;
		for (unsigned a = 0x8CB0; a < 0x8E0E; a++) {
			memcpy(g, f[i], RP_EEPROM_SIZE);
			g[a] ^= 0x01;
			if (rp_checked_block_ok(g))
				continue; // a change in a table entry's own sum byte that still matches cannot happen; skip safe ones
			broken++;
			rp_make_plan(g, f[i], &p);
			refused += p.result == RP_REFUSE_FILE_BLOCK && p.count == 0;
		}
		CHECK(broken > 0x100 && refused == broken, "%s: %u of %u broken files refused", name, refused, broken);

		for (int fill = 0; fill < 2; fill++) {
			memcpy(g, f[i], RP_EEPROM_SIZE);
			memset(g + 8, fill ? 0x00 : 0xFF, RP_EEPROM_SIZE - 8);
			rp_make_plan(g, f[i], &p);
			CHECK(p.result != RP_OK && p.count == 0, "%s: blank (%s) file accepted", name, fill ? "00" : "FF");
		}

		memcpy(g, f[i], RP_EEPROM_SIZE);
		g[RP_UNIQUE_ID + 5] ^= 0x40;
		rp_make_plan(g, f[i], &p);
		CHECK(p.result == RP_REFUSE_OTHER_WALKER && p.count == 0, "%s: other walker's backup: result %d", name,
		      p.result);

		// repair: the walker's block broken (a data byte, then a table byte)
		for (int t = 0; t < 2; t++) {
			memcpy(w, f[(i + 1) % n], RP_EEPROM_SIZE);
			w[t ? 0x8CB3 : 0x8D50] ^= 0x10;
			CHECK(!rp_checked_block_ok(w), "%s: could not break the walker's block", name);
			rp_make_plan(f[i], w, &p);
			CHECK(p.result == RP_OK && p.repair && p.count > 0, "%s: repair not planned (result %d)", name, p.result);
			int seen_checked = 0, order_ok = 1;
			for (unsigned k = 0; k < p.count; k++) {
				if (in_checked(p.order[k]))
					seen_checked = 1;
				else if (seen_checked)
					order_ok = 0; // a normal block after a checked one
			}
			CHECK(order_ok, "%s: checked blocks not last", name);
			if (t)
				CHECK(p.order[p.count - 1] == RP_CHECKED_FROM, "%s: table block not written last", name);
			apply(f[i], w, &p);
			CHECK(rp_checked_block_ok(w) && !memcmp(w + RP_CHECKED_FROM, f[i] + RP_CHECKED_FROM,
			                                        RP_CHECKED_TO - RP_CHECKED_FROM),
			      "%s: repair result wrong", name);
		}
	}
	// the walk areas really differed between some backups (else "untouched" proves nothing)
	CHECK(walk_kept > 1000, "only %u differing walk-area block(s) seen", walk_kept);
	printf("restore plan: %d backup(s), %u pair(s), %u block(s) planned, %u differing walk/team block(s) left as "
	       "the walker had them, %d failure(s)\n", n, pairs, writes, walk_kept, fails);
	for (int i = 0; i < n; i++)
		free(f[i]);
	free(f);
	if (fails)
		return 1;
	printf("restore tests passed\n");
	return 0;
}
