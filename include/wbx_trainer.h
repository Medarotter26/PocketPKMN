// wbx_trainer.h - the WalkerBox trainer: the owner WalkerBox gives every walker it uses (decided 2026-11-15).
// Made on first launch (name chosen by the user, random TID/SID and save id), kept in
// sdmc:/3ds/pocketpkmn/trainer.txt. Portable C (stdio) so the PC tests can use it.
#ifndef WBX_TRAINER_H
#define WBX_TRAINER_H

#include <stddef.h>
#include <stdint.h>

#include "app_paths.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WBX_TRAINER_PATH
#define WBX_TRAINER_PATH APP_DIR "/trainer.txt"
#endif
#define WBX_TRAINER_NAME_MAX 7 // like the games

typedef struct wbx_trainer {
	char name[WBX_TRAINER_NAME_MAX + 1]; // ASCII letters, digits, space
	uint16_t tid, sid;
	uint8_t save_id[0x28]; // what a game puts at identity +0x10 (its own 40 random bytes)
} wbx_trainer;

// 0 = loaded, -1 = no file or not valid.
int wbx_trainer_load(wbx_trainer *t, const char *path);
int wbx_trainer_save(const wbx_trainer *t, const char *path);

// v0.81 (review finding 5 / E, owner: "Go 12" (all three)): as wbx_trainer_load, but when path is missing or not valid
// path.tmp / path.bak is used (a save cut off by a stop; a missing path gets that copy back). 0 = path itself,
// 1 = .tmp, 2 = .bak, -1 = no valid trainer.
int wbx_trainer_load_any(wbx_trainer *t, const char *path);

// Keeps what the walker text can show (letters, digits, space), max 7; "" if nothing is left.
void wbx_trainer_clean_name(const char *in, char out[WBX_TRAINER_NAME_MAX + 1]);

// A new trainer with this name; random comes from the caller (two u32 per call are enough).
void wbx_trainer_new(wbx_trainer *t, const char *name, uint32_t (*rnd)(void));

// The identity WalkerBox sends with 52 / 32: the walker's own identity (base, from its 22 reply) with the
// trainer's TID (+0x0C), SID (+0x0E) and name (+0x48, Gen 4 text: name, 0xFFFF, zeros) put in. Everything
// else (versions, unique id, flags, steps) stays as the walker has it.
void wbx_trainer_identity(const wbx_trainer *t, const uint8_t base[0x68], uint8_t out[0x68]);

// Does this identity (a 22 reply) belong to the trainer (same TID and SID)?
int wbx_trainer_owns(const wbx_trainer *t, const uint8_t identity[0x68]);

#ifdef __cplusplus
}
#endif

#endif
