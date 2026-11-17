// v0.81 (review finding 5 / E, owner: "Go 12" (all three)): see include/safe_file.h.
#include "safe_file.h"
#include <stdio.h>
#include <string.h>

#define SF_PATH 512

static int sf_exists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	fclose(f);
	return 1;
}

// 1 = the file holds exactly these len bytes
static int sf_same(const char *path, const void *data, size_t len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	const unsigned char *d = data;
	unsigned char buf[1024];
	size_t at = 0;
	int ok = 1;
	while (ok && at < len) {
		size_t want = len - at < sizeof(buf) ? len - at : sizeof(buf);
		ok = fread(buf, 1, want, f) == want && memcmp(buf, d + at, want) == 0;
		at += want;
	}
	if (ok && fgetc(f) != EOF)
		ok = 0;
	fclose(f);
	return ok;
}

int sf_write(const char *base, const void *data, size_t len)
{
	char tmp[SF_PATH], bak[SF_PATH];
	if (snprintf(tmp, sizeof(tmp), "%s.tmp", base) >= (int)sizeof(tmp) ||
	    snprintf(bak, sizeof(bak), "%s.bak", base) >= (int)sizeof(bak))
		return -1;
	FILE *f = fopen(tmp, "wb");
	if (!f)
		return -1;
	int ok = fwrite(data, 1, len, f) == len;
	if (fclose(f) != 0)
		ok = 0;
	if (!ok || !sf_same(tmp, data, len)) {
		remove(tmp); // base (the old file) is untouched
		return -1;
	}
	// from here the new content is complete in .tmp; every step below leaves a complete copy on the card
	remove(bak); // an old left-over (base is there, or .tmp is newer)
	int had = sf_exists(base);
	if (had && rename(base, bak) != 0) {
		remove(tmp);
		return -1;
	}
	if (rename(tmp, base) != 0) {
		if (had && rename(bak, base) == 0) // the old one back where the game reads it
			remove(tmp);
		return -1;
	}
	if (!sf_same(base, data, len))
		return -1; // .bak (the old one) is kept
	remove(bak);
	return 0;
}

int sf_pick(const char *base, sf_valid_fn valid, void *ctx, char *path, size_t n)
{
	snprintf(path, n, "%s", base);
	if (valid(base, ctx))
		return 0;
	int there = sf_exists(base);
	static const char *const ext[2] = {".tmp", ".bak"};
	for (int i = 0; i < 2; i++) {
		char alt[SF_PATH];
		if (snprintf(alt, sizeof(alt), "%s%s", base, ext[i]) >= (int)sizeof(alt) || !valid(alt, ctx))
			continue;
		if (there || rename(alt, base) != 0)
			snprintf(path, n, "%s", alt);
		return i + 1;
	}
	return -1;
}

int sf_text_valid(const char *path, void *ctx)
{
	const char *head = ctx;
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	size_t hl = strlen(head);
	char line[128];
	int ok = hl < sizeof(line) - 1 && fread(line, 1, hl + 1, f) == hl + 1 && memcmp(line, head, hl) == 0 &&
	         line[hl] == '\n';
	int c, last = '\n';
	while (ok && (c = fgetc(f)) != EOF)
		last = c;
	fclose(f);
	return ok && last == '\n';
}
