// wbx_sprites.h - walker sprite pack, shipped inside the app's romfs (romfs:/sprites/pack/NNN.bin; user decision
// 2026-11-15: every graphic ships with the CIA). Falls back to the old SD pack (sdmc:/3ds/pocketpkmn/sprites/)
// for a file the romfs lacks. romfs/sprites/pack is a byte-identical copy of sd_out/3ds/walkerbox/sprites
// (tools/sync_romfs_sprites.sh).
//
// One file per national dex number, 0x780 bytes: small 32x24 frame 1 + frame 2 (0x180), large 64x48
// frame 1 + frame 2 (0x600) - the bytes the walker holds at EEPROM 0x91BE-0x993D. Written on the user's PC
// by tools/walker_assets.py (#1-493 from their own HGSS ROM, #494-807 from WalkerBox's conversions).
#ifndef WBX_SPRITES_H
#define WBX_SPRITES_H

#include <stdint.h>

#include "app_paths.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WBX_SPRITE_SMALL_SIZE 0x180
#define WBX_SPRITE_LARGE_SIZE 0x600
#define WBX_SPRITE_SIZE       (WBX_SPRITE_SMALL_SIZE + WBX_SPRITE_LARGE_SIZE)
#define WBX_SPRITE_LAST       807
#ifdef WBX_SPRITE_PC_DIR
#define WBX_SPRITE_DIR        WBX_SPRITE_PC_DIR   // PC tests only (tests/run_sprites.sh)
#else
#define WBX_SPRITE_DIR        "romfs:/sprites/pack"
#endif
// Fallback for a file missing from WBX_SPRITE_DIR: the SD pack older versions read.
#ifdef WBX_SPRITE_SD_PC_DIR
#define WBX_SPRITE_SD_DIR     WBX_SPRITE_SD_PC_DIR // PC tests only
#else
#define WBX_SPRITE_SD_DIR     APP_DIR "/sprites"
#endif

// Fills out with the pack's sprite for dex (1..807): WBX_SPRITE_DIR, else WBX_SPRITE_SD_DIR. Returns 1 if a file
// was there and the right size, 0 if WalkerBox's own placeholder was used instead (missing / damaged file, dex out
// of range). On the 3DS the romfs is mounted here if nothing has mounted it yet (see wbx_sprites.c).
int wbx_sprite_load(int dex, uint8_t out[WBX_SPRITE_SIZE]);

// Gen 1/2 set (user 2026-11-15: "The plan is for gens 1/2/3 to use their sprites"): the Crystal-based LARGE sprites
// (art_src/gen12/out, built into the app's romfs). Unown letters B-Z are 201B.bin .. 201Z.bin. Since v0.45 the
// SMALL pictures are the pack's (HGSS) ones (user: "ditch our custom gen 2 mini sprites"); the files' own small
// part is no longer used.
#ifdef WBX_SPRITE12_PC_DIR
#define WBX_SPRITE12_DIR      WBX_SPRITE12_PC_DIR // PC tests only
#else
#define WBX_SPRITE12_DIR      "romfs:/sprites/gen12"
#endif
#define WBX_SPRITE12_LAST     251
// Gen 3 set (v0.88, user: "we are NOT adjusting the gen 3 sprite set for games in gens 4-7"): LARGE pictures for
// Pokemon from Gen 3 games only (Ruby/Sapphire/Emerald/FireRed/LeafGreen injects), art_src/gen3/out copied as it is.
// Unown letters: 201.bin (A), 201B.bin .. 201Z.bin, 201EM.bin (!), 201QM.bin (?). The SMALL pictures are the pack's,
// as for Gen 1/2 (user: Gens 1-4 all use the default Pokewalker small sprites); the files' own small part is unused.
#ifdef WBX_SPRITE3_PC_DIR
#define WBX_SPRITE3_DIR       WBX_SPRITE3_PC_DIR // PC tests only
#else
#define WBX_SPRITE3_DIR       "romfs:/sprites/gen3"
#endif
#define WBX_SPRITE3_LAST      386
// Form sprites (Gen 5-7 games since v0.90): romfs/sprites/forms/NNN_fF.bin (F = the form number, e.g. 019_f1.bin = Alolan
// Rattata), same 0x780-byte layout as the pack. A missing form file falls back to the species' pack sprite.
#ifdef WBX_SPRITE_FORMS_PC_DIR
#define WBX_SPRITE_FORMS_DIR  WBX_SPRITE_FORMS_PC_DIR // PC tests only
#else
#define WBX_SPRITE_FORMS_DIR  "romfs:/sprites/forms"
#endif

// The sprite for a Pokemon from a game of generation gen: Gen 1/2 games -> the Gen 1/2 set (form = Unown's
// letter 0-25), Gen 3 games -> the Gen 3 set (form = Unown's letter 0-27: 0 A ... 25 Z, 26 !, 27 ?), Gen 5-7 games
// (v0.90; Gen 7 only before) -> WBX_SPRITE_FORMS_DIR/NNN_fF.bin for the game's form F (form 0 too: NNN_f0.bin),
// otherwise (and if that file is missing) the pack as wbx_sprite_load. Gen 4 games never use the forms folder.
// Returns 1 if a file was used, 0 for the placeholder. Same as wbx_sprite_load_for_g with female = 0.
int wbx_sprite_load_for(int dex, int form, int gen, uint8_t out[WBX_SPRITE_SIZE]);
// v0.90: as wbx_sprite_load_for, and for a female (female != 0) from a Gen 6+ game WBX_SPRITE_FORMS_DIR/NNN_female.bin
// first (only Pyroar has one). Male / genderless: exactly wbx_sprite_load_for's result.
int wbx_sprite_load_for_g(int dex, int form, int gen, int female, uint8_t out[WBX_SPRITE_SIZE]);

// WalkerBox's own placeholder sprite (no game art): a "?" in a box, both sizes, both frames.
void wbx_sprite_placeholder(uint8_t out[WBX_SPRITE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
