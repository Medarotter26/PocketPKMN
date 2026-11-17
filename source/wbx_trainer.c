// wbx_trainer.c - the WalkerBox trainer (see include/wbx_trainer.h).
#include "wbx_trainer.h"

#include <stdio.h>
#include <string.h>

#include "safe_file.h" // v0.81 (review finding 5 / E, owner: "Go 12" (all three))
#include "walk_compose.h"

void wbx_trainer_clean_name(const char *in, char out[WBX_TRAINER_NAME_MAX + 1])
{
	int n = 0;
	for (; *in && n < WBX_TRAINER_NAME_MAX; in++) {
		char c = *in;
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == ' ' && n > 0))
			out[n++] = c;
	}
	while (n > 0 && out[n - 1] == ' ')
		n--;
	out[n] = '\0';
}

void wbx_trainer_new(wbx_trainer *t, const char *name, uint32_t (*rnd)(void))
{
	memset(t, 0, sizeof *t);
	wbx_trainer_clean_name(name, t->name);
	uint32_t a = rnd();
	t->tid = (uint16_t)a;
	t->sid = (uint16_t)(a >> 16);
	for (int i = 0; i < 0x28; i += 4) {
		uint32_t r = rnd();
		for (int k = 0; k < 4; k++)
			t->save_id[i + k] = (uint8_t)(r >> (8 * k));
	}
}

int wbx_trainer_save(const wbx_trainer *t, const char *path)
{
	// v0.81 (review finding 5 / E, owner: "Go 12" (all three)): the same text as before, put on the card by sf_write -
	// the old trainer.txt becomes trainer.txt.bak until the new one is in place (it was removed first, so a stop
	// in between left no trainer and the next start made a new one).
	char text[320];
	int n = snprintf(text, sizeof text, "# WalkerBox trainer - the owner of the walkers WalkerBox uses. Keep this file.\n");
	n += snprintf(text + n, sizeof text - n, "name=%s\ntid=%u\nsid=%u\nsave_id=", t->name, t->tid, t->sid);
	for (int i = 0; i < 0x28; i++)
		n += snprintf(text + n, sizeof text - n, "%02x", t->save_id[i]);
	n += snprintf(text + n, sizeof text - n, "\n");
	if (n >= (int)sizeof text || sf_write(path, text, (size_t)n) != 0)
		return -1;
	wbx_trainer back;
	return wbx_trainer_load(&back, path) == 0 && memcmp(&back, t, sizeof back) == 0 ? 0 : -1;
}

int wbx_trainer_load(wbx_trainer *t, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	memset(t, 0, sizeof *t);
	char line[160];
	int have = 0;
	while (fgets(line, sizeof line, f)) {
		line[strcspn(line, "\r\n")] = 0;
		unsigned v;
		if (!strncmp(line, "name=", 5)) {
			wbx_trainer_clean_name(line + 5, t->name);
			have |= t->name[0] ? 1 : 0;
		} else if (sscanf(line, "tid=%u", &v) == 1 && v <= 0xFFFF) {
			t->tid = (uint16_t)v;
			have |= 2;
		} else if (sscanf(line, "sid=%u", &v) == 1 && v <= 0xFFFF) {
			t->sid = (uint16_t)v;
			have |= 4;
		} else if (!strncmp(line, "save_id=", 8) && strlen(line + 8) == 0x50) {
			int ok = 1;
			for (int i = 0; i < 0x28 && ok; i++) {
				unsigned b;
				ok = sscanf(line + 8 + 2 * i, "%2x", &b) == 1;
				t->save_id[i] = (uint8_t)b;
			}
			if (ok)
				have |= 8;
		}
	}
	fclose(f);
	return have == 15 ? 0 : -1;
}

// v0.81 (review finding 5 / E, owner: "Go 12" (all three))
static int trainer_valid(const char *path, void *ctx)
{
	(void)ctx;
	wbx_trainer t;
	return wbx_trainer_load(&t, path) == 0;
}

int wbx_trainer_load_any(wbx_trainer *t, const char *path)
{
	char use[512];
	int from = sf_pick(path, trainer_valid, NULL, use, sizeof use);
	if (from < 0 || wbx_trainer_load(t, use) != 0)
		return -1;
	return from;
}

void wbx_trainer_identity(const wbx_trainer *t, const uint8_t base[0x68], uint8_t out[0x68])
{
	memcpy(out, base, 0x68);
	out[0x0C] = (uint8_t)t->tid;
	out[0x0D] = (uint8_t)(t->tid >> 8);
	out[0x0E] = (uint8_t)t->sid;
	out[0x0F] = (uint8_t)(t->sid >> 8);
	// +0x10 stays as in base (the walker's own unique id): that is what every walk so far has sent, and the
	// walker does not keep this field. t->save_id is kept for later use.
	uint16_t text[8];
	wbx_text_pack(t->name, text, 8, 0); // name, 0xFFFF, zeros - the form the walker holds (user's backup)
	for (int i = 0; i < 8; i++) {
		out[0x48 + 2 * i] = (uint8_t)text[i];
		out[0x49 + 2 * i] = (uint8_t)(text[i] >> 8);
	}
}

int wbx_trainer_owns(const wbx_trainer *t, const uint8_t identity[0x68])
{
	return identity[0x0C] == (uint8_t)t->tid && identity[0x0D] == (uint8_t)(t->tid >> 8) &&
	       identity[0x0E] == (uint8_t)t->sid && identity[0x0F] == (uint8_t)(t->sid >> 8);
}
