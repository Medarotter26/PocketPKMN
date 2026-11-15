// ui_icons.c - colour box icons from romfs:/icons/icons.bin (format: tools/make_icons.py). The file's index is
// read once (about 8 KB, kept); each icon is read when a box / Home is shown and becomes a 64x32 RGBA texture.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_icons.h"

#define ICON_FILE "romfs:/icons/icons.bin"

typedef struct {
	u16 species;
	u8 form, flags;
	u32 offset;
} icon_entry;

static icon_entry *index_;
static int count_ = -1; // -1 = not read yet, 0 = no file / bad file
static FILE *file_;
static int held_;

static FILE *open_file(void)
{
	static int tried_mount;
	if (!file_)
		file_ = fopen(ICON_FILE, "rb");
	if (!file_ && !tried_mount) { // romfs not mounted yet (storage mounts it on first use): mount it here
		tried_mount = 1;
		romfsInit();
		file_ = fopen(ICON_FILE, "rb");
	}
	return file_;
}

static void close_file(void)
{
	if (file_ && !held_) {
		fclose(file_);
		file_ = NULL;
	}
}

static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

static int read_index(void)
{
	if (count_ >= 0)
		return count_;
	count_ = 0;
	FILE *f = open_file();
	u8 h[12];
	if (!f || fseek(f, 0, SEEK_SET) || fread(h, 1, 12, f) != 12 || memcmp(h, "PKIC", 4) || rd16(h + 4) != 1 ||
	    h[8] != UI_ICON_W || h[9] != UI_ICON_H)
		return 0;
	int n = rd16(h + 6);
	u8 *raw = malloc((size_t)n * 8);
	index_ = malloc((size_t)n * sizeof(icon_entry));
	if (raw && index_ && fread(raw, 8, (size_t)n, f) == (size_t)n) {
		for (int i = 0; i < n; i++) {
			index_[i].species = rd16(raw + i * 8);
			index_[i].form = raw[i * 8 + 2];
			index_[i].flags = raw[i * 8 + 3];
			index_[i].offset = rd32(raw + i * 8 + 4);
		}
		count_ = n;
	} else {
		free(index_);
		index_ = NULL;
	}
	free(raw);
	return count_;
}

static const icon_entry *find(int species, int form, int flags)
{
	int lo = 0, hi = count_ - 1;
	u32 key = (u32)species << 16 | (u32)form << 8 | (u32)flags;
	while (lo <= hi) {
		int mid = (lo + hi) / 2;
		const icon_entry *e = &index_[mid];
		u32 k = (u32)e->species << 16 | (u32)e->form << 8 | e->flags;
		if (k == key)
			return e;
		if (k < key)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

// The few form numbers that differ from Gen 7's order (tools/make_icons.py stores Gen 7 numbers).
static int map_form(int species, int form, int gen)
{
	if (gen <= 1 || form < 0 || form > 255)
		return 0;
	if (species == 493 && gen == 4 && form >= 9) // Gen 4 Arceus: form 9 = "???" type, Fire = 10, ...
		return form == 9 ? 0 : form - 1;
	if (species == 25 && gen == 6 && form >= 1 && form <= 6) // Omega Ruby / Alpha Sapphire cosplay Pikachu
		return 32 + form - 1;
	return form;
}

void ui_icons_begin(void)
{
	held_ = 1;
	open_file();
}

void ui_icons_end(void)
{
	held_ = 0;
	close_file();
}

// Morton order inside 8x8 tiles (as ui_core.c's textures)
static inline u32 tex_index(int x, int y, int tw)
{
	return (((y >> 3) * (tw >> 3) + (x >> 3)) << 6) |
	       ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));
}

int ui_icon_load(ui_image *im, int species, int form, int female, int gen)
{
	memset(im, 0, sizeof(*im));
	if (species <= 0 || species > 0xFFFF || read_index() <= 0)
		goto fail;
	form = map_form(species, form, gen);
	female = female ? 1 : 0;
	const icon_entry *e = find(species, form, female);
	if (!e)
		e = find(species, form, 0);
	if (!e)
		e = find(species, 0, female);
	if (!e)
		e = find(species, 0, 0);
	FILE *f = open_file();
	u8 hd[5], pal[255 * 3], px[UI_ICON_W * UI_ICON_H];
	if (!e || !f || fseek(f, (long)e->offset, SEEK_SET) || fread(hd, 1, 5, f) != 5)
		goto fail;
	int x0 = hd[0], y0 = hd[1], w = hd[2], h = hd[3], n = hd[4];
	if (x0 + w > UI_ICON_W || y0 + h > UI_ICON_H || fread(pal, 3, (size_t)n, f) != (size_t)n ||
	    fread(px, 1, (size_t)(w * h), f) != (size_t)(w * h))
		goto fail;
	const int tw = 64, th = 32;
	if (!C3D_TexInit(&im->tex, tw, th, GPU_RGBA8))
		goto fail;
	C3D_TexSetFilter(&im->tex, GPU_NEAREST, GPU_NEAREST);
	u32 *t = (u32 *)im->tex.data;
	memset(t, 0, (size_t)tw * th * 4);
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int v = px[y * w + x];
			if (v && v <= n) {
				const u8 *c = &pal[(v - 1) * 3];
				t[tex_index(x0 + x, y0 + y, tw)] = (u32)c[0] << 24 | (u32)c[1] << 16 | (u32)c[2] << 8 | 0xFF;
			}
		}
	C3D_TexFlush(&im->tex);
	im->sub.width = UI_ICON_W;
	im->sub.height = UI_ICON_H;
	im->sub.left = 0.0f;
	im->sub.right = (float)UI_ICON_W / tw;
	im->sub.top = 1.0f;
	im->sub.bottom = 1.0f - (float)UI_ICON_H / th;
	im->img.tex = &im->tex;
	im->img.subtex = &im->sub;
	im->ok = 1;
	close_file();
	return 1;
fail:
	close_file();
	return 0;
}

void ui_icon_draw(const ui_image *im, float cx, float cy)
{
	ui_image_draw(im, cx - UI_ICON_W / 2.0f, cy - UI_ICON_H / 2.0f, 1);
}

void ui_icon_placeholder(float cx, float cy)
{
	ui_text(cx, cy, "?", 13, UI_SUB, 1, 'c', 1);
}
