// v0.81 (review finding 5 / E, owner: "Go 12" (all three)): putting a whole file on the SD card so that a stop (power
// cut, crash) at any moment leaves a complete copy - the old one or the new one, never neither. The pattern of
// walk_pending.c wp_save, for files without their own check value (trainer.txt, watts.txt, settings.txt, the
// TWiLight Menu .sav of a DS game). Plain C with stdio only: tests/test_files.c runs it on the PC.
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Writes len bytes as base: base.tmp first, read back and compared; then base -> base.bak (a rename onto an
// existing file fails on the 3DS card), base.tmp -> base, base read back and compared, base.bak removed.
// The content is exactly data. Returns 0, or -1: the old file is still base (or, if it could not be put back,
// base.bak with the complete new one in base.tmp - sf_pick finds them).
int sf_write(const char *base, const void *data, size_t len);

// 1 = this copy is complete (the file's owner decides how to tell).
typedef int (*sf_valid_fn)(const char *path, void *ctx);

// The copy of base to read, into path. 0 = base (valid). 1 = base.tmp, 2 = base.bak: base is missing or not valid
// and that copy is (the first valid one, newest first); when base is missing that copy is first renamed back to
// base (then path = base). -1 = no valid copy (path = base, read as before).
int sf_pick(const char *base, sf_valid_fn valid, void *ctx, char *path, size_t n);

// sf_valid_fn for the app's text files: the first line is ctx (a const char *, without its newline) and the file
// ends with a newline.
int sf_text_valid(const char *path, void *ctx);

#ifdef __cplusplus
}
#endif
