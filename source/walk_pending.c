// Pending walk results (v0.68, review finding 4 + addendum C). See include/walk_pending.h.
#include "walk_pending.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define WP_MAGIC   0x31505757u // "WWP1"
#define WP_VERSION 1u

static uint32_t wp_fnv(const void *p, size_t n)
{
	const uint8_t *b = p;
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < n; i++)
		h = (h ^ b[i]) * 16777619u;
	return h;
}

static uint32_t wp_sum(const wp_record *r) { return wp_fnv(r, offsetof(wp_record, check)); }

void wp_init(wp_record *rec)
{
	memset(rec, 0, sizeof(*rec));
	rec->magic = WP_MAGIC;
	rec->version = WP_VERSION;
	rec->ev = -1;
	rec->bank_box = rec->bank_slot = -1;
}

static int read_one(const char *path, wp_record *out)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	wp_record r;
	size_t n = fread(&r, 1, sizeof(r), f);
	int extra = fgetc(f) != EOF;
	fclose(f);
	if (n != sizeof(r) || extra || r.magic != WP_MAGIC || r.version != WP_VERSION || r.check != wp_sum(&r))
		return 0;
	*out = r;
	return 1;
}

int wp_load(const char *base, wp_record *rec)
{
	char path[160];
	const char *ext[3] = {"", ".tmp", ".bak"};
	int found = 0;
	for (int i = 0; i < 3; i++) {
		wp_record r;
		snprintf(path, sizeof(path), "%s%s", base, ext[i]);
		if (read_one(path, &r) && (!found || r.seq > rec->seq)) {
			*rec = r;
			found = 1;
		}
	}
	return found;
}

int wp_save(const char *base, wp_record *rec)
{
	char tmp[160], bak[160];
	snprintf(tmp, sizeof(tmp), "%s.tmp", base);
	snprintf(bak, sizeof(bak), "%s.bak", base);
	rec->seq++;
	rec->check = wp_sum(rec);
	FILE *f = fopen(tmp, "wb");
	if (!f)
		return -1;
	int ok = fwrite(rec, 1, sizeof(*rec), f) == sizeof(*rec);
	if (fclose(f) != 0)
		ok = 0;
	wp_record back;
	if (!ok || !read_one(tmp, &back) || memcmp(&back, rec, sizeof(back)) != 0)
		return -1; // the copies already on the card stay; the newer seq in .tmp is ignored if it is broken
	// the newest valid copy is now .tmp; the steps below never leave the card without a valid newest copy
	remove(bak);
	FILE *old = fopen(base, "rb");
	if (old) {
		fclose(old);
		if (rename(base, bak) != 0)
			return -1;
	}
	if (rename(tmp, base) != 0)
		return -1;
	remove(bak);
	return 0;
}

int wp_remove(const char *base)
{
	char path[160];
	const char *ext[3] = {"", ".tmp", ".bak"};
	int left = 0;
	for (int i = 0; i < 3; i++) {
		snprintf(path, sizeof(path), "%s%s", base, ext[i]);
		remove(path);
		FILE *f = fopen(path, "rb");
		if (f) {
			fclose(f);
			left++;
		}
	}
	return left ? -1 : 0;
}

int wp_all_done(const wp_record *r)
{
	if (r->state != WP_ENDED || r->st_watts == WP_TODO || r->st_gains == WP_TODO)
		return 0;
	if (r->one_slot) {
		for (int i = 0; i < r->ncatch && i < WP_MAX_CATCHES; i++)
			if (r->st_catch[i] == WP_TODO)
				return 0;
		for (int i = 0; i < r->nitem && i < WP_MAX_ITEMS; i++)
			if (r->st_item[i] == WP_TODO)
				return 0;
		if (r->st_return == WP_TODO)
			return 0;
	}
	return 1;
}

int wp_settle_started(wp_record *r)
{
	int n = 0;
	uint8_t *all[3 + WP_MAX_CATCHES + WP_MAX_ITEMS];
	int k = 0;
	all[k++] = &r->st_watts;
	all[k++] = &r->st_gains;
	all[k++] = &r->st_return;
	for (int i = 0; i < WP_MAX_CATCHES; i++)
		all[k++] = &r->st_catch[i];
	for (int i = 0; i < WP_MAX_ITEMS; i++)
		all[k++] = &r->st_item[i];
	for (int i = 0; i < k; i++)
		if (*all[i] == WP_STARTED) {
			*all[i] = WP_DONE;
			n++;
		}
	return n;
}
