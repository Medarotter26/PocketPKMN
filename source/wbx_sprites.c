// wbx_sprites.c - loads walker sprites from the app's romfs (SD pack as fallback); own placeholder when missing.
// Portable C (stdio only) so the PC tests can use it too; the 3DS-only part is the romfs mount below.
#include "wbx_sprites.h"

#include <stdio.h>
#include <string.h>

#include "pw_image.h"

#ifdef __3DS__
#include <3ds.h>
#include <sys/iosupport.h>

// Sprites can be read before storage.cpp's prepareData() has called romfsInit() (sprite viewer, Test Path), so
// the device is found here. If "romfs:" is already mounted it is used; otherwise the app's own romfs is mounted a
// second time under its own name "wbxrom:" - never as "romfs:", so prepareData()'s later romfsInit() is not
// disturbed (a second mount under the same name would replace the device and leak the first). Not cached on
// failure, so a later call tries again.
static const char *rom_device(void)
{
	static const char *dev;
	if (dev)
		return dev;
	if (FindDevice("romfs:") >= 0)
		return dev = "romfs";
	if (R_SUCCEEDED(romfsMountSelf("wbxrom")))
		return dev = "wbxrom";
	return "romfs";
}
#endif

// dir with a leading "romfs:" switched to the device rom_device() picked (3DS only; elsewhere dir as it is).
static const char *on_device(const char *dir, char *buf, size_t n)
{
#ifdef __3DS__
	if (strncmp(dir, "romfs:", 6) == 0) {
		snprintf(buf, n, "%s:%s", rom_device(), dir + 6);
		return buf;
	}
#else
	(void)buf;
	(void)n;
#endif
	return dir;
}

// A box outline with a "?" in the middle, drawn with WalkerBox's own font. Frame 2 is frame 1 moved up
// one pixel (the walker alternates the two frames).
static void placeholder_image(int w, int h, uint8_t *frame1, uint8_t *frame2)
{
	uint8_t text[64 * 48], shades[64 * 48];
	uint8_t tmp[0x300];
	const int th = 16;
	int tw = pw_text_width("?") + 2;
	if (tw > w)
		tw = w;
	pw_text_image("?", tw, th, tmp);
	pw_img_decode(tmp, tw, th, text);
	for (int f = 0; f < 2; f++) {
		int lift = f; // frame 2 one pixel higher
		memset(shades, PW_SHADE_WHITE, (size_t)(w * h));
		int x0 = 2, y0 = 2 - lift, x1 = w - 3, y1 = h - 3 - lift;
		for (int x = x0; x <= x1; x++) {
			shades[y0 * w + x] = PW_SHADE_DARK;
			shades[y1 * w + x] = PW_SHADE_DARK;
		}
		for (int y = y0; y <= y1; y++) {
			shades[y * w + x0] = PW_SHADE_DARK;
			shades[y * w + x1] = PW_SHADE_DARK;
		}
		int ox = (w - tw) / 2, oy = (h - th) / 2 - lift;
		for (int y = 0; y < th; y++)
			for (int x = 0; x < tw; x++)
				if (text[y * tw + x])
					shades[(oy + y) * w + ox + x] = PW_SHADE_BLACK;
		pw_img_encode(shades, w, h, f ? frame2 : frame1);
	}
}

void wbx_sprite_placeholder(uint8_t out[WBX_SPRITE_SIZE])
{
	placeholder_image(32, 24, out, out + 0xC0);
	placeholder_image(64, 48, out + WBX_SPRITE_SMALL_SIZE, out + WBX_SPRITE_SMALL_SIZE + 0x300);
}

static int load_file(const char *path, uint8_t out[WBX_SPRITE_SIZE])
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	size_t n = fread(out, 1, WBX_SPRITE_SIZE, f);
	int extra = fgetc(f);
	fclose(f);
	return n == WBX_SPRITE_SIZE && extra == EOF;
}

int wbx_sprite_load(int dex, uint8_t out[WBX_SPRITE_SIZE])
{
	if (dex >= 1 && dex <= WBX_SPRITE_LAST) {
		char dir[128], path[256];
		snprintf(path, sizeof path, "%s/%03d.bin", on_device(WBX_SPRITE_DIR, dir, sizeof dir), dex);
		if (load_file(path, out))
			return 1;
		snprintf(path, sizeof path, WBX_SPRITE_SD_DIR "/%03d.bin", dex);
		if (load_file(path, out))
			return 1;
	}
	wbx_sprite_placeholder(out);
	return 0;
}

int wbx_sprite_load_for(int dex, int form, int gen, uint8_t out[WBX_SPRITE_SIZE])
{
	return wbx_sprite_load_for_g(dex, form, gen, 0, out);
}

int wbx_sprite_load_for_g(int dex, int form, int gen, int female, uint8_t out[WBX_SPRITE_SIZE])
{
	// v0.90 (2026-11-17, user: "Gen 5 forms should show"): the forms folder for Gen 5-7 games (was Gen 7 only).
	// Female first (Gen 6+ games; only Pyroar has a NNN_female.bin), then NNN_fF.bin for the game's form number,
	// form 0 too (only Vivillon Icy Snow and Xerneas Neutral have an _f0 file: the pack shows Meadow / Active).
	// No file -> the pack, as before.
	if (gen >= 5 && dex >= 1 && dex <= WBX_SPRITE_LAST) {
		char dir[128], path[256];
		const char *d = on_device(WBX_SPRITE_FORMS_DIR, dir, sizeof dir);
		if (female && gen >= 6) {
			snprintf(path, sizeof path, "%s/%03d_female.bin", d, dex);
			if (load_file(path, out))
				return 1;
		}
		if (form >= 0 && form < 32) {
			snprintf(path, sizeof path, "%s/%03d_f%d.bin", d, dex, form);
			if (load_file(path, out))
				return 1;
		}
	}
	if ((gen == 1 || gen == 2) && dex >= 1 && dex <= WBX_SPRITE12_LAST) {
		char dir[128], path[256];
		const char *d = on_device(WBX_SPRITE12_DIR, dir, sizeof dir);
		if (dex == 201 && form >= 1 && form <= 25) // Unown B-Z
			snprintf(path, sizeof path, "%s/201%c.bin", d, 'A' + form);
		else
			snprintf(path, sizeof path, "%s/%03d.bin", d, dex);
		if (load_file(path, out)) {
			// v0.45 (user: "lets ditch our custom gen 2 mini sprites. Lets just use their standard pokewalker gen 4
			// ones"): the small 32x24 pictures come from the pack (HGSS), only the large ones from the Gen 1/2 set.
			static uint8_t pack[WBX_SPRITE_SIZE];
			if (wbx_sprite_load(dex, pack))
				memcpy(out, pack, WBX_SPRITE_SMALL_SIZE);
			return 1;
		}
	}
	if (gen == 3 && dex >= 1 && dex <= WBX_SPRITE3_LAST) { // v0.88: Gen 3 games only
		char dir[128], path[256];
		const char *d = on_device(WBX_SPRITE3_DIR, dir, sizeof dir);
		if (dex == 201 && form >= 1 && form <= 25) // Unown B-Z
			snprintf(path, sizeof path, "%s/201%c.bin", d, 'A' + form);
		else if (dex == 201 && form == 26) // Unown !
			snprintf(path, sizeof path, "%s/201EM.bin", d);
		else if (dex == 201 && form == 27) // Unown ?
			snprintf(path, sizeof path, "%s/201QM.bin", d);
		else // Deoxys: Normal only (386.bin)
			snprintf(path, sizeof path, "%s/%03d.bin", d, dex);
		if (load_file(path, out)) {
			// as Gen 1/2: the small 32x24 pictures from the pack, only the large ones from the Gen 3 set
			static uint8_t pack[WBX_SPRITE_SIZE];
			if (wbx_sprite_load(dex, pack))
				memcpy(out, pack, WBX_SPRITE_SMALL_SIZE);
			return 1;
		}
	}
	return wbx_sprite_load(dex, out);
}
