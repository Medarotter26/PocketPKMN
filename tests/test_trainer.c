// test_trainer.c - PC checks of source/wbx_trainer.c. Usage: test_trainer <walker backup>
#include <stdio.h>
#include <string.h>

#include "pw_proto.h"
#include "wbx_trainer.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static uint32_t seed = 12345;
static uint32_t rnd(void) { return seed = seed * 1103515245u + 12345u; }

int main(int argc, char **argv)
{
	char c[8];
	wbx_trainer_clean_name("  Bé la!xyz9", c);
	CHECK(strcmp(c, "B laxyz") == 0, "clean '%s'", c); // leading spaces, 'é', '!' left out; max 7
	wbx_trainer_clean_name("Ash Ketchum", c);
	CHECK(strcmp(c, "Ash Ket") == 0, "clean '%s'", c);
	wbx_trainer_clean_name("ABCDEF ", c);
	CHECK(strcmp(c, "ABCDEF") == 0, "trailing space '%s'", c);

	wbx_trainer t, back;
	wbx_trainer_new(&t, "Walker", rnd);
	CHECK(strcmp(t.name, "Walker") == 0, "name");
	CHECK(wbx_trainer_save(&t, "/tmp/wbx_trainer_test.txt") == 0, "save");
	CHECK(wbx_trainer_load(&back, "/tmp/wbx_trainer_test.txt") == 0 && memcmp(&back, &t, sizeof t) == 0, "load");
	FILE *f = fopen("/tmp/wbx_trainer_bad.txt", "w");
	fprintf(f, "name=X\ntid=5\n");
	fclose(f);
	CHECK(wbx_trainer_load(&back, "/tmp/wbx_trainer_bad.txt") != 0, "incomplete file accepted");
	CHECK(wbx_trainer_load(&back, "/tmp/wbx_no_such_file.txt") != 0, "missing file accepted");

	if (argc > 1) {
		static uint8_t ee[0x10000];
		f = fopen(argv[1], "rb");
		CHECK(f && fread(ee, 1, sizeof ee, f) == sizeof ee, "read backup");
		if (f)
			fclose(f);
		const uint8_t *walker = ee + 0x00ED; // IdentityData as the walker keeps it (= its 22 reply)
		uint8_t id[0x68];
		CHECK(!wbx_trainer_owns(&t, walker), "owns the user's walker before");
		wbx_trainer_identity(&t, walker, id);
		CHECK(wbx_trainer_owns(&t, id), "identity not owned");
		int other = 0;
		for (int i = 0; i < 0x68; i++)
			if (!(i >= 0x0C && i < 0x10) && !(i >= 0x48 && i < 0x58))
				other += id[i] != walker[i];
		CHECK(other == 0, "%d byte(s) outside TID/SID/name changed", other);
		// name in the walker's form: chars, FFFF, zeros (e.g. a 4-letter name = 4 chars, ffff, then zeros)
		// W 0x0141, a 0x0145, l 0x0150, k 0x014F, e 0x0149, r 0x0156 (A = 0x012B, a = 0x0145)
	const uint8_t want[16] = { 0x41, 0x01, 0x45, 0x01, 0x50, 0x01, 0x4F, 0x01, 0x49, 0x01, 0x56, 0x01, 0xFF, 0xFF, 0, 0 };
		CHECK(memcmp(id + 0x48, want, 16) == 0, "name bytes");
		CHECK(walker[0x48 + 8] == 0xFF && walker[0x48 + 9] == 0xFF && walker[0x48 + 10] == 0, "walker's own name form");
		pw_identity p;
		pw_identity_parse(id, &p);
		char name[12];
		pw_text_to_ascii(p.trainer_name, 8, name, sizeof name);
		CHECK(strcmp(name, "Walker") == 0 && p.tid == t.tid && p.sid == t.sid, "parsed '%s' %u %u", name, p.tid, p.sid);
	}
	printf("%s (%d failure(s))\n", fails ? "FAILED" : "ALL TRAINER CHECKS PASSED", fails);
	return fails != 0;
}
