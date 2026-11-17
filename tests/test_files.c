// PC test of the whole-file saves (v0.81, review finding 5 / E, owner: "Go 12" (all three)): source/safe_file.c, used by
// the trainer (source/wbx_trainer.c, built as it is) and by code that is not built on the PC - here as copies of it:
// main.c watts_add / watts_read and settings_save / settings_load, storage.cpp dsWrite (TWiLight Menu .sav) and
// scanSdForDsSaves (one folder).
// Every file step (each fopen, write, fclose, remove, rename) can be the last one before a stop (power cut). A stop
// inside a write leaves half of it on the card, or (second pass) an empty file. Then the app starts again (load),
// saves once more and may be stopped there too. At every point the loader must find the old content or the new one:
// never neither, never a mix.
// Built with -DOLD_CODE against the old source tree (tests/run_files.sh --old DIR) the copies are the old code and
// the test shows where data was lost (it FAILs there).
#include <dirent.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wbx_trainer.h"
#ifndef OLD_CODE
#include "safe_file.h"
#endif

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { if (++fails <= 30) { printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } } while (0)

// wbx_trainer_identity's helper (source/walk_compose.c, not needed here)
int wbx_text_pack(const char *utf8, uint16_t *out, int slots, int last_ffff)
{
	(void)utf8; (void)out; (void)slots; (void)last_ffff;
	return 0;
}

// --- the stop: every file call goes through these (linked with -Wl,--wrap=...) ------------------------------------
FILE *__real_fopen(const char *path, const char *mode);
int __real_fclose(FILE *f);
size_t __real_fwrite(const void *p, size_t size, size_t n, FILE *f);
int __real_remove(const char *path);
int __real_rename(const char *from, const char *to);

static long budget = -1; // file steps left before the stop; -1 = no stop
static long ops;
static int empty_mode;   // 1: a file open for writing at the stop is left empty (else: what was written so far)
static jmp_buf stop_at;
#define MAX_OPEN 16
static FILE *opened[MAX_OPEN];
static int opened_w[MAX_OPEN];

static void stop_now(void)
{
	for (int i = 0; i < MAX_OPEN; i++)
		if (opened[i]) {
			if (opened_w[i] && empty_mode && ftruncate(fileno(opened[i]), 0) != 0)
				printf("ftruncate failed\n");
			__real_fclose(opened[i]);
			opened[i] = NULL;
		}
	budget = -1;
	longjmp(stop_at, 1);
}

static int step(void)
{
	ops++;
	return budget >= 0 && budget-- == 0;
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
	if (step())
		stop_now();
	FILE *f = __real_fopen(path, mode);
	if (f) {
		int w = strchr(mode, 'w') || strchr(mode, 'a');
		if (w)
			setvbuf(f, NULL, _IONBF, 0); // every write reaches the file at once (a stop loses nothing written)
		for (int i = 0; i < MAX_OPEN; i++)
			if (!opened[i]) {
				opened[i] = f;
				opened_w[i] = w;
				break;
			}
	}
	return f;
}

int __wrap_fclose(FILE *f)
{
	if (step())
		stop_now();
	for (int i = 0; i < MAX_OPEN; i++)
		if (opened[i] == f)
			opened[i] = NULL;
	return __real_fclose(f);
}

size_t __wrap_fwrite(const void *p, size_t size, size_t n, FILE *f)
{
	if (step()) {
		__real_fwrite(p, 1, size * n / 2, f); // half of it reached the card
		stop_now();
	}
	return __real_fwrite(p, size, n, f);
}

int __wrap_fprintf(FILE *f, const char *fmt, ...)
{
	char buf[1024];
	va_list a;
	va_start(a, fmt);
	int n = vsnprintf(buf, sizeof(buf), fmt, a);
	va_end(a);
	if (step()) {
		__real_fwrite(buf, 1, (size_t)n / 2, f);
		stop_now();
	}
	return __real_fwrite(buf, 1, (size_t)n, f) == (size_t)n ? n : -1;
}

int __wrap_remove(const char *path)
{
	if (step())
		stop_now();
	return __real_remove(path);
}

int __wrap_rename(const char *from, const char *to)
{
	if (step())
		stop_now();
	struct stat st;
	if (stat(to, &st) == 0)
		return -1; // as on the 3DS SD card (FAT, devkitPro newlib): a rename onto an existing file fails
	return __real_rename(from, to);
}

static char dir[64];

static void clean(void)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char p[400];
	while (d && (e = readdir(d)))
		if (e->d_name[0] != '.') {
			snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
			__real_remove(p);
		}
	if (d)
		closedir(d);
}

static int exists(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0;
}

enum { ST_A, ST_B, ST_C, ST_D }; // A = before, B = the save, C / D = the next save made from A / B

typedef struct scen {
	const char *name;
	const char *main_file;
	void (*setup)(void);   // the card before the save: content A, as a good save leaves it
	int (*save)(int nth);  // 1: A -> B; 2: the next save, after the start again
	int (*load)(void);     // the app's start: the content it gets (ST_*), -1 = none / a mix
	int (*next)(int from); // the content the second save makes from what was loaded
} scen;

static int next_c(int from) { (void)from; return ST_C; }

static const char *st_name(int v)
{
	static const char *const n[] = {"old", "new", "next (from old)", "next (from new)"};
	return v >= 0 && v <= ST_D ? n[v] : "NOTHING VALID (a fresh start / zero / a mix)";
}

// The first save from A, stopped before file step k (-1: not stopped). 1 = it ran to its end.
static int save_stopped(const scen *s, long k)
{
	clean();
	s->setup();
	ops = 0;
	budget = k;
	if (setjmp(stop_at))
		return 0;
	s->save(1);
	budget = -1;
	return 1;
}

// The start again (load) and the next save, stopped before file step k. 1 = both ran to their end; *v = what the
// load found (-2: stopped before it ended).
static int again_stopped(const scen *s, long k, volatile int *v)
{
	*v = -2;
	ops = 0;
	budget = k;
	if (setjmp(stop_at))
		return 0;
	*v = s->load();
	s->save(2);
	budget = -1;
	return 1;
}

static void run(const scen *s, int empty)
{
	empty_mode = empty;
	int lost = 0, lost2 = 0, gone = 0, f0 = fails;
	long k1;
	for (k1 = 0;; k1++) {
		int done1 = save_stopped(s, k1);
		if (!done1 && !exists(s->main_file))
			gone++; // what TWiLight Menu (or the old loader) sees before the app starts again
		int v = s->load();
		if (done1)
			CHECK(v == ST_B, "%s: a save with no stop: the start found %s", s->name, st_name(v));
		else
			CHECK(v == ST_A || v == ST_B, "%s%s: stop before file step %ld of the save: the next start found %s",
			      s->name, empty ? " (empty file)" : "", k1, st_name(v));
		if (v != ST_A && v != ST_B)
			lost++;
		// the start again (may itself be stopped) and the next save, stopped at each of its steps
		for (long k2 = 0;; k2++) {
			save_stopped(s, done1 ? -1 : k1);
			volatile int v2;
			int done2 = again_stopped(s, k2, &v2);
			int f = s->load();
			int ok = v2 == -2 ? (f == ST_A || f == ST_B) : f >= 0 && (f == v2 || f == s->next(v2));
			CHECK(ok, "%s%s: stop before step %ld of the save, start again, stop before step %ld of the next: "
			      "found %s", s->name, empty ? " (empty file)" : "", k1, k2, st_name(f));
			if (!ok)
				lost2++;
			if (done2)
				break;
		}
		if (done1)
			break;
	}
	printf("%-9s %-11s %2ld stop points: lost at the next start %d, lost after a second stop %d, "
	       "main file missing right after the stop %d%s\n", s->name, empty ? "(empty)" : "(half)", k1, lost,
	       lost2, gone, fails > f0 ? "  <- FAIL" : "");
}

// --- trainer.txt: source/wbx_trainer.c itself ----------------------------------------------------------------------
static char trainer_path[128];
static wbx_trainer tr[3];

static void trainer_setup(void) { wbx_trainer_save(&tr[0], trainer_path); }
static int trainer_save(int nth) { return wbx_trainer_save(&tr[nth == 1 ? 1 : 2], trainer_path); }
static int trainer_load(void) // main.c ensure_trainer
{
	wbx_trainer t;
#ifdef OLD_CODE
	if (wbx_trainer_load(&t, trainer_path) != 0)
		return -1;
#else
	if (wbx_trainer_load_any(&t, trainer_path) < 0)
		return -1;
#endif
	for (int i = 0; i < 3; i++)
		if (!memcmp(&t, &tr[i], sizeof(t)))
			return i == 2 ? ST_C : i;
	return -1;
}

// --- watts.txt: a copy of main.c watts_read / watts_add --------------------------------------------------------------
static char watts_path[128], watts_tmp[140];
#define WATTS_PATH watts_path
#define WATTS_GAMES 32
#define WATTS_HEAD "# PocketPKMN: watts collected per game (title id, watts)"

static int watts_read(unsigned long long *tids, unsigned long *totals)
{
	int n = 0;
#ifdef OLD_CODE
	FILE *f = fopen(WATTS_PATH, "r");
#else
	char path[600];
	sf_pick(WATTS_PATH, sf_text_valid, (void *)WATTS_HEAD, path, sizeof(path));
	FILE *f = fopen(path, "r");
#endif
	if (!f)
		return 0;
	char line[96];
	while (n < WATTS_GAMES && fgets(line, sizeof(line), f)) {
		unsigned long long t;
		unsigned long w;
		if (sscanf(line, "%llx %lu", &t, &w) == 2) {
			tids[n] = t;
			totals[n++] = w;
		}
	}
	fclose(f);
	return n;
}

static int watts_add(unsigned long long tid, unsigned watts)
{
	unsigned long long tids[WATTS_GAMES];
	unsigned long totals[WATTS_GAMES];
	int n = watts_read(tids, totals), at = -1;
	for (int i = 0; i < n; i++)
		if (tids[i] == tid)
			at = i;
	if (at < 0) {
		if (n >= WATTS_GAMES)
			return -1;
		at = n++;
		tids[at] = tid;
		totals[at] = 0;
	}
	totals[at] += watts;
#ifdef OLD_CODE
	FILE *f = fopen(watts_tmp, "w"); // = WATTS_PATH ".tmp"
	if (!f)
		return -1;
	fprintf(f, "# PocketPKMN: watts collected per game (title id, watts)\n");
	for (int i = 0; i < n; i++)
		fprintf(f, "%016llx %lu\n", tids[i], totals[i]);
	int bad = fclose(f) != 0;
	if (bad)
		return -1;
	remove(WATTS_PATH);
	return rename(watts_tmp, WATTS_PATH) == 0 ? 0 : -1;
#else
	char text[128 + WATTS_GAMES * 40];
	int len = snprintf(text, sizeof(text), WATTS_HEAD "\n");
	for (int i = 0; i < n; i++)
		len += snprintf(text + len, sizeof(text) - len, "%016llx %lu\n", tids[i], totals[i]);
	if (len >= (int)sizeof(text))
		return -1;
	return sf_write(WATTS_PATH, text, (size_t)len);
#endif
}
#undef WATTS_PATH

#define G1 0x0004000000aaa000ULL
#define G2 0x0004800d01234567ULL

static void watts_setup(void)
{
	FILE *f = fopen(watts_path, "w");
	fprintf(f, "# PocketPKMN: watts collected per game (title id, watts)\n%016llx %lu\n%016llx %lu\n", G1, 100UL, G2,
	        200UL);
	fclose(f);
}
static int watts_save(int nth) { return nth == 1 ? watts_add(G1, 50) : watts_add(G2, 7); }
static int watts_load(void)
{
	unsigned long long t[WATTS_GAMES];
	unsigned long w[WATTS_GAMES];
	int n = watts_read(t, w);
	if (n != 2 || t[0] != G1 || t[1] != G2)
		return -1;
	static const unsigned long want[4][2] = {{100, 200}, {150, 200}, {100, 207}, {150, 207}};
	for (int i = 0; i < 4; i++)
		if (w[0] == want[i][0] && w[1] == want[i][1])
			return i;
	return -1;
}
static int watts_next(int from) { return from == ST_A ? ST_C : ST_D; }

// --- settings.txt: a copy of main.c settings_save / settings_load ----------------------------------------------------
static char settings_path[128];
#define SETTINGS_PATH settings_path
#define SETTINGS_HEAD "# PocketPKMN settings"
static int bg_now, music_now, sounds_now; // what ui_background() / sound_music_on() / sound_effects_on() give
static int bg, set_music, set_sounds;     // what settings_load set

static void settings_save(void)
{
#ifdef OLD_CODE
	FILE *f = fopen(SETTINGS_PATH, "w");
	if (!f)
		return;
	fprintf(f, "# PocketPKMN settings\nbackground=%d\nmusic=%d\nsounds=%d\n", bg_now, music_now, sounds_now);
	fclose(f);
#else
	char text[160];
	int len = snprintf(text, sizeof(text), SETTINGS_HEAD "\nbackground=%d\nmusic=%d\nsounds=%d\n", bg_now, music_now,
	                   sounds_now);
	if (len > 0 && len < (int)sizeof(text))
		sf_write(SETTINGS_PATH, text, (size_t)len);
#endif
}

static void settings_load(void)
{
#ifdef OLD_CODE
	FILE *f = fopen(SETTINGS_PATH, "r");
#else
	char path[600];
	sf_pick(SETTINGS_PATH, sf_text_valid, (void *)SETTINGS_HEAD, path, sizeof(path));
	FILE *f = fopen(path, "r");
#endif
	if (!f)
		return;
	char line[96];
	int v;
	while (fgets(line, sizeof(line), f)) {
		if (sscanf(line, "background=%d", &v) == 1)
			bg = v;
		if (sscanf(line, "music=%d", &v) == 1)
			set_music = v != 0;
		if (sscanf(line, "sounds=%d", &v) == 1)
			set_sounds = v != 0;
	}
	fclose(f);
}
#undef SETTINGS_PATH

static const int set_val[3][3] = {{3, 0, 0}, {5, 0, 1}, {2, 1, 0}}; // A, B, C (the defaults are 0, 1, 1)
static void settings_put(int i)
{
	bg_now = set_val[i][0];
	music_now = set_val[i][1];
	sounds_now = set_val[i][2];
	settings_save();
}
static void settings_setup(void) { settings_put(0); }
static int settings_save_n(int nth)
{
	settings_put(nth == 1 ? 1 : 2);
	return 0;
}
static int settings_load_st(void)
{
	bg = 0, set_music = 1, set_sounds = 1; // the app's defaults
	settings_load();
	for (int i = 0; i < 3; i++)
		if (bg == set_val[i][0] && set_music == set_val[i][1] && set_sounds == set_val[i][2])
			return i == 2 ? ST_C : i;
	return -1;
}

// --- a TWiLight Menu .sav: copies of storage.cpp dsFileRead, dsWrite (SD file) and scanSdForDsSaves ---------------
#define DS_SAVE_SIZE 0x80000
static char sav_path[128];
static unsigned char ds_whole[DS_SAVE_SIZE + 0x10000], ds_data[3][DS_SAVE_SIZE];
static long ds_len;

static int ds_file_read(const char *path) // dsFileRead (into ds_whole)
{
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return 0;
	fseek(fp, 0, SEEK_END);
	long n = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	int ok = n >= (long)DS_SAVE_SIZE && n <= (long)DS_SAVE_SIZE + 0x10000;
	if (ok) {
		ds_len = n;
		ok = fread(ds_whole, 1, (size_t)n, fp) == (size_t)n;
	}
	fclose(fp);
	return ok;
}

static int ds_write(const unsigned char *data) // dsWrite, SD file branch (the read-back after it only reads)
{
	if (!ds_file_read(sav_path))
		return 0;
	memcpy(ds_whole, data, DS_SAVE_SIZE);
#ifdef OLD_CODE
	char tmp[140];
	snprintf(tmp, sizeof(tmp), "%s.tmp", sav_path);
	FILE *fp = fopen(tmp, "wb");
	int ok = fp && fwrite(ds_whole, 1, (size_t)ds_len, fp) == (size_t)ds_len;
	if (fp)
		ok = (fclose(fp) == 0) && ok;
	if (!ok || remove(sav_path) != 0 || rename(tmp, sav_path) != 0)
		return 0;
#else
	if (sf_write(sav_path, ds_whole, (size_t)ds_len) != 0)
		return 0;
#endif
	return 1;
}

static int ds_sav_valid(const char *path, void *ctx) // dsSavValid
{
	(void)ctx;
	struct stat st;
	return stat(path, &st) == 0 && st.st_size >= (off_t)DS_SAVE_SIZE && st.st_size <= (off_t)DS_SAVE_SIZE + 0x10000;
}

static int ends(const char *s, const char *end)
{
	size_t a = strlen(s), b = strlen(end);
	return a > b && !strcasecmp(s + a - b, end);
}

static int ds_scan_load(void) // scanSdForDsSaves on the folder, then dsFileRead of the save it lists
{
	char found[4][160], cut[4][160];
	int nf = 0, nc = 0;
	DIR *d = opendir(dir);
	struct dirent *e;
	while (d && (e = readdir(d))) {
		char p[400];
		snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
		if (ends(e->d_name, ".sav")) {
			if (ds_sav_valid(p, NULL) && nf < 4)
				snprintf(found[nf++], 160, "%s", p);
		}
#ifndef OLD_CODE
		else if ((ends(e->d_name, ".sav.tmp") || ends(e->d_name, ".sav.bak")) && nc < 4) {
			snprintf(cut[nc], 160, "%s", p);
			cut[nc++][strlen(p) - 4] = 0;
		}
#endif
	}
	if (d)
		closedir(d);
#ifndef OLD_CODE
	for (int i = 0; i < nc; i++) {
		if (exists(cut[i]))
			continue;
		char use[600];
		int from = sf_pick(cut[i], ds_sav_valid, NULL, use, sizeof(use));
		if (from > 0 && !strcmp(use, cut[i]) && nf < 4)
			snprintf(found[nf++], 160, "%s", cut[i]);
	}
#endif
	(void)cut;
	(void)nc;
	for (int i = 0; i < nf; i++)
		if (!strcmp(found[i], sav_path)) {
			if (!ds_file_read(sav_path) || ds_len != DS_SAVE_SIZE + 0x10)
				return -1;
			for (int k = 0; k < 3; k++)
				if (!memcmp(ds_whole, ds_data[k], DS_SAVE_SIZE))
					return k == 2 ? ST_C : k;
			return -1;
		}
	return -1; // TWiLight Menu finds no save: the game starts a new one
}

static void ds_setup(void)
{
	FILE *f = fopen(sav_path, "wb");
	fwrite(ds_data[0], 1, DS_SAVE_SIZE, f);
	fwrite("TWiLight extra16", 1, 0x10, f); // bytes after the save (kept as they are)
	fclose(f);
}
static int ds_save(int nth) { return ds_write(ds_data[nth == 1 ? 1 : 2]); }

static uint32_t seed = 12345;
static uint32_t rnd(void) { return seed = seed * 1103515245u + 12345u; }

int main(void)
{
	snprintf(dir, sizeof(dir), "/tmp/pocketpkmn_files_XXXXXX");
	if (!mkdtemp(dir)) {
		printf("no temp folder\n");
		return 1;
	}
	snprintf(trainer_path, sizeof(trainer_path), "%s/trainer.txt", dir);
	snprintf(watts_path, sizeof(watts_path), "%s/watts.txt", dir);
	snprintf(watts_tmp, sizeof(watts_tmp), "%s.tmp", watts_path);
	snprintf(settings_path, sizeof(settings_path), "%s/settings.txt", dir);
	snprintf(sav_path, sizeof(sav_path), "%s/POKEMON P.sav", dir);
	wbx_trainer_new(&tr[0], "Old", rnd);
	wbx_trainer_new(&tr[1], "New", rnd);
	wbx_trainer_new(&tr[2], "Third", rnd);
	for (int k = 0; k < 3; k++)
		for (int i = 0; i < DS_SAVE_SIZE; i++)
			ds_data[k][i] = (unsigned char)(rnd() >> 16);

	static const scen all[] = {
		{"trainer", trainer_path, trainer_setup, trainer_save, trainer_load, next_c},
		{"watts", watts_path, watts_setup, watts_save, watts_load, watts_next},
		{"settings", settings_path, settings_setup, settings_save_n, settings_load_st, next_c},
		{"DS .sav", sav_path, ds_setup, ds_save, ds_scan_load, next_c},
	};
	printf("test_files (%s code):\n",
#ifdef OLD_CODE
	       "OLD"
#else
	       "fixed"
#endif
	);
	for (int i = 0; i < 4; i++)
		for (int empty = 0; empty < 2; empty++)
			run(&all[i], empty);

#ifndef OLD_CODE
	// a save with no stop leaves only the file itself
	clean();
	ds_setup();
	CHECK(ds_save(1) == 1, "DS save");
	char p[160];
	snprintf(p, sizeof(p), "%s.tmp", sav_path);
	CHECK(!exists(p), "a .sav.tmp is left");
	snprintf(p, sizeof(p), "%s.bak", sav_path);
	CHECK(!exists(p), "a .sav.bak is left");
	// a copy left by an older version (old code stopped after its remove: only .sav.tmp, complete) is put back
	clean();
	ds_setup();
	snprintf(p, sizeof(p), "%s.tmp", sav_path);
	__real_rename(sav_path, p);
	CHECK(ds_scan_load() == ST_A && exists(sav_path) && !exists(p), "an old version's .sav.tmp not put back");
	clean();
	trainer_setup();
	snprintf(p, sizeof(p), "%s.tmp", trainer_path);
	__real_rename(trainer_path, p);
	CHECK(trainer_load() == ST_A && exists(trainer_path), "an old version's trainer.txt.tmp not put back");
	clean();
	watts_setup();
	snprintf(p, sizeof(p), "%s.tmp", watts_path);
	__real_rename(watts_path, p);
	CHECK(watts_load() == ST_A && exists(watts_path), "an old version's watts.txt.tmp not put back");
	// a cut-off .tmp (no newline at the end / wrong first line) is not used
	clean();
	FILE *f = fopen(p, "w");
	fprintf(f, "# PocketPKMN: watts collected per game (title id, watts)\n%016llx 1", G1);
	fclose(f);
	CHECK(watts_load() == -1 && !exists(watts_path), "a cut-off watts.txt.tmp used");
#endif
	clean();
	rmdir(dir);
	printf("%d of %d checks failed\n", fails, checks);
	printf(fails ? "FAIL\n" : "all passed\n");
	return fails ? 1 : 0;
}
