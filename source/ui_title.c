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

int ui_title_load(ui_title *t, u64 title_id)
{
	static u8 smdh[SMDH_SIZE];
	memset(t, 0, sizeof(*t));
	t->title_id = title_id;
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
