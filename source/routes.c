// routes.c - see include/routes.h.
#include "routes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pw_image.h"
#include "wbx_sprites.h"

const char *routes_file_for(unsigned long long title_id, int gen, const char **version)
{
	const char *v = "all";
	const char *file = NULL;
	if (gen == 2) {
		file = "gsc";
		v = "all";
	} else if (gen == 6 && title_id == 0x0004000000055D00ULL) {
		file = "xy";
		v = "x";
	} else if (gen == 6 && title_id == 0x0004000000055E00ULL) {
		file = "xy";
		v = "y";
	} else if (gen == 7) { // Sun / Moon / Ultra Sun / Ultra Moon (game card and download share the title ID)
		static const struct { unsigned long long tid; const char *v; } g7[] = {
			{ 0x0004000000164800ULL, "sn" }, { 0x0004000000175E00ULL, "mn" },
			{ 0x00040000001B5000ULL, "us" }, { 0x00040000001B5100ULL, "um" },
		};
		for (size_t i = 0; i < sizeof g7 / sizeof g7[0]; i++)
			if (title_id == g7[i].tid) {
				file = "sm";
				v = g7[i].v;
			}
	}
	if (version)
		*version = v;
	return file;
}

// bounded copy (cuts quietly; names in the route files are short)
static void copyn(char *dst, size_t size, const char *src)
{
	size_t n = strlen(src);
	if (n >= size)
		n = size - 1;
	memcpy(dst, src, n);
	dst[n] = 0;
}

// U+2019 / U+2018 (curly apostrophes, e.g. "Farfetch’d", "King’s Rock") -> ASCII ' : the walker font has only that
static void plain_apostrophes(char *s)
{
	char *w = s;
	for (const char *r = s; *r;) {
		if ((unsigned char)r[0] == 0xE2 && (unsigned char)r[1] == 0x80 &&
		    ((unsigned char)r[2] == 0x99 || (unsigned char)r[2] == 0x98)) {
			*w++ = 0x27; // '
			r += 3;
		} else {
			*w++ = *r++;
		}
	}
	*w = 0;
}

static void trim(char *s)
{
	size_t n = strlen(s);
	while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
		s[--n] = 0;
	size_t i = 0;
	while (s[i] == ' ')
		i++;
	if (i)
		memmove(s, s + i, strlen(s + i) + 1);
}

static void field(const char *line, const char *key, char *out, size_t n)
{
	out[0] = 0;
	const char *p = strstr(line, key);
	if (!p)
		return;
	p += strlen(key);
	size_t k = 0;
	while (p[k] && p[k] != '|' && p[k] != '\n' && k + 1 < n) {
		out[k] = p[k];
		k++;
	}
	out[k] = 0;
	trim(out);
}

int routes_load(const char *name, route_file *out)
{
	memset(out, 0, sizeof(*out));
	copyn(out->file, sizeof(out->file), name);
	char path[160];
	snprintf(path, sizeof(path), ROUTES_DIR "/%s.txt", name);
	FILE *f = fopen(path, "r");
	if (!f)
		return -1;
	char line[256];
	route_course *c = NULL;
	while (fgets(line, sizeof(line), f)) {
		char *comment = strchr(line, ';');
		char note[64] = "";
		if (comment) {
			copyn(note, sizeof(note), comment + 1);
			plain_apostrophes(note);
			*comment = 0;
			trim(note);
		}
		trim(line);
		if (!line[0])
			continue;
		if (!strncmp(line, "items ", 6)) {
			out->item_gen = strstr(line, "gen2") ? 2 : 0;
		} else if (!strncmp(line, "course ", 7)) {
			if (out->ncourse >= ROUTES_MAX_COURSES) {
				out->rejected++; // audit M7
				break;
			}
			c = &out->course[out->ncourse++];
			memset(c, 0, sizeof(*c));
			c->number = atoi(line + 7);
			const char *bar = strchr(line, '|');
			if (bar) {
				size_t k = 0;
				for (const char *p = bar + 1; *p && *p != '|' && k + 1 < sizeof(c->name); p++)
					c->name[k++] = *p;
				c->name[k] = 0;
				trim(c->name);
			}
			field(line, "unlock=", c->unlock, sizeof(c->unlock));
			field(line, "picture=", c->picture, sizeof(c->picture));
			field(line, "favoured=", c->favoured, sizeof(c->favoured));
			field(line, "desc=", c->desc, sizeof(c->desc));
		} else if (!strncmp(line, "poke ", 5) && c && c->npoke < 6) {
			route_poke_def *p = &c->poke[c->npoke];
			int m[4];
			char g;
			if (sscanf(line + 5, " %c %d %d %d %d %d %d %d %d %d", &g, &p->slot, &p->chance, &p->species, &p->level,
			           &p->steps, &m[0], &m[1], &m[2], &m[3]) == 10) {
				p->group = g;
				for (int i = 0; i < 4; i++)
					p->moves[i] = (uint16_t)m[i];
				char *colon = strchr(note, ':'); // "Pidgey: Tackle, Sand Attack"
				if (colon)
					*colon = 0;
				copyn(p->name, sizeof(p->name), note);
				const char *fm = strstr(line, " form="); // Gen 7 files
				p->form = fm ? atoi(fm + 6) : 0;
				if (p->form < 0 || p->form > 31)
					p->form = 0;
				c->npoke++;
			} else
				out->rejected++; // audit M7
		} else if (!strncmp(line, "item ", 5) && c && c->nitem < 20) {
			route_item_def *it = &c->item[c->nitem];
			if (sscanf(line + 5, "%d %d %d %d %11s", &it->rank, &it->chance, &it->item, &it->steps, it->version) == 5) {
				copyn(it->name, sizeof(it->name), note);
				c->nitem++;
			} else
				out->rejected++;
		} else if (!strncmp(line, "poke ", 5) || !strncmp(line, "item ", 5)) {
			out->rejected++; // before any course, or a 7th Pokemon / 21st item
		}
	}
	fclose(f);
	return out->ncourse;
}

void routes_picture(const char *slug, uint8_t out[0xC0])
{
	uint8_t sh[32 * 24];
	memset(sh, 0, sizeof(sh));
	char path[160];
	snprintf(path, sizeof(path), ROUTES_DIR "/pictures/%s.txt", slug);
	FILE *f = fopen(path, "r");
	if (f) {
		char line[64];
		int y = 0;
		while (y < 24 && fgets(line, sizeof(line), f)) {
			if (line[0] == ';')
				continue;
			size_t n = strlen(line); // audit M8: a short line is blank after its end
			for (int x = 0; x < 32; x++) {
				char ch = (size_t)x < n ? line[x] : ' ';
				sh[y * 32 + x] = ch == '#' ? 3 : ch == '+' ? 2 : ch == '-' ? 1 : 0;
			}
			y++;
		}
		fclose(f);
	}
	pw_img_encode(sh, 32, 24, out);
}

int routes_build(const route_course *c, const char *version, int gen, unsigned seed, wbx_route *r)
{
	memset(r, 0, sizeof(*r));
	r->image_index = 0;
	wbx_text_pack(c->name, r->name, 21, 0);
	wbx_label_image(c->name, 80, 16, r->name_img);
	routes_picture(c->picture, r->image);
	// one Pokemon per walker slot: the two candidates of that slot's group, picked by the seed
	for (int slot = 0; slot < 3; slot++) {
		const route_poke_def *cand[2] = { NULL, NULL };
		int n = 0;
		for (int i = 0; i < c->npoke && n < 2; i++)
			if (c->poke[i].slot == slot)
				cand[n++] = &c->poke[i];
		if (!n)
			return -1;
		const route_poke_def *p = cand[n == 2 ? (seed >> slot) & 1 : 0];
		static uint8_t spr[WBX_SPRITE_SIZE];
		wbx_sprite_load_for(p->species, p->form, gen, spr);
		// a form travels in the summary (variant = form, flag "has form") and comes back with a catch
		wbx_summary_pack(r->poke[slot].summary, (uint16_t)p->species, 0, p->moves, (uint8_t)p->level,
		                 (uint8_t)p->form, p->form ? WBX_FLAG_HAS_FORM : 0);
		r->poke[slot].min_steps = (uint16_t)p->steps;
		r->poke[slot].percent = (uint8_t)p->chance;
		memcpy(r->poke[slot].small, spr, WBX_SMALL_SIZE);
		wbx_label_image(p->name, 80, 16, r->poke[slot].name_img);
		if (slot == 2) // as wbx_route_test: the large picture of walker slot 2
			memcpy(r->large_c, spr + WBX_SPRITE_SMALL_SIZE, WBX_LARGE_SIZE);
	}
	// items: rank 0..9, the line for this version (or "all")
	for (int rank = 0; rank < 10; rank++) {
		const route_item_def *pick = NULL;
		for (int i = 0; i < c->nitem; i++) {
			const route_item_def *it = &c->item[i];
			if (it->rank != rank)
				continue;
			if (!strcmp(it->version, "all") || (version && !strcmp(it->version, version)))
				pick = it;
		}
		if (!pick)
			return -1;
		r->item[rank] = (uint16_t)pick->item;
		r->item_steps[rank] = (uint16_t)pick->steps;
		r->item_percent[rank] = (uint8_t)pick->chance;
		wbx_label_image(pick->name, 96, 16, r->item_img[rank]);
	}
	return 0;
}

// v0.52 (user: "the next step is to block locked courses"): is the course open for this save's progress?
// "start" = always; "badges=N" = N or more badges; "hof" = Hall of Fame entered; "hof watts=N" = Hall of Fame and N
// or more watts collected (the watts PocketPKMN has banked); anything unknown = open (never lock by mistake).
int routes_unlocked(const char *u, int badges, int hof, unsigned watts)
{
	int n = 0;
	if (!strcmp(u, "start"))
		return 1;
	if (sscanf(u, "badges=%d", &n) == 1)
		return badges >= n;
	if (sscanf(u, "trials=%d", &n) == 1) // Gen 7: badges = island trials done
		return badges >= n;
	if (sscanf(u, "hof watts=%d", &n) == 1)
		return hof != 0 && watts >= (unsigned)n;
	if (!strncmp(u, "hof", 3))
		return hof != 0;
	return 1;
}

void routes_unlock_text(const char *u, char *out, int size)
{
	int n = 0;
	if (!strcmp(u, "start"))
		snprintf(out, (size_t)size, "From the start");
	else if (sscanf(u, "badges=%d", &n) == 1)
		snprintf(out, (size_t)size, "After badge %d", n);
	else if (sscanf(u, "trials=%d", &n) == 1) {
		// the island challenge in story order (Sun / Moon and Ultra Sun / Ultra Moon alike)
		static const char *const trial[] = { "Ilima's trial", "Hala's grand trial", "Lana's trial", "Kiawe's trial",
			"Mallow's trial", "Olivia's grand trial", "Sophocles' trial", "Acerola's trial", "Nanu's grand trial",
			"the Vast Poni trial", "Hapu's grand trial" };
		if (n >= 1 && n <= 11)
			snprintf(out, (size_t)size, "After %s", trial[n - 1]);
		else
			snprintf(out, (size_t)size, "After trial %d", n);
	}
	else if (sscanf(u, "hof watts=%d", &n) == 1)
		snprintf(out, (size_t)size, "Hall of Fame + %d W", n);
	else if (!strcmp(u, "hof"))
		snprintf(out, (size_t)size, "After the Hall of Fame");
	else
		snprintf(out, (size_t)size, "%s", u);
}
