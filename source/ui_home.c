// ui_home.c - Home (mock_1_home) and the plain list screen used by Walker tools / Settings (v0.32).
// Draw + input only: main.c fills the data and runs whatever was chosen.
#include <stdio.h>
#include <string.h>

#include "ui_home.h"
#include "ui_icons.h"
#include "sound.h"

static ui_image home_icon; // colour box icon of the walker slot's Pokemon, loaded while Home runs

static const char *const k_tiles[3][3] = {
	// v0.50 (user: "For the home menu let's do walker, game settings, and settings")
	{ "Walker", "Link test, backup,", "set up, reset" },
	{ "Game settings", "Move Pok\xC3\xA9mon,", "restore saves" },
	{ "Settings", "Background,", "trainer, credits" },
};

static int touched(touchPosition t, float x, float y, float w, float h)
{
	return t.px >= x && t.px < x + w && t.py >= y && t.py < y + h;
}

// v0.57 (user: "the text is a tiny bit too small. The headers also should be what is bold ... Is that not how our
// other screens do it?"): as the route info screen - the heading bold, its value plain, both a little larger
static void fact(float x, float y, const char *k, const char *v)
{
	ui_text(x, y, k, 9.5f, UI_TEXT, 1, 'l', 0);
	ui_text(x, y + 11, v, 10.5f, UI_TEXT, 0, 'l', 0);
}

// A game name that fits under its icon (92 px): "Pokémon " dropped first (as the mockup's "Alpha Sapphire"), then
// cut with "…".
static void fit_name(const char *in, char *out, size_t n)
{
	const float w = 92;
	snprintf(out, n, "%s", in);
	if (ui_text_width(out, 9) <= w)
		return;
	if (!strncmp(in, "Pok\xC3\xA9mon ", 9))
		snprintf(out, n, "%s", in + 9);
	else if (!strncmp(in, "Pokemon ", 8))
		snprintf(out, n, "%s", in + 8);
	size_t len = strlen(out);
	while (len > 1 && ui_text_width(out, 9) > w) {
		do // cut one UTF-8 character
			len--;
		while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80);
		snprintf(out + len, n - len, "\xE2\x80\xA6");
	}
}

// v0.43: the games row (mock_1_home): up to four HOME icons + names, centred; "+N more" if there are more.
static void draw_games(const ui_home_games *g)
{
	if (!g || !g->scanned) {
		ui_text(200, 204, "Looking for your games\xE2\x80\xA6", 9, UI_SUB, 0, 'c', 1);
		return;
	}
	if (!g->n) {
		ui_text(200, 198, "No Pok\xC3\xA9mon saves found on this 3DS.", 10, UI_SUB, 0, 'c', 1);
		ui_text(200, 212, "(A game must be saved at least once.)", 9, UI_SUB, 0, 'c', 1);
		return;
	}
	float x0 = 200 - (g->n - 1) * 48.0f;
	char name[64];
	for (int i = 0; i < g->n; i++) {
		float cx = x0 + i * 96;
		if (g->icon[i].ok)
			ui_image_draw(&g->icon[i].icon, cx - 16, 183, 32.0f / 48.0f);
		else
			ui_box(UI_GAMEICON, cx - 16, 183, 32, 32);
		fit_name(g->name[i], name, sizeof(name));
		ui_text(cx, 224, name, 9, UI_TEXT, 1, 'c', 1);
	}
	if (g->total > g->n) {
		char more[24];
		snprintf(more, sizeof(more), "+%d more", g->total - g->n);
		ui_text(386, 168, more, 8.5f, C2D_Color32(230, 246, 220, 255), 0, 'r', 1);
	}
}

static void draw_home_top(const ui_home_data *d, int frame)
{
	ui_target_top();
	ui_status_bar(d->trainer);
	// v0.51 (user: "Where's the pixel grid?"): the walker screen at 2x with the LCD grid (a grid needs 2 screen pixels
	// per LCD pixel); the facts move right of it, "Walking since" becomes a fact, the games panel is 4 px shorter
	ui_box(UI_PANEL, 8, 22, 384, 128);
	ui_band(8, 22, 384, 20, "Pok\xC3\xA9mon Info", 12, 1); // v0.49 (user: "could it say something like Pokemon Info")
	ui_lcd_frame(20, 48, 128, 96);
	ui_lcd(20, 48, 64, 48, 2);
	if (d->slot != UI_SLOT_EMPTY)
		ui_image_draw(&d->big[d->animate ? frame : 0], 20, 48, 2);
	ui_lcd_grid(20, 48, 64, 48, 2);
	const float tx = 162;
	ui_text(tx, 50, d->headline, 13, UI_TEXT, 1, 'l', 0);
	if (d->slot == UI_SLOT_WALKING) {
		fact(tx, 70, "Route", d->route);
		fact(282, 70, "Watts kept", d->watts);
		fact(tx, 96, "Training", d->training);
		fact(282, 96, "Walking since", d->since);
	} else {
		ui_text(tx, 76, d->line1, 10.5f, UI_SUB, 0, 'l', 0);
		ui_text(tx, 92, d->line2, 10.5f, UI_SUB, 0, 'l', 0);
	}
	if (d->goes_back[0]) {
		if (d->game.ok)
			ui_image_draw(&d->game.icon, tx, 122, 20.0f / 48.0f);
		else
			ui_box(UI_GAMEICON, tx, 122, 20, 20);
		char gb[96];
		snprintf(gb, sizeof(gb), "%s", d->goes_back);
		char *tail = strstr(gb, " at walk end");
		if (tail && ui_text_width(gb, 9) > 384 - (tx + 26)) // too long for the panel: without "at walk end"
			*tail = 0;
		ui_text(tx + 26, 132, gb, 9, UI_SUB, 0, 'l', 1);
	}
	ui_box(UI_PANEL, 8, 158, 384, 76);
	ui_band(8, 158, 384, 20, "Games on this 3DS", 12, 0);
	draw_games(d->games);
}

static void draw_home_bottom(const ui_home_data *d, int sel)
{
	ui_target_bottom();
	ui_band(0, 8, 320, 22, "What would you like to do?", 12, 0);
	ui_box(sel == 0 ? UI_GLOSS6_SEL : UI_GLOSS6, 12, 40, 296, 70);
	ui_stripe(16, 46, 58);
	ui_box(UI_ICONBOX, 24, 50, 58, 50); // v0.51: wide enough for a 50 x 30 icon (user: sprites did not fit the box)
	// in-app icons are only the Gen 6/7 ones (user); the walker small sprite stays on the walker views
	if (d->slot != UI_SLOT_EMPTY && home_icon.ok)
		ui_icon_draw(&home_icon, 53, 75);
	else if (d->slot != UI_SLOT_EMPTY)
		ui_icon_placeholder(53, 75);
	// v0.49: a subtitle wider than the space before the A key goes on two lines (cut at a space), the title moves up
	const float sub_w = 186;
	char l1[96], l2[96];
	snprintf(l1, sizeof(l1), "%s", d->main_sub);
	l2[0] = 0;
	if (ui_text_width(l1, 10) > sub_w) {
		size_t len = strlen(l1), cut = 0;
		for (size_t i = 1; i < len; i++)
			if (l1[i] == ' ') {
				char t[96];
				snprintf(t, sizeof(t), "%.*s", (int)i, l1);
				if (ui_text_width(t, 10) > sub_w)
					break;
				cut = i;
			}
		if (cut) {
			snprintf(l2, sizeof(l2), "%s", l1 + cut + 1);
			l1[cut] = 0;
		}
		size_t n2 = strlen(l2);
		while (n2 > 1 && ui_text_width(l2, 10) > sub_w) { // still too long: cut with "…"
			do
				n2--;
			while (n2 > 0 && ((unsigned char)l2[n2] & 0xC0) == 0x80);
			snprintf(l2 + n2, sizeof(l2) - n2, "\xE2\x80\xA6");
		}
	}
	if (l2[0]) {
		ui_text(88, 52, d->main_title, 15, UI_TEXT, 1, 'l', 0);
		ui_text(88, 74, l1, 10, UI_SUB, 0, 'l', 0);
		ui_text(88, 88, l2, 10, UI_SUB, 0, 'l', 0);
	} else {
		ui_text(88, 62, d->main_title, 15, UI_TEXT, 1, 'l', 0);
		ui_text(88, 84, d->main_sub, 10, UI_SUB, 0, 'l', 0);
	}
	ui_key(290, 75, 'A', 10);
	for (int i = 0; i < 3; i++) {
		float x = 12 + i * 100;
		ui_box(sel == i + 1 ? UI_GLOSS6_SEL : UI_GLOSS6, x, 120, 96, 70);
		ui_text(x + 48, 140, k_tiles[i][0], 11, UI_TEXT, 1, 'c', 1);
		// v0.34 (user: "the small text under walker tools is hard to read"): 8 -> 9.5, darker
		ui_text(x + 48, 159, k_tiles[i][1], 9.5f, C2D_Color32(64, 86, 58, 255), 0, 'c', 1);
		ui_text(x + 48, 173, k_tiles[i][2], 9.5f, C2D_Color32(64, 86, 58, 255), 0, 'c', 1);
	}
	static const char *const l[] = { "B Close" }, *const r[] = { "A Select" };
	ui_footer(l, 1, r, 1);
}

static int home_run(const ui_home_data *d, int *sel);

int ui_home_run(const ui_home_data *d, int *sel)
{
	// Home never runs during IR, so the icon file may be read here.
	if (d->slot != UI_SLOT_EMPTY && d->species > 0)
		ui_icon_load(&home_icon, d->species, d->form, d->female, d->gen);
	int r = home_run(d, sel);
	ui_image_free(&home_icon);
	return r;
}

static int home_run(const ui_home_data *d, int *sel)
{
	int frame = 0;
	u64 flip = osGetTime();
	while (aptMainLoop()) {
		sound_input();
		u32 down = hidKeysDown();
		if (down & (KEY_START | KEY_B))
			return UI_HOME_EXIT;
		if ((down & KEY_SELECT) && (hidKeysHeld() & KEY_L)) { // L + SELECT: debug view (user 2026-11-15)
			ui_debug_view();
			continue;
		}
		if (down & KEY_SELECT)
			return UI_HOME_GUARD;
		if ((down & KEY_DOWN) && *sel == 0)
			*sel = 1;
		else if ((down & KEY_UP) && *sel > 0)
			*sel = 0;
		else if ((down & KEY_RIGHT) && *sel > 0 && *sel < 3)
			(*sel)++;
		else if ((down & KEY_LEFT) && *sel > 1)
			(*sel)--;
		if (down & KEY_TOUCH) {
			touchPosition t;
			hidTouchRead(&t);
			int hit = touched(t, 12, 40, 296, 70) ? 0 : -1;
			for (int i = 0; i < 3 && hit < 0; i++)
				if (touched(t, 12 + i * 100, 120, 96, 70))
					hit = i + 1;
			if (hit >= 0) {
				*sel = hit;
				down |= KEY_A;
			}
		}
		if (down & KEY_A)
			return *sel == 0 ? UI_HOME_MAIN : *sel == 1 ? UI_HOME_TOOLS : *sel == 2 ? UI_HOME_GAMESET : UI_HOME_SETTINGS;
		if (d->animate && osGetTime() - flip >= 500) { // the walker alternates its two frames
			frame ^= 1;
			flip = osGetTime();
		}
		if (ui_begin()) {
			draw_home_top(d, frame);
			draw_home_bottom(d, *sel);
			ui_end();
		}
	}
	return UI_HOME_EXIT;
}

// --- list screen ---------------------------------------------------------------------------------------------
#define ROW_H   28
#define ROWS    6
#define LIST_Y  40

int ui_list_run(const char *page, const char *title, const char *const *labels, const char *const *help, int n,
                int *sel)
{
	int first = 0;
	if (n <= 0) // audit M13: an empty list has nothing to choose
		return -1;
	if (*sel < 0 || *sel >= n)
		*sel = 0;
	while (aptMainLoop()) {
		sound_input();
		u32 down = hidKeysDown();
		if (down & KEY_B)
			return -1;
		if ((down & KEY_SELECT) && (hidKeysHeld() & KEY_L)) { // L + SELECT: debug view
			ui_debug_view();
			continue;
		}
		if (down & KEY_DOWN)
			*sel = (*sel + 1) % n;
		if (down & KEY_UP)
			*sel = (*sel + n - 1) % n;
		if (*sel < first)
			first = *sel;
		if (*sel >= first + ROWS)
			first = *sel - ROWS + 1;
		if (down & KEY_TOUCH) {
			touchPosition t;
			hidTouchRead(&t);
			for (int i = 0; i < ROWS && first + i < n; i++)
				if (touched(t, 16, LIST_Y + i * ROW_H, 288, ROW_H - 4)) {
					*sel = first + i;
					down |= KEY_A;
				}
		}
		if (down & KEY_A)
			return *sel;
		if (!ui_begin())
			continue;
		ui_target_top();
		ui_status_bar(page);
		ui_box(UI_PANEL, 8, 30, 384, 120);
		ui_band(8, 30, 384, 20, labels[*sel], 12, 0);
		// help text: one line per '\n'
		char buf[256];
		snprintf(buf, sizeof(buf), "%s", help ? help[*sel] : "");
		float y = 60;
		for (char *line = strtok(buf, "\n"); line; line = strtok(NULL, "\n"), y += 14)
			ui_text(20, y, line, 10, UI_TEXT, 0, 'l', 0);
		ui_target_bottom();
		ui_band(0, 8, 320, 22, title, 12, 0);
		for (int i = 0; i < ROWS && first + i < n; i++) {
			int k = first + i;
			float ry = LIST_Y + i * ROW_H;
			ui_box(k == *sel ? UI_GLOSS3_SEL : UI_GLOSS3, 16, ry, 288, ROW_H - 4);
			ui_rect(19, ry + 4, 3, ROW_H - 12, C2D_Color32(70, 130, 62, 255));
			ui_text(30, ry + (ROW_H - 4) / 2.0f, labels[k], 11, UI_TEXT, 1, 'l', 1);
		}
		if (first > 0)
			ui_arrow(160, LIST_Y - 6, 'u', 4, UI_SUB); // v0.49: drawn (no arrow glyph in the font)
		if (first + ROWS < n)
			ui_arrow(160, LIST_Y + ROWS * ROW_H - 1, 'd', 4, UI_SUB);
		static const char *const l[] = { "B Back" }, *const r[] = { "A Select" };
		ui_footer(l, 1, r, 1);
		ui_end();
	}
	return -1;
}
