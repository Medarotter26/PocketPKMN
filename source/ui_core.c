// ui_core.c - drawing layer for WalkerBox's GPU screens (citro2d). See include/ui.h and docs/UI_PLAN.md.
//
// Pieces (panels, buttons, bands, badges, header, footer) and the two backgrounds are pictures made on the PC by
// tools/ui_atlas.py with the mockup code at 1x; they are built into the 3dsx (gfx/*.t3s), so drawing never reads
// a file. All text is drawn here with the app's font.
#include <stdlib.h>
#include <string.h>

#include "dbg.h"
#include "pw_image.h"
#include "app_paths.h"
#include "ui.h"
#include "ui_atlas_t3x.h"
#include "ui_bgbot_t3x.h"
#include "ui_bgtop_t3x.h"
#include "ui_unknown_t3x.h"

// Row order of our own textures. 0: image row 0 at the start of the texture memory, subtexture top = 1.0.
// If pictures made here ever show upside down, this is the one switch.
#define UI_TEX_FLIP 0

// Font size of the mockups -> citro2d scale of the system font.
#define UI_FONT_K (1.0f / 24.0f)

static C3D_RenderTarget *t_top, *t_bot;
static C2D_SpriteSheet sh_atlas, sh_bgtop, sh_bgbot, sh_unknown;
static C2D_TextBuf tbuf;
static int cur_w = UI_TOP_W;

// --- IR time ---------------------------------------------------------------------------------------------
static int ir_busy;
static unsigned ir_frames, ir_skipped;
static u64 ir_max_ticks;

void ui_set_ir_busy(int busy)
{
	if (busy && !ir_busy) {
		ir_frames = ir_skipped = 0;
		ir_max_ticks = 0;
	}
	if (!busy && ir_busy)
		dbg_printf("ui: during IR %u frame(s) drawn, %u skipped (GPU busy), longest %lu us", ir_frames, ir_skipped,
		           (unsigned long)(ir_max_ticks * 1000000ull / SYSCLOCK_ARM11));
	ir_busy = busy;
}

int ui_begin(void)
{
	if (ir_busy) {
		if (!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) {
			ir_skipped++;
			return 0;
		}
		ir_frames++;
	} else {
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	}
	C2D_TextBufClear(tbuf);
	return 1;
}

void ui_end(void)
{
	C3D_FrameEnd(0);
}

// v0.47: four backgrounds per screen (gfx/ui_bgtop.t3s / ui_bgbot.t3s order), chosen in Settings
static int bg_index;

void ui_set_background(int index)
{
	bg_index = index >= 0 && index < UI_BACKGROUNDS ? index : 0;
}

int ui_background(void)
{
	return bg_index;
}

const char *ui_background_name(int index)
{
	static const char *const names[UI_BACKGROUNDS] = { "Classic", "Pok\xC3\xA9 Ball", "Walker screen", "Route scene" };
	return names[index >= 0 && index < UI_BACKGROUNDS ? index : 0];
}

// v0.48: 1 and 2 scroll down slowly (256-px tall tiles), 3 scrolls left (512-px wide tile). Whole pixels only, so
// the nearest-filtered tiles stay sharp; the offset comes from the clock (no state, nothing to load).
#define BG_TILE_V 256
#define BG_TILE_H 512
#define BG_TILE_A_W 448 // the Poke Ball tile repeats sideways too (7 balls of 64 px)
#define BG_MS_PER_PX_V 80 // 12.5 px a second
#define BG_MS_PER_PX_H 60 // about 17 px a second

static void draw_bg(C2D_SpriteSheet sh)
{
	size_t n = C2D_SpriteSheetCount(sh);
	size_t i = (size_t)bg_index < n ? (size_t)bg_index : 0;
	C2D_Image im = C2D_SpriteSheetGetImage(sh, i);
	u64 ms = osGetTime();
	if (i == 1) { // v0.50 (user: "Can we have that one diagonally scroll"): down and to the right, 448 x 256 tile
		float y = (float)(int)((ms / BG_MS_PER_PX_V) % BG_TILE_V);
		float x = (float)(int)((ms / BG_MS_PER_PX_V) % BG_TILE_A_W);
		for (int dx = 0; dx < 2; dx++)
			for (int dy = 0; dy < 2; dy++)
				C2D_DrawImageAt(im, x - BG_TILE_A_W * dx, y - BG_TILE_V * dy, 0, NULL, 1, 1);
	} else if (i == 2) {
		float y = (float)(int)((ms / BG_MS_PER_PX_V) % BG_TILE_V);
		C2D_DrawImageAt(im, 0, y - BG_TILE_V, 0, NULL, 1, 1);
		C2D_DrawImageAt(im, 0, y, 0, NULL, 1, 1);
	} else if (i == 3) {
		float x = (float)(int)((ms / BG_MS_PER_PX_H) % BG_TILE_H);
		C2D_DrawImageAt(im, -x, 0, 0, NULL, 1, 1);
		C2D_DrawImageAt(im, BG_TILE_H - x, 0, 0, NULL, 1, 1);
	} else
		C2D_DrawImageAt(im, 0, 0, 0, NULL, 1, 1);
}

int ui_background_moves(void)
{
	return bg_index >= 1 && bg_index < UI_BACKGROUNDS;
}

void ui_target_top(void)
{
	C2D_TargetClear(t_top, C2D_Color32(196, 228, 170, 255));
	C2D_SceneBegin(t_top);
	cur_w = UI_TOP_W;
	draw_bg(sh_bgtop);
}

void ui_target_bottom(void)
{
	C2D_TargetClear(t_bot, C2D_Color32(196, 228, 170, 255));
	C2D_SceneBegin(t_bot);
	cur_w = UI_BOT_W;
	draw_bg(sh_bgbot);
}

// --- our own textures --------------------------------------------------------------------------------------
static inline u32 tex_index(int x, int y, int tw, int th)
{
#if UI_TEX_FLIP
	y = th - 1 - y;
#else
	(void)th;
#endif
	return (((y >> 3) * (tw >> 3) + (x >> 3)) << 6) |
	       ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));
}

static int pow2(int v)
{
	int p = 8;
	while (p < v)
		p <<= 1;
	return p;
}

static int image_alloc(ui_image *im, int w, int h)
{
	memset(im, 0, sizeof(*im));
	int tw = pow2(w), th = pow2(h);
	if (!C3D_TexInit(&im->tex, (u16)tw, (u16)th, GPU_RGBA8))
		return 0;
	C3D_TexSetFilter(&im->tex, GPU_NEAREST, GPU_NEAREST);
	memset(im->tex.data, 0, (size_t)tw * th * 4);
	im->sub.width = (u16)w;
	im->sub.height = (u16)h;
	im->sub.left = 0.0f;
	im->sub.right = (float)w / tw;
	im->sub.top = 1.0f;
	im->sub.bottom = 1.0f - (float)h / th;
	im->img.tex = &im->tex;
	im->img.subtex = &im->sub;
	im->ok = 1;
	return 1;
}

static inline u32 rgba(u8 r, u8 g, u8 b, u8 a)
{
	return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | a;
}

static const u8 lcd_rgb[4][3] = { { 196, 206, 168 }, { 140, 152, 116 }, { 82, 92, 66 }, { 30, 36, 24 } };

int ui_image_from_walker(ui_image *im, const u8 *data, int w, int h, int transparent_white)
{
	u8 shades[64 * 48];
	if (w * h > (int)sizeof(shades) || !image_alloc(im, w, h))
		return 0;
	pw_img_decode(data, w, h, shades);
	u32 *px = (u32 *)im->tex.data;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			int s = shades[y * w + x] & 3;
			px[tex_index(x, y, im->tex.width, im->tex.height)] =
			    (transparent_white && !s) ? 0 : rgba(lcd_rgb[s][0], lcd_rgb[s][1], lcd_rgb[s][2], 255);
		}
	C3D_TexFlush(&im->tex);
	return 1;
}

int ui_image_from_rgb565(ui_image *im, const u16 *src, int w, int h)
{
	if (!image_alloc(im, w, h))
		return 0;
	u32 *px = (u32 *)im->tex.data;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			u16 c = src[y * w + x];
			u8 r = (u8)(((c >> 11) & 31) * 255 / 31), g = (u8)(((c >> 5) & 63) * 255 / 63), b = (u8)((c & 31) * 255 / 31);
			px[tex_index(x, y, im->tex.width, im->tex.height)] = rgba(r, g, b, 255);
		}
	C3D_TexFlush(&im->tex);
	return 1;
}

void ui_image_free(ui_image *im)
{
	if (im->ok)
		C3D_TexDelete(&im->tex);
	im->ok = 0;
}

void ui_image_draw(const ui_image *im, float x, float y, float scale)
{
	if (im->ok)
		C2D_DrawImageAt(im->img, x, y, 0, NULL, scale, scale);
}

// --- atlas pieces --------------------------------------------------------------------------------------------
// Index = line in gfx/ui_atlas.t3s (tools/ui_atlas.py, PIECES order).
enum {
	A_PANEL, A_GLOSS6, A_GLOSS6_SEL, A_GLOSS3, A_GLOSS3_SEL, A_INFO, A_ICONBOX, A_GAMEICON, A_LCDFRAME,
	A_BAND20_G, A_BAND20_R, A_BAND22_G, A_BAND22_R, A_KEY7, A_KEY8, A_KEY10, A_STRIPE, A_STATUS, A_FOOTER,
	A_SCENE, // v0.46: the 3DS + Pokewalker render with the IR arcs (368 x 116)
	A_SCENE_A0 // v0.50: 20-23 = the same picture with 0, 1, 2, 3 arcs (A_SCENE = all 4)
};
#define PAD 6 // shadow room around each piece (tools/ui_atlas.py P)

// slice size (corner) of each 9-slice piece, in canvas pixels
static const u8 nine_corner[UI_PIECE_COUNT] = { 14, 14, 14, 11, 11, 14, 14, 14, 15 };

// Draws the part (sx, sy, sw, sh) of an atlas image into (dx, dy, dw, dh).
static void draw_part(C2D_Image im, float sx, float sy, float sw, float sh, float dx, float dy, float dw, float dh)
{
	const Tex3DS_SubTexture *s = im.subtex;
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return;
	Tex3DS_SubTexture p;
	p.width = (u16)sw;
	p.height = (u16)sh;
	float du = (s->right - s->left) / s->width, dv = (s->bottom - s->top) / s->height;
	p.left = s->left + du * sx;
	p.right = s->left + du * (sx + sw);
	p.top = s->top + dv * sy;
	p.bottom = s->top + dv * (sy + sh);
	C2D_Image part = { im.tex, &p };
	C2D_DrawImageAt(part, dx, dy, 0, NULL, dw / sw, dh / sh);
}

static void nine_slice(C2D_Image im, int c, float x, float y, float w, float h)
{
	float W = im.subtex->width, H = im.subtex->height;
	float cx = c, cy = c;
	if (w < 2 * cx)
		cx = w / 2;
	if (h < 2 * cy)
		cy = h / 2;
	float xs[3][2] = { { 0, cx }, { cx, W - 2 * c }, { W - cx, cx } };  // source x, w
	float ys[3][2] = { { 0, cy }, { cy, H - 2 * c }, { H - cy, cy } };
	float xd[3][2] = { { x, cx }, { x + cx, w - 2 * cx }, { x + w - cx, cx } }; // dest x, w
	float yd[3][2] = { { y, cy }, { y + cy, h - 2 * cy }, { y + h - cy, cy } };
	for (int j = 0; j < 3; j++)
		for (int i = 0; i < 3; i++)
			draw_part(im, xs[i][0], ys[j][0], xs[i][1], ys[j][1], xd[i][0], yd[j][0], xd[i][1], yd[j][1]);
}

void ui_box(int piece, float x, float y, float w, float h)
{
	if (piece < 0 || piece >= UI_PIECE_COUNT)
		return;
	nine_slice(C2D_SpriteSheetGetImage(sh_atlas, (size_t)piece), nine_corner[piece], x - PAD, y - PAD, w + 2 * PAD,
	           h + 2 * PAD);
}

void ui_rect(float x, float y, float w, float h, u32 color)
{
	C2D_DrawRectSolid(x, y, 0, w, h, color);
}

// --- text ---------------------------------------------------------------------------------------------------
static int parse(C2D_Text *t, const char *s)
{
	if (!s || !*s)
		return 0;
	if (!C2D_TextParse(t, tbuf, s))
		return 0;
	C2D_TextOptimize(t);
	return 1;
}

float ui_text_width(const char *s, float size)
{
	C2D_Text t;
	if (!parse(&t, s))
		return 0;
	float w = 0, h = 0;
	C2D_TextGetDimensions(&t, size * UI_FONT_K, size * UI_FONT_K, &w, &h);
	return w;
}

void ui_text(float x, float y, const char *s, float size, u32 color, int bold, char align, int mid)
{
	C2D_Text t;
	if (!parse(&t, s))
		return;
	float k = size * UI_FONT_K;
	if (mid) {
		float w = 0, h = 0;
		C2D_TextGetDimensions(&t, k, k, &w, &h);
		y -= h / 2;
	}
	u32 flags = C2D_WithColor | (align == 'c' ? C2D_AlignCenter : align == 'r' ? C2D_AlignRight : C2D_AlignLeft);
	C2D_DrawText(&t, flags, x, y, 0, k, k, color);
	if (bold) // the system font has no bold: a second pass half a pixel over
		C2D_DrawText(&t, flags, x + 0.5f, y, 0, k, k, color);
}

static void shadow_text(float x, float y, const char *s, float size, u32 color, u32 shadow, int bold, char align)
{
	ui_text(x + 0.8f, y + 0.8f, s, size, shadow, bold, align, 1);
	ui_text(x, y, s, size, color, bold, align, 1);
}

// --- composed pieces -------------------------------------------------------------------------------------------
void ui_band(float x, float y, float w, float h, const char *text, float size, int red)
{
	int idx = h >= 21 ? (red ? A_BAND22_R : A_BAND22_G) : (red ? A_BAND20_R : A_BAND20_G);
	C2D_Image im = C2D_SpriteSheetGetImage(sh_atlas, (size_t)idx);
	float W = im.subtex->width, H = im.subtex->height, c = PAD + 4;
	float dx = x - PAD, dw = w + 2 * PAD, dy = y - PAD;
	draw_part(im, 0, 0, c, H, dx, dy, c, H);
	draw_part(im, c, 0, W - 2 * c, H, dx + c, dy, dw - 2 * c, H);
	draw_part(im, W - c, 0, c, H, dx + dw - c, dy, c, H);
	shadow_text(x + w / 2, y + h / 2, text, size, UI_WHITE, C2D_Color32(0, 0, 0, 60), 1, 'c');
}

// Centre of a glyph's drawn pixels relative to where C2D draws it (left-aligned, top of the line), at scale 1.
// Read from the font's own glyph sheet (A4, 8x8 tiles; sheet row 0 = texture top, as our textures). Cached.
static int key_ink(char c, float *ox, float *oy)
{
	static float cx_[128], cy_[128];
	static signed char state[128]; // 0 unknown, 1 ok, -1 failed
	unsigned u = (unsigned char)c;
	if (u >= 128)
		return 0;
	if (state[u]) {
		*ox = cx_[u];
		*oy = cy_[u];
		return state[u] > 0;
	}
	state[u] = -1;
	CFNT_s *f = fontGetSystemFont();
	TGLP_s *t = f ? fontGetGlyphInfo(f) : NULL;
	if (!t || t->sheetFmt != GPU_A4 || !t->sheetData)
		return 0;
	int gi = fontGlyphIndexFromCodePoint(f, u);
	fontGlyphPos_s gp;
	fontCalcGlyphPos(&gp, f, gi, GLYPH_POS_CALC_VTXCOORD, 1.0f, 1.0f);
	const u8 *sheet = t->sheetData + (size_t)gp.sheetIndex * t->sheetSize;
	int W = t->sheetWidth, H = t->sheetHeight;
	int xl = (int)(gp.texcoord.left * W + 0.5f), xr = (int)(gp.texcoord.right * W + 0.5f);
	int yt = (int)((1.0f - gp.texcoord.top) * H + 0.5f), yb = (int)((1.0f - gp.texcoord.bottom) * H + 0.5f);
	int L = 1 << 30, R = -1, T = 1 << 30, B = -1;
	for (int y = yt; y < yb; y++)
		for (int x = xl; x < xr; x++) {
			int idx = (((y >> 3) * (W >> 3) + (x >> 3)) << 6) |
			          ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));
			u8 b = sheet[idx >> 1];
			int a = (idx & 1) ? b >> 4 : b & 15;
			if (a >= 6) {
				if (x < L) L = x;
				if (x > R) R = x;
				if (y < T) T = y;
				if (y > B) B = y;
			}
		}
	if (R < 0 || xr <= xl || yb <= yt)
		return 0;
	float sx = (gp.vtxcoord.right - gp.vtxcoord.left) / (xr - xl);
	float sy = (gp.vtxcoord.bottom - gp.vtxcoord.top) / (yb - yt);
	cx_[u] = gp.vtxcoord.left + ((L + R + 1) / 2.0f - xl) * sx;
	cy_[u] = gp.vtxcoord.top + ((T + B + 1) / 2.0f - yt) * sy;
	state[u] = 1;
	*ox = cx_[u];
	*oy = cy_[u];
	dbg_printf("ui: key '%c' ink x %d-%d y %d-%d of cell %d-%d / %d-%d -> centre %.2f, %.2f", c, L, R, T, B, xl, xr,
	           yt, yb, (double)cx_[u], (double)cy_[u]);
	return 1;
}

void ui_key(float cx, float cy, char key, int r)
{
	int idx = r >= 10 ? A_KEY10 : r >= 8 ? A_KEY8 : A_KEY7;
	int rr = r >= 10 ? 10 : r >= 8 ? 8 : 7;
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sh_atlas, (size_t)idx), cx - rr - PAD, cy - rr - PAD, 0, NULL, 1, 1);
	// v0.36 (user: "Make sure the button letters are centered in their circles"): the letter's drawn pixels
	// (measured in the font) are centred on the badge, not its line box. The bold pass adds 0.5 px -> -0.25.
	char s[2] = { key, 0 };
	float k = 10 * UI_FONT_K, ox = 0, oy = 0;
	if (!key_ink(key, &ox, &oy)) { // font not readable: the old way
		ui_text(cx + 0.5f, cy + 0.8f, s, 10, C2D_Color32(255, 255, 255, 160), 1, 'c', 1);
		ui_text(cx, cy + 0.3f, s, 10, C2D_Color32(58, 62, 70, 255), 1, 'c', 1);
		return;
	}
	float x = cx - ox * k - 0.25f, y = cy - oy * k;
	ui_text(x + 0.5f, y + 0.5f, s, 10, C2D_Color32(255, 255, 255, 160), 1, 'l', 0);
	ui_text(x, y, s, 10, C2D_Color32(58, 62, 70, 255), 1, 'l', 0);
}

void ui_stripe(float x, float y, float h)
{
	C2D_Image im = C2D_SpriteSheetGetImage(sh_atlas, A_STRIPE);
	C2D_DrawImageAt(im, x, y, 0, NULL, 1, h / im.subtex->height);
}

void ui_brackets(float x0, float y0, float x1, float y1)
{
	for (int side = 0; side < 2; side++) {
		float sx = side ? x1 : x0, dx = side ? -4 : 4;
		ui_rect(sx - 1, y0 - 1, 2, y1 - y0 + 2, UI_ACCENT);
		ui_rect(dx > 0 ? sx : sx + dx, y0 - 1, 4, 2, UI_ACCENT);
		ui_rect(dx > 0 ? sx : sx + dx, y1 - 1, 4, 2, UI_ACCENT);
	}
}

void ui_connect_scene(float x, float y, int searching)
{
	// v0.50 (user: "animate the signal image going from the pokewalker to the 3ds"; "as it's looking for a transfer,
	// the arc just flashes on and off, and once it's transferring then it does its animation"): searching = all
	// arcs on / off every 500 ms; transferring = the arcs appear one by one from the walker (0, 1, 2, 3, all 4),
	// 300 ms a step. From the clock (one picture a frame, nothing loaded).
	// v0.51 (user: "sometimes it starts at 0, sometimes 1 ... sometimes 4"): during a transfer the screen is drawn
	// only now and then, so a step taken from the clock jumped. Now the step only moves on by one, on a drawn frame,
	// once its time is up: always 0, 1, 2, 3, 4, 0 ... in order (slower when frames are rare).
	static u64 last;
	static int step, was_searching = -1;
	u64 ms = osGetTime();
	int period = searching ? 500 : 300, steps = searching ? 2 : 5;
	if (searching != was_searching) { // a new phase starts at its beginning
		was_searching = searching;
		step = 0;
		last = ms;
	} else if (ms - last >= (u64)period) {
		step = (step + 1) % steps;
		last = ms;
	}
	size_t i;
	if (searching)
		i = step ? (size_t)A_SCENE_A0 : (size_t)A_SCENE; // all arcs, then none
	else
		i = step == 4 ? A_SCENE : (size_t)(A_SCENE_A0 + step);
	if (i >= C2D_SpriteSheetCount(sh_atlas))
		i = A_SCENE;
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sh_atlas, i), x, y, 0, NULL, 1, 1);
}

// v0.50 (user: "Instead of just a question mark for Pokemon not discovered yet in routes, use this"): the 32x32
// "?" picture the user gave, top-left at (x, y)
void ui_unknown_icon(float x, float y)
{
	if (sh_unknown && C2D_SpriteSheetCount(sh_unknown) > 0)
		C2D_DrawImageAt(C2D_SpriteSheetGetImage(sh_unknown, 0), x, y, 0, NULL, 1, 1);
}

// v0.49: a filled triangle pointing l/r/u/d, centred on (cx, cy), r = half its size
void ui_arrow(float cx, float cy, char dir, float r, u32 color)
{
	float x0, y0, x1, y1, x2, y2;
	switch (dir) {
	case 'l': x0 = cx - r; y0 = cy; x1 = cx + r * 0.7f; y1 = cy - r; x2 = cx + r * 0.7f; y2 = cy + r; break;
	case 'r': x0 = cx + r; y0 = cy; x1 = cx - r * 0.7f; y1 = cy - r; x2 = cx - r * 0.7f; y2 = cy + r; break;
	case 'u': x0 = cx; y0 = cy - r; x1 = cx - r; y1 = cy + r * 0.7f; x2 = cx + r; y2 = cy + r * 0.7f; break;
	default:  x0 = cx; y0 = cy + r; x1 = cx - r; y1 = cy - r * 0.7f; x2 = cx + r; y2 = cy - r * 0.7f; break;
	}
	C2D_DrawTriangle(x0, y0, color, x1, y1, color, x2, y2, color, 0);
}

void ui_status_bar(const char *right)
{
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sh_atlas, A_STATUS), 0, 0, 0, NULL, 1, 1);
	// v0.51 (user: "the text at the top green bar is a little thin and hard to read"): larger, both bold, white,
	// darker shadow
	shadow_text(20, 8, APP_NAME, 10.5f, UI_WHITE, C2D_Color32(0, 0, 0, 150), 1, 'l');
	if (right)
		shadow_text(UI_TOP_W - 6, 8, right, 10, UI_WHITE, C2D_Color32(0, 0, 0, 150), 1, 'r');
}

void ui_footer(const char *const *left, int nleft, const char *const *right, int nright)
{
	float y = UI_H - 24;
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(sh_atlas, A_FOOTER), 0, y - PAD, 0, NULL, 1, 1);
	float x = 8;
	for (int i = 0; i < nleft; i++) { // "B Close": key letter, space, text
		const char *t = left[i] + 2;
		ui_key(x + 7, y + 12, left[i][0], 7);
		ui_text(x + 18, y + 12, t, 10, UI_TEXT, 1, 'l', 1);
		x += 26 + ui_text_width(t, 10);
	}
	x = UI_BOT_W - 8;
	for (int i = nright - 1; i >= 0; i--) {
		const char *t = right[i] + 2;
		float tw = ui_text_width(t, 10);
		ui_text(x, y + 12, t, 10, UI_TEXT, 1, 'r', 1);
		ui_key(x - tw - 11, y + 12, right[i][0], 7);
		x -= tw + 32;
	}
}

void ui_lcd_frame(float x, float y, int w, int h)
{
	ui_box(UI_LCDFRAME, x - 6, y - 6, w + 12, h + 12);
}

void ui_lcd(float x, float y, int w, int h, int scale)
{
	ui_rect(x, y, w * scale, h * scale, UI_LCD_BG);
	ui_rect(x, y, w * scale, 1, C2D_Color32(0, 0, 0, 60)); // inner shadow on the glass
}

// v0.50 (user: "what happened to the pixel grid over Walker screen images?" - the mockups' lcd_grid, round 4, never
// made it into the app): a faint dark line between the LCD pixels of a walker picture drawn at scale >= 2
void ui_lcd_grid(float x, float y, int w, int h, int scale)
{
	if (scale < 2)
		return;
	u32 c = C2D_Color32(0, 0, 0, scale >= 3 ? 30 : 24);
	for (int i = 1; i < w; i++)
		ui_rect(x + i * scale, y, 1, h * scale, c);
	for (int j = 1; j < h; j++)
		ui_rect(x, y + j * scale, w * scale, 1, c);
}

void ui_info_box(float x, float y, float w, float h, const char *title, const char *value, float size)
{
	ui_box(UI_INFO, x, y, w, h);
	float step = size + 3, y0 = y + h / 2 - step / 2;
	ui_text(x + w / 2, y0, title, size, UI_TEXT, 1, 'c', 1);
	ui_text(x + w / 2, y0 + step, value, size, UI_TEXT, 0, 'c', 1);
}

// --- text view: the old consoles, in RAM ---------------------------------------------------------------------
static u16 *fb_top, *fb_bot;  // 400x240 / 320x240, column-major like the real framebuffer
static ui_image tv_top, tv_bot; // their textures (512x256)

u16 *ui_legacy_fb(gfxScreen_t screen)
{
	return screen == GFX_TOP ? fb_top : fb_bot;
}

static void legacy_convert(const u16 *fb, int w, ui_image *im)
{
	u32 *px = (u32 *)im->tex.data;
	const u32 text = rgba(40, 60, 36, 255);
	for (int x = 0; x < w; x++) {
		const u16 *col = fb + x * 240;
		for (int y = 0; y < 240; y++) {
			u16 c = col[239 - y];
			u32 v;
			switch (c) {
			case 0x0000: v = 0; break; // console background -> the panel shows through
			case UI_LEGACY_WHITE: v = rgba(lcd_rgb[0][0], lcd_rgb[0][1], lcd_rgb[0][2], 255); break;
			case 0xAD55: v = rgba(lcd_rgb[1][0], lcd_rgb[1][1], lcd_rgb[1][2], 255); break;
			case 0x52AA: v = rgba(lcd_rgb[2][0], lcd_rgb[2][1], lcd_rgb[2][2], 255); break;
			case UI_LEGACY_BLACK: v = rgba(lcd_rgb[3][0], lcd_rgb[3][1], lcd_rgb[3][2], 255); break;
			default: v = text; break; // console text
			}
			px[tex_index(x, y, im->tex.width, im->tex.height)] = v;
		}
	}
	C3D_TexFlush(&im->tex);
}

static void legacy_draw_frame(void)
{
	legacy_convert(fb_top, UI_TOP_W, &tv_top);
	legacy_convert(fb_bot, UI_BOT_W, &tv_bot);
	ui_target_top();
	ui_box(UI_PANEL, 2, 2, UI_TOP_W - 4, UI_H - 4);
	ui_image_draw(&tv_top, 0, 0, 1);
	ui_target_bottom();
	ui_box(UI_PANEL, 2, 2, UI_BOT_W - 4, UI_H - 4);
	ui_image_draw(&tv_bot, 0, 0, 1);
}

static int legacy_pending;

// --- v0.44: message screen ------------------------------------------------------------------------------------------
static int msg_mode;          // 0 = working ("One moment"), 1 = ask, 2 = OK
static int msg_dangerous, raw_mode;
static char msg_question[160];

void ui_msg_ask(const char *question, int dangerous)
{
	msg_mode = 1;
	msg_dangerous = dangerous;
	snprintf(msg_question, sizeof(msg_question), "%s", question ? question : "");
}

void ui_msg_ok(void)
{
	msg_mode = 2;
}

void ui_msg_clear(void)
{
	msg_mode = 0;
}

void ui_legacy_raw(int on)
{
	raw_mode = on;
}

// Lines that are key hints of the old text screens (the bottom screen shows the keys now) or the question itself.
static int msg_skip(const char *l)
{
	if (!strcmp(l, "A = yes, B = no") || !strcmp(l, "Hold L+R and press A = yes, B = no") ||
	    !strcmp(l, "Press A to go back."))
		return 1;
	return msg_mode == 1 && l[0] && strstr(msg_question, l) != NULL;
}

// v0.45 (user: "If we want to make this official looking, an official app would never show quite this extensive
// amount of detail"): link details, progress counters and file paths stay in the debug view only.
int ui_msg_technical(const char *l)
{
	static const char *const prefix[] = {
		"Connected", "Trainer:", "Paired:", "Total steps", "Saved /",
		"Saved ", "Waiting up to", "(Recorded in", "1. On the walker",
		"2. Point the walker", "   ", "Part ", "PocketPKMN Pokemon from box", "Back in PocketPKMN box", "Watts:",
		"Steps on this walk", "Owner:", "Menus and sounds", "(first walk from", "Walk:", "Route:", "Unique ID",
		"Identity", "Session", "Link test", "Bus ", "Transfers seen", "Sniffed", "(details in",
	};
	if (strstr(l, "/3ds/") || strstr(l, "eeprom_"))
		return 1;
	for (size_t i = 0; i < sizeof(prefix) / sizeof(prefix[0]); i++)
		if (!strncmp(l, prefix[i], strlen(prefix[i])))
			return 1;
	return 0;
}

static int all_caps_title(const char *l)
{
	int letters = 0;
	for (const char *p = l; *p; p++) {
		if (*p >= 'a' && *p <= 'z')
			return 0;
		letters += *p >= 'A' && *p <= 'Z';
	}
	return letters >= 3;
}

static void sentence(const char *in, char *out, size_t n)
{
	size_t k = 0;
	for (; in[k] && k + 1 < n; k++)
		out[k] = (k == 0 || in[k] < 'A' || in[k] > 'Z') ? in[k] : (char)(in[k] - 'A' + 'a');
	out[k] = 0;
}

static void draw_message_view(void)
{
	int n = ui_page_count(), first = 0;
	char title[64] = "";
	// header: the flow's title (its first line in capitals, e.g. "SEND TO WALKER")
	if (n > 0 && all_caps_title(ui_page_line(0))) {
		sentence(ui_page_line(0), title, sizeof(title));
		first = 1;
	}
	const char *lines[64];
	int m = 0;
	for (int i = first; i < n && m < 63; i++)
		if (!msg_skip(ui_page_line(i)))
			lines[m++] = ui_page_line(i);
	const char *cur = ui_page_current();
	if (cur[0] && !msg_skip(cur))
		lines[m++] = cur;
	while (m > 0 && !lines[m - 1][0]) // no blank line at the end
		m--;
	// v0.45: only what matters now - technical lines dropped, then the LAST paragraph (the lines after the last
	// blank line), centred and larger, like a system app's message. Everything is still in the debug view.
	const char *keep[64];
	int k = 0;
	for (int i = 0; i < m; i++)
		if (!lines[i][0] || !ui_msg_technical(lines[i]))
			keep[k++] = lines[i];
	while (k > 0 && !keep[k - 1][0])
		k--;
	int start = k;
	while (start > 0 && keep[start - 1][0])
		start--;
	if (k - start > 8)
		start = k - 8;
	ui_target_top();
	ui_status_bar(title[0] ? title : NULL);
	ui_box(UI_PANEL, 20, 34, 360, 188);
	if (title[0])
		ui_band(20, 34, 360, 22, title, 12, 0);
	int rows = k - start;
	float y = 132 - rows * 9.0f + (title[0] ? 10 : 0);
	for (int i = start; i < k; i++, y += 18) {
		int caps = all_caps_title(keep[i]); // a heading inside the page ("WALK ENDED. ...")
		char sc[64];
		int pct = -1;
		const char *pc = strrchr(keep[i], '%');
		if (pc) {
			const char *d0 = pc;
			while (d0 > keep[i] && d0[-1] >= '0' && d0[-1] <= '9')
				d0--;
			if (d0 < pc) {
				pct = atoi(d0);
				size_t t = (size_t)(d0 - keep[i]);
				while (t > 0 && keep[i][t - 1] == ' ')
					t--;
				snprintf(sc, sizeof(sc), "%.*s", (int)t, keep[i]);
			}
		}
		if (pct >= 0) { // v0.46: a bar instead of "Reading  100%"
			ui_text(60, y, sc, 11, UI_TEXT, 1, 'l', 1);
			char pt[8];
			snprintf(pt, sizeof(pt), "%d %%", pct > 100 ? 100 : pct);
			ui_text(340, y, pt, 11, UI_SUB, 1, 'r', 1);
			ui_rect(170, y - 4, 120, 8, C2D_Color32(222, 232, 214, 255));
			ui_rect(170, y - 4, 120.0f * (pct > 100 ? 100 : pct) / 100.0f, 8, UI_BAND);
		} else if (caps) {
			sentence(keep[i], sc, sizeof(sc));
			ui_text(200, y, sc, 13, UI_BAND, 1, 'c', 1);
		} else {
			ui_text(200, y, keep[i], 12, UI_TEXT, 0, 'c', 1);
		}
	}
	if (!rows && msg_mode == 0)
		ui_text(200, 140, "One moment\xE2\x80\xA6", 12, UI_SUB, 0, 'c', 1);
	ui_target_bottom();
	if (msg_mode == 1) {
		char q[160];
		snprintf(q, sizeof(q), "%s", msg_question);
		const char *ql[6];
		int qn = 0;
		for (char *s = strtok(q, "\n"); s && qn < 6; s = strtok(NULL, "\n"))
			ql[qn++] = s;
		// v0.49: the text centred between the band and two buttons inside the box, like a system dialog
		float body = qn * 18 + (msg_dangerous ? 18 : 0);
		float h = 22 + 14 + body + 12 + 30 + 12;
		float py = (UI_H - 24 - h) / 2;
		ui_box(UI_PANEL, 24, py, 272, h);
		ui_band(24, py, 272, 22, msg_dangerous ? "Please check" : "Question", 12, msg_dangerous);
		float ty = py + 22 + 14 + 9;
		for (int i = 0; i < qn; i++)
			ui_text(160, ty + i * 18, ql[i], 11.5f, UI_TEXT, 1, 'c', 1);
		if (msg_dangerous)
			ui_text(160, ty + qn * 18, "Hold L + R, then press A", 10, UI_ACCENT, 1, 'c', 1);
		float by = py + h - 12 - 30;
		ui_box(UI_GLOSS3, 40, by, 112, 30);
		ui_text(88, by + 15, "No", 12, UI_TEXT, 1, 'c', 1);
		ui_key(132, by + 15, 'B', 8);
		ui_box(UI_GLOSS3_SEL, 168, by, 112, 30);
		ui_text(216, by + 15, "Yes", 12, UI_TEXT, 1, 'c', 1);
		ui_key(260, by + 15, 'A', 8);
		static const char *const l[] = { "B No" }, *const r[] = { "A Yes" };
		ui_footer(l, 1, r, 1);
	} else if (msg_mode == 2) {
		ui_box(UI_GLOSS3_SEL, 70, 96, 180, 36);
		ui_text(160, 114, "OK", 13, UI_TEXT, 1, 'c', 1);
		ui_key(232, 114, 'A', 8);
		static const char *const r[] = { "A OK" };
		ui_footer(NULL, 0, r, 1);
	} else {
		ui_box(UI_PANEL, 60, 96, 200, 40);
		ui_text(160, 116, "One moment\xE2\x80\xA6", 11, UI_SUB, 0, 'c', 1);
	}
}

void ui_legacy_present(void)
{
	u64 t0 = svcGetSystemTick();
	if (ui_connect_active()) { // v0.36: a walker link is open -> the connect screen instead of the text
		legacy_pending = !ui_connect_present(ir_busy);
	} else {
		if (!ui_begin()) {
			legacy_pending = 1;
			return;
		}
		legacy_pending = 0;
		if (raw_mode)
			legacy_draw_frame();
		else
			draw_message_view(); // v0.44
		ui_end();
	}
	if (ir_busy) {
		u64 dt = svcGetSystemTick() - t0;
		if (dt > ir_max_ticks)
			ir_max_ticks = dt;
	}
}

void ui_loading(const char *text)
{
	if (!ui_begin())
		return;
	ui_target_top();
	ui_status_bar(NULL);
	ui_box(UI_PANEL, 100, 100, 200, 44);
	ui_text(200, 122, text, 12, UI_TEXT, 1, 'c', 1);
	ui_target_bottom();
	ui_box(UI_PANEL, 60, 100, 200, 44);
	ui_text(160, 122, "One moment\xE2\x80\xA6", 11, UI_SUB, 0, 'c', 1);
	ui_end();
}

void ui_legacy_retry(void)
{
	// v0.48: the message screen is drawn again each frame while a moving background is on (never in IR time, the
	// connect screen or the raw text view)
	if (legacy_pending || (ui_background_moves() && !ir_busy && !raw_mode && !ui_connect_active()))
		ui_legacy_present();
}

void ui_debug_view(void)
{
	int was = raw_mode;
	raw_mode = 1; // the debug view is the old text, exactly as printed
	ui_legacy_present();
	raw_mode = was;
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & KEY_B)
			break;
		gspWaitForVBlank();
	}
}

// --- start-up ---------------------------------------------------------------------------------------------------
static void nearest(C2D_SpriteSheet sh)
{
	C3D_TexSetFilter(C2D_SpriteSheetGetImage(sh, 0).tex, GPU_NEAREST, GPU_NEAREST);
}

void ui_init(PrintConsole *top, PrintConsole *bottom)
{
	// The consoles first (consoleInit routes stdout to the console), then pointed at RAM, then the screens
	// are given back to the GPU (consoleInit switched them to single-buffered RGB565).
	fb_top = (u16 *)calloc(UI_TOP_W * 240, sizeof(u16));
	fb_bot = (u16 *)calloc(UI_BOT_W * 240, sizeof(u16));
	consoleInit(GFX_TOP, top);
	top->frameBuffer = fb_top;
	consoleInit(GFX_BOTTOM, bottom);
	bottom->frameBuffer = fb_bot;
	ui_tap_stdout(); // v0.36: the connect screen reads the flows' progress lines (source/ui_connect.c)
	consoleSelect(top);
	consoleClear();
	consoleSelect(bottom);
	consoleClear();
	consoleSelect(top);
	gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
	gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxSetDoubleBuffering(GFX_BOTTOM, true);

	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 4);
	C2D_Prepare();
	t_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	t_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	sh_atlas = C2D_SpriteSheetLoadFromMem(ui_atlas_t3x, ui_atlas_t3x_size);
	sh_bgtop = C2D_SpriteSheetLoadFromMem(ui_bgtop_t3x, ui_bgtop_t3x_size);
	sh_bgbot = C2D_SpriteSheetLoadFromMem(ui_bgbot_t3x, ui_bgbot_t3x_size);
	sh_unknown = C2D_SpriteSheetLoadFromMem(ui_unknown_t3x, ui_unknown_t3x_size);
	nearest(sh_atlas);
	nearest(sh_bgtop);
	nearest(sh_bgbot);
	nearest(sh_unknown);
	tbuf = C2D_TextBufNew(8192);
	image_alloc(&tv_top, UI_TOP_W, UI_H);
	image_alloc(&tv_bot, UI_BOT_W, UI_H);
	dbg_printf("ui: start (atlas %s, backgrounds %s/%s)", sh_atlas ? "ok" : "MISSING", sh_bgtop ? "ok" : "MISSING",
	           sh_bgbot ? "ok" : "MISSING");
}

void ui_exit(void)
{
	ui_image_free(&tv_top);
	ui_image_free(&tv_bot);
	C2D_TextBufDelete(tbuf);
	C2D_SpriteSheetFree(sh_atlas);
	C2D_SpriteSheetFree(sh_bgtop);
	C2D_SpriteSheetFree(sh_bgbot);
	C2D_SpriteSheetFree(sh_unknown);
	C2D_Fini();
	C3D_Fini();
}
