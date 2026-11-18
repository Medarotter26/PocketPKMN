// ui_send.c - the send screen (mock_4_send): the Pokemon as the walker will show it, the course list of its game
// and the EV stat to train. Draw + input only (main.c builds and sends the walk). Never runs during IR.
#include <stdio.h>
#include <string.h>

#include "sound.h"
#include "ui.h"
#include "ui_send.h"
#include "wbx_sprites.h"

static const char *const k_ev_short[7] = { "Off", "HP", "Atk", "Def", "SpA", "SpD", "Spe" };
static const char *const k_ev_long[6] = { "HP", "Attack", "Defense", "Sp. Atk", "Sp. Def", "Speed" };

#define ROWS 2
#define ROW_Y 36
#define ROW_H 34

static int ncourses(const ui_send_info *in)
{
	return in->rf && in->rf->ncourse > 0 ? in->rf->ncourse : 1;
}

static const char *course_name(const ui_send_info *in, int i)
{
	return in->rf && in->rf->ncourse > 0 ? in->rf->course[i].name : "Test Path";
}

// v0.52 (user: "block locked courses", "Greyed with a lock"): a course the save has not reached yet
static int locked(const ui_send_info *in, int i)
{
	return in->rf && in->rf->ncourse > 0 && in->progress_known &&
	       !routes_unlocked(in->rf->course[i].unlock, in->badges, in->hof, in->watts);
}

#define UI_LOCKED_TEXT C2D_Color32(150, 162, 146, 255)

// a small padlock, its body's top-left at (x, y)
static void lock_icon(float x, float y, u32 c)
{
	ui_rect(x + 1.5f, y - 5, 1.5f, 5, c);
	ui_rect(x + 6, y - 5, 1.5f, 5, c);
	ui_rect(x + 1.5f, y - 6, 6, 1.5f, c);
	ui_rect(x, y, 9, 7, c);
}

static void draw(const ui_send_info *in, ui_image *big, int frame, int animate, int sel, int first, int ev)
{
	char t[160];
	ui_target_top();
	ui_status_bar("Send to walker");
	ui_lcd_frame(138, 42, 128, 96);
	ui_lcd(138, 42, 64, 48, 2);
	ui_image_draw(&big[animate ? frame : 0], 138, 42, 2);
	ui_lcd_grid(138, 42, 64, 48, 2);
	// v0.52 (user: "this box feels very messy"): three columns - a small grey label, the value in bold, a short note
	// under it - instead of three bulleted sentences; the route is on the bottom screen already
	ui_box(UI_PANEL, 20, 172, 376, 62);
	const float cx[3] = { 36, 160, 290 };
	ui_text(cx[0], 186, "Pok\xC3\xA9mon", 9, UI_SUB, 0, 'l', 1);
	ui_text(cx[0], 202, in->mon[0] ? in->mon : in->line1, 12, UI_TEXT, 1, 'l', 1);
	ui_text(cx[1], 186, "From", 9, UI_SUB, 0, 'l', 1);
	ui_text(cx[1], 202, in->game[0] ? in->game : "-", 12, UI_TEXT, 1, 'l', 1);
	if (in->trainer[0]) {
		snprintf(t, sizeof(t), "Trainer %s", in->trainer);
		ui_text(cx[1], 218, t, 9, UI_SUB, 0, 'l', 1);
	}
	ui_text(cx[2], 186, "EV training", 9, UI_SUB, 0, 'l', 1);
	ui_text(cx[2], 202, ev >= 0 ? k_ev_long[ev] : in->ev_allowed ? "Off" : "None", 12, UI_TEXT, 1, 'l', 1);
	// v0.95 (owner: no "1 per 20 steps" formula on screen): a plain line
	ui_text(cx[2], 218, ev >= 0 ? "Trains while walking" : in->ev_allowed ? "" : "No EVs in Gen 1/2", 9, UI_SUB, 0, 'l', 1);
	ui_rect(cx[1] - 12, 182, 1, 40, C2D_Color32(176, 208, 156, 255));
	ui_rect(cx[2] - 12, 182, 1, 40, C2D_Color32(176, 208, 156, 255));
	(void)sel;

	ui_target_bottom();
	ui_band(10, 6, 300, 22, "Route", 12, 0);
	int n = ncourses(in);
	for (int r = 0; r < ROWS && first + r < n; r++) {
		int i = first + r;
		float y = ROW_Y + r * ROW_H;
		ui_box(i == sel ? UI_GLOSS3_SEL : UI_GLOSS3, 14, y, 292, ROW_H - 6);
		int lk = locked(in, i);
		float my = y + (ROW_H - 6) / 2.0f;
		if (lk)
			lock_icon(24, my - 2, UI_LOCKED_TEXT);
		ui_text(lk ? 40 : 24, my, course_name(in, i), 12, lk ? UI_LOCKED_TEXT : UI_TEXT, 1, 'l', 1);
		if (in->rf && in->rf->ncourse > 0) { // v0.43: the course's unlock instead of 3 of its Pokemon
			char u[48];
			routes_unlock_text(in->rf->course[i].unlock, u, sizeof(u));
			ui_text(298, my, u, 9, lk ? UI_LOCKED_TEXT : UI_SUB, 0, 'r', 1);
		}
	}
	// v0.52 (user: the "1 / 18" was "too close" to the course boxes): in the Route band, right side
	snprintf(t, sizeof(t), "%d / %d", sel + 1, n);
	ui_text(300, 17, t, 9.5f, UI_WHITE, 1, 'r', 1);
	ui_band(10, 110, 300, 22, "Train an EV on this walk", 12, 0);
	for (int k = 0; k < 7; k++) {
		float x = 14 + k * 42;
		int on = (k == 0 && ev < 0) || (k > 0 && ev == k - 1);
		ui_box(on ? UI_GLOSS3_SEL : UI_GLOSS3, x, 138, 37, 22);
		ui_text(x + 18.5f, 149, k_ev_short[k], 10, (k > 0 && !in->ev_allowed) ? C2D_Color32(170, 180, 164, 255) : UI_TEXT,
		        1, 'c', 1);
	}
	if (locked(in, sel)) { // v0.52: no sending on a locked course - the button says why
		char u[48];
		routes_unlock_text(in->rf->course[sel].unlock, u, sizeof(u));
		snprintf(t, sizeof(t), "Locked \xC2\xB7 opens %s%s", u[0] == 'A' ? "a" : "", u[0] == 'A' ? u + 1 : u);
		ui_box(UI_GLOSS3, 10, 172, 300, 32);
		lock_icon(28, 186, UI_LOCKED_TEXT);
		ui_text(164, 188, t, 11, UI_LOCKED_TEXT, 1, 'c', 1);
	} else {
		ui_box(UI_GLOSS3_SEL, 10, 172, 300, 32);
		ui_rect(13, 176, 3, 24, C2D_Color32(70, 130, 62, 255));
		ui_text(160, 188, "Send to walker", 12, UI_TEXT, 0, 'c', 1);
		ui_key(292, 188, 'A', 8);
	}
	static const char *const l[] = { "B Back" }, *const rr[] = { "X Info", "L Route", "R EV" };
	static const char *const rr_test[] = { "L Route", "R EV" };
	if (in->rf && in->rf->ncourse > 0)
		ui_footer(l, 1, rr, 3);
	else
		ui_footer(l, 1, rr_test, 2);
}

// v0.43: everything about one course (X on the send screen): picture, unlock, favoured types, all six Pokemon with
// their levels and the steps they need (v0.50: no items). B / X / A goes back.
// v0.51 (user: "they don't need the letters next to rare middle common"; "a better name for middle")
static const char *const k_group[3] = { "Rare", "Uncommon", "Common" };

// Word-wrapped text in a width (system font), up to max_lines lines.
static void wrap_text(float x, float y, float w, const char *s, float size, u32 color, int max_lines, float lh)
{
	char line[96];
	const char *p = s;
	for (int n = 0; n < max_lines && *p; n++) {
		size_t len = 0, fit = 0;
		while (p[len]) {
			size_t next = len;
			while (p[next] && p[next] != ' ')
				next++;
			if (next >= sizeof(line))
				break;
			memcpy(line, p, next);
			line[next] = 0;
			if (ui_text_width(line, size) > w && fit)
				break;
			fit = next;
			len = next;
			while (p[len] == ' ')
				len++;
			if (!p[next])
				break;
		}
		if (!fit)
			fit = strlen(p) < sizeof(line) - 1 ? strlen(p) : sizeof(line) - 1;
		memcpy(line, p, fit);
		line[fit] = 0;
		ui_text(x, y + n * lh, line, size, color, 0, 'l', 1);
		p += fit;
		while (*p == ' ')
			p++;
	}
}

static void route_info(const ui_send_info *in, const route_course *c)
{
	ui_image pic;
	u8 raw[0xC0];
	routes_picture(c->picture, raw);
	ui_image_from_walker(&pic, raw, 32, 24, 0);
	// v0.46 (user: "it should show their mini sprites and if it's not a Pokemon you have seen on the route yet it
	// should be the question mark mini sprite"): the walker's small picture of each route Pokemon; not yet caught on
	// this course (ledger) -> the "?" picture and "???" for the name.
	ui_image mini[6];
	int seen[6];
	memset(mini, 0, sizeof(mini));
	static u8 spr[WBX_SPRITE_SIZE];
	for (int k = 0; k < c->npoke && k < 6; k++) {
		seen[k] = in->seen ? in->seen(c->name, c->poke[k].species) : 1;
		if (!seen[k])
			continue; // v0.50: drawn with ui_unknown_icon instead
		wbx_sprite_load_for(c->poke[k].species, c->poke[k].form, in->gen, spr);
		ui_image_from_walker(&mini[k], spr, 32, 24, 1);
	}
	char t[96], u[48];
	routes_unlock_text(c->unlock, u, sizeof(u));
	char fav[64];
	snprintf(fav, sizeof(fav), "%s", c->favoured);
	for (char *q = fav; *q; q++)
		if (*q == ',')
			*q = '/';
	while (aptMainLoop()) {
		sound_input();
		if (hidKeysDown() & (KEY_B | KEY_X | KEY_A | KEY_TOUCH))
			break;
		if (!ui_begin())
			continue;
		ui_target_top();
		ui_status_bar("Route info");
		ui_box(UI_PANEL, 8, 22, 384, 212);
		ui_band(8, 22, 384, 20, c->name, 12, 0);
		ui_lcd_frame(20, 52, 96, 72);
		ui_lcd(20, 52, 32, 24, 3);
		ui_image_draw(&pic, 20, 52, 3);
		ui_lcd_grid(20, 52, 32, 24, 3);
		// v0.51 (user: "Favored types doesn't need to be shown either, and opens is what should be bold along with
		// About This Route"): the two headings bold, their values plain; more room for the description
		(void)fav;
		ui_text(136, 50, "Opens", 10.5f, UI_TEXT, 1, 'l', 0);
		if (in->progress_known && !routes_unlocked(c->unlock, in->badges, in->hof, in->watts)) { // v0.52
			char lu[64];
			snprintf(lu, sizeof(lu), "%s  (locked)", u);
			lock_icon(136, 68, UI_ACCENT);
			ui_text(150, 64, lu, 10.5f, UI_ACCENT, 0, 'l', 0);
		} else
			ui_text(136, 64, u, 10.5f, UI_TEXT, 0, 'l', 0);
		// v0.46: room for a route description (user: "there should be a bar for route description ... I don't have
		// them written now anyways") - the course's "desc=" text from the route file
		ui_text(136, 84, "About this route", 10.5f, UI_TEXT, 1, 'l', 0);
		if (c->desc[0])
			wrap_text(136, 105, 244, c->desc, 9.5f, UI_TEXT, 2, 12);
		else
			ui_text(136, 105, "No description yet.", 9.5f, UI_SUB, 0, 'l', 1);
		// Pokemon: one row per walker slot (C rare .. A common), two candidates each
		for (int slot = 0; slot < 3; slot++) {
			float y = 142 + slot * 30;
			if (slot) // v0.51 (user: "have lines separate each group")
				ui_rect(20, y - 7, 360, 1, C2D_Color32(176, 208, 156, 255));
			ui_text(20, y + 8, k_group[slot], 9.5f, UI_SUB, 1, 'l', 1);
			int col = 0;
			for (int k = 0; k < c->npoke && col < 2; k++) {
				const route_poke_def *p = &c->poke[k];
				if (p->slot != slot)
					continue;
				float x = 96 + col * 146;
				if (seen[k])
					ui_image_draw(&mini[k], x, y - 4, 1);
				else
					ui_unknown_icon(x, y - 8); // 32x32, same centre as the 32x24 mini sprite
				snprintf(t, sizeof(t), "%s  Lv.%d", seen[k] ? p->name : "???", p->level);
				ui_text(x + 34, y + 2, t, 10.5f, seen[k] ? UI_TEXT : UI_SUB, 1, 'l', 1);
				snprintf(t, sizeof(t), "from %d steps \xC2\xB7 %d%%", p->steps, p->chance);
				ui_text(x + 34, y + 15, t, 8.5f, UI_SUB, 0, 'l', 1);
				col++;
			}
		}
		ui_target_bottom();
		// v0.50 (user: "do not show items"): the dowsing items list is gone; the bottom screen only has the footer
		static const char *const l[] = { "B Back" };
		ui_footer(l, 1, NULL, 0);
		ui_end();
	}
	ui_image_free(&pic);
	for (int k = 0; k < 6; k++)
		ui_image_free(&mini[k]);
}

int ui_send_run(const ui_send_info *in, int *course, int *ev)
{
	ui_image big[2];
	ui_image_from_walker(&big[0], in->sprite + WBX_SPRITE_SMALL_SIZE, 64, 48, 0);
	ui_image_from_walker(&big[1], in->sprite + WBX_SPRITE_SMALL_SIZE + 0x300, 64, 48, 0);
	int animate = memcmp(in->sprite + WBX_SPRITE_SMALL_SIZE, in->sprite + WBX_SPRITE_SMALL_SIZE + 0x300, 0x300) != 0;
	int n = ncourses(in), sel = 0, first = 0, e = in->ev_allowed ? *ev : -1, frame = 0, result = 0;
	if (e < -1 || e > 5)
		e = -1;
	u64 flip = osGetTime();
	while (aptMainLoop()) {
		sound_input();
		u32 d = hidKeysDown();
		if (d & KEY_B)
			break;
		if ((d & KEY_X) && in->rf && in->rf->ncourse > 0)
			route_info(in, &in->rf->course[sel]);
		if (d & (KEY_L | KEY_DOWN))
			sel = (sel + 1) % n;
		if (d & KEY_UP)
			sel = (sel + n - 1) % n;
		if (in->ev_allowed && (d & (KEY_R | KEY_RIGHT)))
			e = e >= 5 ? -1 : e + 1;
		if (in->ev_allowed && (d & KEY_LEFT))
			e = e <= -1 ? 5 : e - 1;
		if (d & KEY_TOUCH) {
			touchPosition tp;
			hidTouchRead(&tp);
			for (int r = 0; r < ROWS && first + r < n; r++)
				if (tp.py >= ROW_Y + r * ROW_H && tp.py < ROW_Y + r * ROW_H + ROW_H - 6 && tp.px >= 14 && tp.px < 306)
					sel = first + r;
			for (int k = 0; k < 7; k++)
				if (in->ev_allowed && tp.py >= 138 && tp.py < 160 && tp.px >= 14 + k * 42 && tp.px < 51 + k * 42)
					e = k - 1;
			if (tp.py >= 172 && tp.py < 204)
				d |= KEY_A;
		}
		if (sel < first)
			first = sel;
		if (sel >= first + ROWS)
			first = sel - ROWS + 1;
		if ((d & KEY_A) && !locked(in, sel)) { // v0.52: A does nothing on a locked course
			*course = in->rf && in->rf->ncourse > 0 ? sel : -1;
			*ev = e;
			result = 1;
			break;
		}
		if (animate && osGetTime() - flip >= 500) {
			frame ^= 1;
			flip = osGetTime();
		}
		if (ui_begin()) {
			draw(in, big, frame, animate, sel, first, e);
			ui_end();
		}
	}
	ui_image_free(&big[0]);
	ui_image_free(&big[1]);
	return result;
}
