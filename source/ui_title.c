// ui_title.c - a title's HOME menu name and icon, read from its SMDH ("icon" file of the title's content).
// Read only, never during IR (callers load titles when a screen opens).
#include <stdio.h>
#include <string.h>

#include "dbg.h"
#include "ui.h"

#define SMDH_SIZE       0x36C0
#define SMDH_TITLES     0x0008 // 16 x { short name 0x80, long name 0x100, publisher 0x80 }
#define SMDH_LANG_EN    1
#define SMDH_LARGE_ICON 0x24C0 // 48x48 RGB565 in 8x8 tiles

static int read_smdh(u64 tid, FS_MediaType media, u8 *out)
{
	u32 arch[4] = { (u32)tid, (u32)(tid >> 32), (u32)media, 0 };
	static const u32 file[5] = { 0, 0, 2, 0x6E6F6369 /* "icon" */, 0 };
	FS_Path ap = { PATH_BINARY, sizeof(arch), arch };
	FS_Path fp = { PATH_BINARY, sizeof(file), file };
	Handle h;
	if (R_FAILED(FSUSER_OpenFileDirectly(&h, ARCHIVE_SAVEDATA_AND_CONTENT, ap, fp, FS_OPEN_READ, 0)))
		return 0;
	u32 got = 0;
	Result r = FSFILE_Read(h, &got, 0, out, SMDH_SIZE);
	FSFILE_Close(h);
	return R_SUCCEEDED(r) && got == SMDH_SIZE && memcmp(out, "SMDH", 4) == 0;
}

// v0.63 (user: Platinum "showed up, just its picture icon was missing"): DS games have no SMDH. PocketPKMN's made-up
// DS title IDs are 0x0004800D << 32 | game << 24 | 24-bit path hash (0 = the game card; storage.cpp).
//   game card: the card's own DS icon (banner: 32x32 4-bit tiles at 0x20, 16 BGR555 colours at 0x220, colour 0 =
//              transparent), scaled to 48x48 on a white tile like the HOME menu shows DS cards;
//   SD card (TWiLight Menu): a plain cartridge drawn here, its label in the game's colour - so the two never look
//              alike. Nothing of the game's own art is stored in the app.
#define DS_BANNER_SIZE 0x23C0

static u16 rgb565(int r, int g, int b) { return (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }

static int ds_title_load(ui_title *t, u64 title_id)
{
	static u16 px[48 * 48];
	int game = (int)((title_id >> 24) & 0xFF);
	static const char *names[] = { "DS game", "Pok\xC3\xA9mon Diamond", "Pok\xC3\xA9mon Pearl", "Pok\xC3\xA9mon Platinum",
	                               "Pok\xC3\xA9mon HeartGold", "Pok\xC3\xA9mon SoulSilver" };
	snprintf(t->name, sizeof(t->name), "%s", names[game >= 1 && game <= 5 ? game : 0]);
	if ((title_id & 0xFFFFFF) == 0) { // the game card
		static u8 banner[DS_BANNER_SIZE];
		Result r = FSUSER_GetLegacyBannerData(MEDIATYPE_GAME_CARD, 0ULL, banner);
		if (R_FAILED(r)) {
			dbg_printf("ui: DS card banner %08lx", (unsigned long)r);
			return 0;
		}
		u16 pal[16];
		for (int i = 0; i < 16; i++) {
			u16 c = (u16)(banner[0x220 + i * 2] | (banner[0x221 + i * 2] << 8));
			pal[i] = rgb565((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
		}
		for (int y = 0; y < 48; y++)
			for (int x = 0; x < 48; x++) {
				int sx = x * 32 / 48, sy = y * 32 / 48; // nearest
				int tile = (sy >> 3) * 4 + (sx >> 3), in = (sy & 7) * 8 + (sx & 7);
				u8 b = banner[0x20 + tile * 32 + in / 2];
				int ci = (in & 1) ? b >> 4 : b & 15;
				px[y * 48 + x] = ci ? pal[ci] : 0xFFFF;
			}
	} else { // a .sav file on the SD card: a plain cartridge
		static const u8 label[6][3] = { { 150, 150, 150 }, { 60, 110, 210 }, { 220, 120, 160 },
		                                { 120, 120, 135 }, { 220, 170, 40 }, { 170, 175, 190 } };
		const u8 *lc = label[game >= 1 && game <= 5 ? game : 0];
		u16 white = 0xFFFF, body = rgb565(70, 72, 80), edge = rgb565(40, 40, 46), lab = rgb565(lc[0], lc[1], lc[2]);
		for (int y = 0; y < 48; y++)
			for (int x = 0; x < 48; x++) {
				u16 c = white;
				int in_body = x >= 8 && x < 40 && y >= 6 && y < 44 && !(x >= 34 && y < 12 && (x - 34) > (y - 6));
				if (in_body) {
					int border = x == 8 || x == 39 || y == 6 || y == 43 || (x >= 34 && y < 12 && (x - 34) == (y - 6));
					c = border ? edge : body;
					if (x >= 12 && x < 36 && y >= 14 && y < 34)
						c = lab; // the label
					if (y >= 38 && y < 42 && x >= 12 && x < 36 && (x & 1))
						c = edge; // the contacts
				}
				px[y * 48 + x] = c;
			}
	}
	ui_image_from_rgb565(&t->icon, px, 48, 48);
	t->ok = 1;
	return 1;
}

int ui_title_load(ui_title *t, u64 title_id)
{
	static u8 smdh[SMDH_SIZE];
	memset(t, 0, sizeof(*t));
	t->title_id = title_id;
	if ((title_id >> 32) == 0x0004800DULL)
		return ds_title_load(t, title_id);
	if (!read_smdh(title_id, MEDIATYPE_SD, smdh) && !read_smdh(title_id, MEDIATYPE_GAME_CARD, smdh)) {
		dbg_printf("ui: no SMDH for %016llx", (unsigned long long)title_id);
		return 0;
	}
	u16 name16[0x41];
	memcpy(name16, smdh + SMDH_TITLES + SMDH_LANG_EN * 0x200, 0x80);
	name16[0x40] = 0;
	ssize_t n = utf16_to_utf8((u8 *)t->name, name16, sizeof(t->name) - 1);
	t->name[n > 0 ? n : 0] = 0;
	for (char *c = t->name; *c; c++)
		if (*c == '\n')
			*c = ' ';
	// untile the 48x48 icon (8x8 tiles, Z order inside a tile)
	static u16 px[48 * 48];
	const u16 *src = (const u16 *)(smdh + SMDH_LARGE_ICON);
	for (int y = 0; y < 48; y++)
		for (int x = 0; x < 48; x++) {
			int in = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
			px[y * 48 + x] = src[((y >> 3) * 6 + (x >> 3)) * 64 + in];
		}
	ui_image_from_rgb565(&t->icon, px, 48, 48);
	t->ok = 1;
	return 1;
}

void ui_title_free(ui_title *t)
{
	ui_image_free(&t->icon);
	t->ok = 0;
}
