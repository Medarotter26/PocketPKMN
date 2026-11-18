// routes.h - the route files (romfs/routes/*.txt, made by tools/routes_xy.py / routes_gsc.py) and the walker route
// built from one course. Portable C (stdio) so the PC tests can use it.
#ifndef ROUTES_H
#define ROUTES_H

#include <stdint.h>

#include "walk_compose.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ROUTES_PC_DIR
#define ROUTES_DIR ROUTES_PC_DIR // PC tests only
#else
#define ROUTES_DIR "romfs:/routes"
#endif

#define ROUTES_MAX_COURSES 48 // v0.97: Ruby / Sapphire / Emerald have 43 courses (was 24)

typedef struct route_poke_def {
	char group;      // 'A' filler .. 'C' prize
	int slot;        // walker slot: 0 = C (rarest), 1 = B, 2 = A
	int chance, species, level, steps;
	uint16_t moves[4];
	char name[24];   // from the file's comment
	int form;        // Gen 7 files: "form=N" (Alolan forms, Blue-Striped Basculin); 0 otherwise
} route_poke_def;

typedef struct route_item_def {
	int rank, chance, item, steps;
	char version[12]; // "all" or a game version ("x", "y", ...)
	char name[32];
} route_item_def;

typedef struct route_course {
	int number;
	char name[32], unlock[32], picture[40], favoured[48];
	char desc[160]; // optional "desc=" (v0.46: room for the user's route descriptions; "" = none yet)
	route_poke_def poke[6];
	int npoke;
	route_item_def item[20];
	int nitem;
} route_course;

typedef struct route_file {
	char file[16];   // "gsc", "xy"
	int item_gen;    // 2 = Gen 2 item ids, 0 = the usual (Gen 4+) ids
	route_course course[ROUTES_MAX_COURSES];
	int ncourse;
	int rejected;    // audit M7: lines that could not be used (bad numbers, too many Pokemon / items / courses)
} route_file;

// Which route file a game uses: "xy" for Pokemon X / Y (version "x" / "y"), "gsc" for Gold / Silver / Crystal
// (any Gen 2 save), "sm" for Sun / Moon / Ultra Sun / Ultra Moon ("sn" / "mn" / "us" / "um"), "oras" for Omega Ruby /
// Alpha Sapphire ("or" / "as", v0.93), "dppt" for Diamond / Pearl / Platinum, "frlg" for FireRed / LeafGreen ("fr" /
// "lg", v0.94, see routes_note_gen3). NULL = no routes
// yet ("Not yet compatible").
const char *routes_file_for(unsigned long long title_id, int gen, const char **version);
// v0.94: the games scan notes which Gen 3 titles are FireRed ("fr") / LeafGreen ("lg") - their saves look alike and
// (v0.97) Ruby "r" / Sapphire "s" / Emerald "e" -> "rse";
// a GBA inject's title ID is its own; NULL = not FireRed / LeafGreen. routes_file_for(tid, 3, ...) then gives "frlg".
void routes_note_gen3(unsigned long long title_id, const char *version);

// Loads ROUTES_DIR/<name>.txt. Returns the course count, or -1.
int routes_load(const char *name, route_file *out);

// The walker route for one course: one of the two Pokemon of each group (seed picks), the items for this
// version, the course picture (ROUTES_DIR/pictures/<picture>.txt, a blank picture if missing), names as walker
// pictures, sprites by generation (gen 2 courses -> the Gen 1/2 set). Returns 0, or -1 if the course is incomplete.
int routes_build(const route_course *c, const char *version, int gen, unsigned seed, wbx_route *out);

// The course picture (ROUTES_DIR/pictures/<slug>.txt) as walker image data (32x24, 2-bit); blank if missing.
void routes_picture(const char *slug, uint8_t out[0xC0]);

// "start", "badges=4", "trials=3", "hof", "hof watts=10000" -> "From the start", "After badge 4",
// "After Lana's trial", ...
void routes_unlock_text(const char *unlock, char *out, int size);
// v0.52: 1 if the course is open for this progress ("start", "badges=N", "trials=N", "hof", "hof watts=N"; unknown =
// open). For Gen 7 games "badges" is the number of island trials done (storage_game_progress counts the trial
// Z-Crystals in the bag).
int routes_unlocked(const char *unlock, int badges, int hof, unsigned watts);

#ifdef __cplusplus
}
#endif

#endif
