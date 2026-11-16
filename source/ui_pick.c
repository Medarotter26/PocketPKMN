// ui_pick.c - choosing the Pokemon for a walk on GPU screens (v0.37): game select (mock_2_game_select),
// "Not yet compatible" (mock_2b), the game's boxes + the Pokemon's details (mock_3a), confirm, then the same
// storage step as before (storage_pick_commit = the old pickInGame A branch). Never runs during IR.
//
// Box icons: the Gen 6/7 colour icons only (source/ui_icons.c, user 2026-11-15); a "?" placeholder if one is missing.
#include <stdio.h>
#include <string.h>

#include "storage.h"
#include "sound.h"
#include "ui.h"
#include "ui_icons.h"
#include "ui_pick.h"
#include "wbx_sprites.h"

#define MAX_GAMES 24
#define MAX_SLOTS 30

static storage_game_info games[MAX_GAMES];
static ui_title icons[MAX_GAMES];
static int ngames;

static storage_box_mon mons[MAX_SLOTS];
static ui_image box_img[MAX_SLOTS];
static int per_box, nboxes;
static char box_name[32];

static ui_image big[2], small_cur;
static int big_species = -1, big_form = -1, big_gen = -1, big_animate;

// v0.61: 3 = a DS save file on the SD card (TWiLight Menu++; user: "show up different from cartridges")
static const char *const k_kind[4] = { "Virtual Console", "Game card", "Game Boy Advance", "SD card (TWiLight Menu)" };
static const char *const k_ev[6] = { "HP", "Atk", "Def", "SpA", "SpD", "Spe" };

static void free_box(void)
{
	for (int i = 0; i < MAX_SLOTS; i++)
		ui_image_free(&box_img[i]);
}

static void free_big(void)
{
	ui_image_free(&big[0]);
	ui_image_free(&big[1]);
	ui_image_free(&small_cur);
	big_species = -1;
}

static void load_big(int species, int form, int gen)
{
	if (species == big_species && form == big_form && gen == big_gen)
		return;
	free_big();
	static u8 spr[WBX_SPRITE_SIZE];
	wbx_sprite_load_for(species, gen == 2 ? form : 0, gen, spr); // Gen 1/2 games: the Gen 1/2 set
	ui_image_from_walker(&big[0], spr + WBX_SPRITE_SMALL_SIZE, 64, 48, 0);
	ui_image_from_walker(&big[1], spr + WBX_SPRITE_SMALL_SIZE + 0x300, 64, 48, 0);
	ui_image_from_walker(&small_cur, spr, 32, 24, 0);
	big_animate = memcmp(spr + WBX_SPRITE_SMALL_SIZE, spr + WBX_SPRITE_SMALL_SIZE + 0x300, 0x300) != 0;
	big_species = species;
	big_form = form;
	big_gen = gen;
}

static int load_box(int g, int box)
{
	free_box();
	nboxes = storage_game_box(g, box, mons, MAX_SLOTS, box_name, sizeof(box_name));
	if (nboxes < 0)
		return 0;
	per_box = games[g].per_box > MAX_SLOTS ? MAX_SLOTS : games[g].per_box;
	ui_icons_begin();
	for (int i = 0; i < per_box; i++)
		if (mons[i].present && !mons[i].egg)
			ui_icon_load(&box_img[i], mons[i].species, mons[i].form, mons[i].gender == 1, games[g].gen);
	ui_icons_end();
	return 1;
}

// A slot's box icon centred on (cx, cy). User: in-app box icons are only the Gen 6/7 ones (the walker's small
// sprites are shown only on the walker views) -> a missing icon is a neutral "?" placeholder.
static void slot_icon(int i, float cx, float cy)
{
	if (box_img[i].ok)
		ui_icon_draw(&box_img[i], cx, cy);
	else
		ui_icon_placeholder(cx, cy);
}

// --- shared pieces ---------------------------------------------------------------------------------------------
static void game_icon(int g, float x, float y, float size)
{
	if (g >= 0 && g < ngames && icons[g].ok)
		ui_image_draw(&icons[g].icon, x, y, size / 48.0f);
	else
		ui_box(UI_GAMEICON, x, y, size, size);
}

static void bullet(float x, float y, const char *t)
{
	ui_rect(x, y - 2.5f, 5, 5, UI_BAND);
	ui_text(x + 9, y, t, 11, UI_TEXT, 0, 'l', 1);
}

static void main_button(float x, float y, float w, float h, const char *t)
{
	ui_box(UI_GLOSS3_SEL, x, y, w, h);
	ui_rect(x + 3, y + 4, 3, h - 8, C2D_Color32(70, 130, 62, 255));
	ui_text(x + w / 2, y + h / 2, t, 12, UI_TEXT, 0, 'c', 1);
	ui_key(x + w - 15, y + h / 2, 'A', 8);
}

// A dialog over a dimmed bottom screen (mock_2b): red or green band, centred lines, one OK-style button.
static void popup(const char *title, int red, const char *const *lines, int n, const char *sub, const char *button)
{
	ui_rect(0, 0, UI_BOT_W, UI_H, C2D_Color32(20, 36, 16, 110));
	float h = 70 + n * 16 + (sub ? 14 : 0);
	float y = (UI_H - 24 - h) / 2;
	ui_box(UI_PANEL, 30, y, 260, h);
	ui_band(30, y, 260, 22, title, 12, red);
	for (int i = 0; i < n; i++)
		ui_text(160, y + 36 + i * 16, lines[i], 11, UI_TEXT, 0, 'c', 1);
	if (sub)
		ui_text(160, y + 36 + n * 16, sub, 9.5f, UI_SUB, 0, 'c', 1);
	main_button(85, y + h - 34, 150, 26, button);
}

// --- game select -----------------------------------------------------------------------------------------------
static void draw_game_select(int sel, int not_compatible)
{
	const storage_game_info *g = &games[sel];
	ui_target_top();
	ui_status_bar("Send to walker");
	if (ngames > 1) {
		int prev = (sel + ngames - 1) % ngames, next = (sel + 1) % ngames;
		ui_box(UI_GAMEICON, 111, 46, 34, 34);
		game_icon(prev, 111, 46, 34);
		ui_box(UI_GAMEICON, 271, 46, 34, 34);
		game_icon(next, 271, 46, 34);
	}
	ui_box(UI_GLOSS6_SEL, 178, 32, 60, 60);
	game_icon(sel, 184, 38, 48);
	ui_band(0, 98, UI_TOP_W, 22, g->name, 12, 1);
	ui_box(UI_PANEL, 20, 127, 376, 59);
	char t[64];
	snprintf(t, sizeof(t), "Trainer: %s", g->trainer);
	bullet(30, 142, t);
	snprintf(t, sizeof(t), "ID No. %05u", g->tid);
	bullet(218, 142, t);
	bullet(30, 166, k_kind[g->kind < 0 || g->kind > 3 ? 0 : g->kind]);
	snprintf(t, sizeof(t), "Generation %d", g->gen);
	bullet(218, 166, t);
	ui_box(UI_PANEL, 20, 195, 376, 40);
	ui_text(32, 208, "Choose the game whose Pok\xC3\xA9mon goes for a walk.", 11, UI_TEXT, 0, 'l', 1);
	snprintf(t, sizeof(t), "L / R: other games (%d found)", ngames);
	ui_text(32, 224, t, 9.5f, UI_SUB, 0, 'l', 1);

	ui_target_bottom();
	main_button(70, 92, 276 - 70 + 4, 38, "Use this game");
	static const char *const l[] = { "B Back" }, *const r[] = { "L Previous", "R Next" };
	ui_footer(l, 1, r, 2);
	if (not_compatible) {
		char a[80];
		snprintf(a, sizeof(a), "%s has no walker", g->name);
		const char *lines[3] = { a, "routes yet, so its Pok\xC3\xA9mon", "can't go for a walk." };
		popup("Not yet compatible", 1, lines, 3, "Routes come one game at a time.", "OK");
	}
}

// --- box view ----------------------------------------------------------------------------------------------------
static void slot_pos(int i, float *cx, float *cy)
{
	*cx = 60 + (i % 6) * 40; // v0.49: centred (user: "Try removing the bar and then centering the box")
	*cy = 52 + (i / 6) * 31;
}

static void gauge(float x, float y, float w, const char *name, const char *val, float frac, u32 col)
{
	ui_text(x, y, name, 9, UI_SUB, 0, 'l', 1);
	ui_text(x + w, y, val, 9, UI_TEXT, 1, 'r', 1);
	ui_rect(x, y + 6, w, 5, C2D_Color32(222, 232, 214, 255));
	if (frac > 0)
		ui_rect(x, y + 6, w * (frac > 1 ? 1 : frac) < 3 ? 3 : w * (frac > 1 ? 1 : frac), 5, col);
}

static void draw_mon_top(int g, const storage_box_mon *m, int frame)
{
	const storage_game_info *gi = &games[g];
	char t[96];
	ui_target_top();
	snprintf(t, sizeof(t), "%s \xC2\xB7 %s", gi->name, gi->trainer);
	ui_status_bar(t);
	// left: walker views + game
	ui_box(UI_PANEL, 9, 23, 187, 210);
	// v0.51 (user: "Where's the pixel grid?"; "remove the rest and the box icon and just place the walker icon under
	// the big sprite but bring the big sprite down some"; "I'd like the game and player name to be shown still"):
	// the big walker screen, the walker icon at 2x under it (both with the LCD grid), the game line at the bottom
	ui_lcd_frame(38, 38, 128, 96);
	ui_lcd(38, 38, 64, 48, 2);
	if (m && m->present && !m->egg)
		ui_image_draw(&big[big_animate ? frame : 0], 38, 38, 2);
	ui_lcd_grid(38, 38, 64, 48, 2);
	ui_lcd_frame(70, 152, 64, 48);
	ui_lcd(70, 152, 32, 24, 2);
	if (m && m->present && !m->egg)
		ui_image_draw(&small_cur, 70, 152, 2);
	ui_lcd_grid(70, 152, 32, 24, 2);
	game_icon(g, 18, 210, 18);
	ui_text(42, 214, gi->name, 9.5f, UI_TEXT, 1, 'l', 1);
	snprintf(t, sizeof(t), "%s \xC2\xB7 ID %05u", gi->trainer, gi->tid);
	ui_text(42, 225, t, 8, UI_SUB, 0, 'l', 1);
	// right: facts
	ui_box(UI_PANEL, 204, 23, 188, 210);
	if (!m || !m->present) {
		ui_band(204, 23, 188, 21, "Empty slot", 12, 0);
		return;
	}
	if (m->egg) {
		ui_band(204, 23, 188, 21, "Egg", 12, 0);
		ui_text(298, 80, "An egg cannot go for a walk.", 10, UI_SUB, 0, 'c', 1);
		return;
	}
	snprintf(t, sizeof(t), "%s  Lv.%d", m->species_name, m->level);
	ui_band(204, 23, 188, 21, t, 12, 0);
	if (m->gender < 2)
		ui_text(384, 34, m->gender ? "\xE2\x99\x80" : "\xE2\x99\x82", 12,
		        m->gender ? C2D_Color32(240, 150, 170, 255) : C2D_Color32(130, 200, 240, 255), 1, 'r', 1);
	float x = 214, x2 = 304, w = 168;
	ui_text(x, 52, "Nickname", 8.5f, UI_SUB, 0, 'l', 0);
	ui_text(x, 61, m->nickname, 10, UI_TEXT, 1, 'l', 0);
	ui_text(x2, 52, "Dex No.", 8.5f, UI_SUB, 0, 'l', 0);
	snprintf(t, sizeof(t), "%03d", m->species);
	ui_text(x2, 61, t, 10, UI_TEXT, 1, 'l', 0);
	ui_text(x, 76, "Nature", 8.5f, UI_SUB, 0, 'l', 0);
	ui_text(x, 85, m->nature[0] ? m->nature : "-", 10, UI_TEXT, 1, 'l', 0);
	ui_text(x2, 76, "Ability", 8.5f, UI_SUB, 0, 'l', 0);
	ui_text(x2, 85, m->ability[0] ? m->ability : "-", 10, UI_TEXT, 1, 'l', 0);
	snprintf(t, sizeof(t), "%u", m->friendship);
	gauge(x, 105, w, "Friendship", t, m->friendship / 255.0f, UI_BAND);
	if (m->exp_level_span) {
		snprintf(t, sizeof(t), "%u EXP", m->exp_to_next);
		gauge(x, 127, w, "Next level", t, (float)m->exp_into_level / m->exp_level_span, C2D_Color32(66, 130, 200, 255));
	} else {
		gauge(x, 127, w, "Next level", "max", 1, C2D_Color32(66, 130, 200, 255));
	}
	if (!m->has_evs) {
		ui_text(x, 152, "No EVs in Gen 1/2 (stat experience)", 9, UI_SUB, 0, 'l', 1);
		return;
	}
	int total = 0;
	for (int i = 0; i < 6; i++)
		total += m->evs[i];
	ui_text(x, 148, "EVs", 9, UI_SUB, 0, 'l', 1);
	snprintf(t, sizeof(t), "%d / 510", total);
	ui_text(x + w, 148, t, 9, UI_TEXT, 1, 'r', 1);
	for (int i = 0; i < 6; i++) {
		float yy = 160 + i * 11;
		ui_text(x, yy, k_ev[i], 8.5f, UI_SUB, 1, 'l', 1);
		float bx = x + 26, bw = w - 48;
		ui_rect(bx, yy - 2.5f, bw, 5, C2D_Color32(222, 232, 214, 255));
		if (m->evs[i])
			ui_rect(bx, yy - 2.5f, bw * m->evs[i] / 252.0f < 3 ? 3 : bw * m->evs[i] / 252.0f, 5, UI_BAND);
		snprintf(t, sizeof(t), "%d", m->evs[i]);
		ui_text(x + w, yy, t, 8.5f, UI_TEXT, 1, 'r', 1);
	}
}

static void draw_box_bottom(int g, int box, int cur)
{
	char t[80];
	ui_target_bottom();
	snprintf(t, sizeof(t), "%s \xC2\xB7 %s", games[g].name, box_name[0] ? box_name : "Box");
	if (!box_name[0])
		snprintf(t, sizeof(t), "%s \xC2\xB7 Box %d", games[g].name, box + 1);
	ui_band(60, 6, 200, 22, t, 11, 0);
	// v0.49: drawn triangles (the system font has no arrow characters: they showed as "?")
	ui_arrow(48, 17, 'l', 5, UI_BAND);
	ui_arrow(272, 17, 'r', 5, UI_BAND);
	if (cur >= 0 && cur < per_box) { // v0.51: around the whole 50 x 30 icon, drawn first so no icon is covered
		float sx, sy;
		slot_pos(cur, &sx, &sy);
		ui_box(UI_GLOSS3_SEL, sx - 26, sy - 18, 52, 36);
	}
	for (int i = 0; i < per_box; i++) {
		float cx, cy;
		slot_pos(i, &cx, &cy);
		if (!mons[i].present)
			ui_rect(cx - 1.5f, cy - 1.5f, 3, 3, C2D_Color32(150, 196, 120, 200));
		else if (mons[i].egg)
			ui_text(cx, cy, "Egg", 9, UI_SUB, 1, 'c', 1);
		else
			slot_icon(i, cx, cy);
	}
	static const char *const l[] = { "B Back" }, *const r[] = { "X Games", "A Send" };
	ui_footer(l, 1, r, 2);
}

// 1 = yes (A), 0 = no (B); the box view stays behind the dialog.
static int confirm_send(int g, int box, int cur, const storage_box_mon *m)
{
	char a[64];
	snprintf(a, sizeof(a), "Send %s (Lv.%d) for a walk?", m->nickname, m->level);
	const char *lines[3] = { a, "It moves out of the game into the", "walker slot and comes back after." };
	while (aptMainLoop()) {
		sound_input();
		u32 d = hidKeysDown();
		if (d & KEY_A)
			return 1;
		if (d & KEY_B)
			return 0;
		if (ui_begin()) {
			draw_mon_top(g, m, 0);
			draw_box_bottom(g, box, cur);
			popup("Send to walker", 0, lines, 3, "B: no", "Yes");
			ui_end();
		}
	}
	return 0;
}

static void message(int g, int box, int cur, const char *title, const char *msg)
{
	char buf[256];
	snprintf(buf, sizeof(buf), "%s", msg);
	const char *lines[6];
	int n = 0;
	for (char *s = strtok(buf, "\n"); s && n < 6; s = strtok(NULL, "\n"))
		lines[n++] = s;
	while (aptMainLoop()) {
		sound_input();
		if (hidKeysDown() & (KEY_A | KEY_B))
			return;
		if (ui_begin()) {
			draw_mon_top(g, &mons[cur], 0);
			draw_box_bottom(g, box, cur);
			popup(title, 1, lines, n, NULL, "OK");
			ui_end();
		}
	}
}

// Box view of game g. Returns 1 = moved into the walker slot, 0 = back to the game list.
static int box_view(int g)
{
	int box = 0, cur = 0, frame = 0;
	u64 flip = osGetTime();
	if (!load_box(g, box))
		return 0;
	while (aptMainLoop()) {
		sound_input();
		u32 d = hidKeysDown();
		if (d & (KEY_B | KEY_X))
			return 0;
		int rows = (per_box + 5) / 6;
		if (d & KEY_RIGHT)
			cur = (cur % 6 == 5 || cur + 1 >= per_box) ? cur - cur % 6 : cur + 1;
		if (d & KEY_LEFT)
			cur = cur % 6 == 0 ? (cur + 5 < per_box ? cur + 5 : per_box - 1) : cur - 1;
		if (d & KEY_DOWN)
			cur = (cur + 6 < per_box) ? cur + 6 : cur % 6;
		if (d & KEY_UP)
			cur = cur >= 6 ? cur - 6 : ((rows - 1) * 6 + cur < per_box ? (rows - 1) * 6 + cur : cur);
		if ((d & (KEY_R | KEY_L)) && nboxes > 0) {
			box = (box + ((d & KEY_R) ? 1 : nboxes - 1)) % nboxes;
			load_box(g, box);
		}
		if (d & KEY_TOUCH) {
			touchPosition tp;
			hidTouchRead(&tp);
			for (int i = 0; i < per_box; i++) {
				float cx, cy;
				slot_pos(i, &cx, &cy);
				if (tp.px >= cx - 18 && tp.px < cx + 18 && tp.py >= cy - 14 && tp.py < cy + 14)
					cur = i;
			}
			if (tp.py < 30 && tp.px < 60 && nboxes > 0) { // audit M13
				box = (box + nboxes - 1) % nboxes;
				load_box(g, box);
			} else if (tp.py < 30 && tp.px > 260 && nboxes > 0) {
				box = (box + 1) % nboxes;
				load_box(g, box);
			}
		}
		const storage_box_mon *m = &mons[cur];
		if (m->present && !m->egg)
			load_big(m->species, m->form, games[g].gen);
		if (d & KEY_A) {
			if (!m->present)
				message(g, box, cur, "Send to walker", "That slot is empty.");
			else if (m->egg)
				message(g, box, cur, "Send to walker", "An egg cannot go for a walk.");
			else if (confirm_send(g, box, cur, m)) {
				char msg[200] = "";
				if (storage_pick_commit(g, box, cur, msg, sizeof(msg)))
					return 1;
				message(g, box, cur, "Could not send", msg);
				load_box(g, box);
			}
		}
		if (big_animate && osGetTime() - flip >= 500) {
			frame ^= 1;
			flip = osGetTime();
		}
		if (ui_begin()) {
			draw_mon_top(g, m, frame);
			draw_box_bottom(g, box, cur);
			ui_end();
		}
	}
	return 0;
}

static void release_all(void)
{
	free_box();
	free_big();
	for (int i = 0; i < ngames; i++)
		ui_title_free(&icons[i]);
	ngames = 0;
	storage_games_release();
}

int ui_pick_for_walk(char *why, int why_size)
{
	why[0] = 0;
	ui_loading("Looking for your games…"); // v0.43: the scan reads every save - show something first
	ngames = storage_games_scan(games, MAX_GAMES, why, why_size);
	if (ngames <= 0) {
		if (ngames == 0)
			snprintf(why, (size_t)why_size, "No Pokemon save found.\n(Has the game been saved at least once?)");
		release_all();
		return -1;
	}
	for (int i = 0; i < ngames; i++)
		ui_title_load(&icons[i], games[i].title_id);
	int sel = 0, nc = 0;
	while (aptMainLoop()) {
		sound_input();
		u32 d = hidKeysDown();
		if (nc) { // the Not yet compatible dialog is open
			if (d & (KEY_A | KEY_B))
				nc = 0;
		} else {
			if (d & KEY_B)
				break;
			if (d & (KEY_R | KEY_RIGHT))
				sel = (sel + 1) % ngames;
			if (d & (KEY_L | KEY_LEFT))
				sel = (sel + ngames - 1) % ngames;
			if (d & KEY_TOUCH) {
				touchPosition tp;
				hidTouchRead(&tp);
				if (tp.px >= 70 && tp.px < 350 && tp.py >= 92 && tp.py < 130)
					d |= KEY_A;
			}
			if (d & KEY_A) {
				if (!games[sel].has_routes)
					nc = 1;
				else if (box_view(sel)) {
					release_all();
					return 1;
				}
			}
		}
		if (ui_begin()) {
			draw_game_select(sel, nc);
			ui_end();
		}
	}
	release_all();
	return 0;
}

// --- v0.43: the walker slot holds a Pokemon back from a walk (was a text screen) -----------------------------------
static void draw_waiting(const ui_image *big, const char *label, const char *const *lines, int n, const char *title,
                         int footer_choice)
{
	ui_target_top();
	ui_status_bar("Send to walker");
	ui_lcd_frame(138, 34, 128, 96);
	ui_lcd(138, 34, 64, 48, 2);
	if (big && big->ok)
		ui_image_draw(big, 138, 34, 2);
	ui_lcd_grid(138, 34, 64, 48, 2);
	ui_box(UI_PANEL, 20, 160, 376, 66);
	ui_text(208, 178, label, 12, UI_TEXT, 1, 'c', 1);
	ui_text(208, 198, "is back from a walk. Its game was not", 10.5f, UI_SUB, 0, 'c', 1);
	ui_text(208, 213, "found then, so it waits in the walker slot.", 10.5f, UI_SUB, 0, 'c', 1);
	ui_target_bottom();
	ui_rect(0, 0, UI_BOT_W, UI_H, C2D_Color32(20, 36, 16, 60));
	float h = 46 + n * 16;
	float y = (UI_H - 24 - h) / 2;
	ui_box(UI_PANEL, 24, y, 272, h);
	ui_band(24, y, 272, 22, title, 12, 0);
	for (int i = 0; i < n; i++)
		ui_text(160, y + 36 + i * 16, lines[i], 11, UI_TEXT, 0, 'c', 1);
	if (footer_choice) {
		static const char *const l[] = { "B Back" }, *const r[] = { "X Put back", "A Send again" };
		ui_footer(l, 1, r, 2);
	} else {
		static const char *const r[] = { "A OK" };
		ui_footer(NULL, 0, r, 1);
	}
}

int ui_slot_waiting(const unsigned char *sprite, const char *label,
                    int (*put_back)(char *msg, int size))
{
	ui_image big;
	memset(&big, 0, sizeof(big));
	if (sprite)
		ui_image_from_walker(&big, sprite + WBX_SPRITE_SMALL_SIZE, 64, 48, 0);
	const char *ask[2] = { "Send it on another walk,", "or try to put it back in its game?" };
	int result = 0;
	while (aptMainLoop()) {
		sound_input();
		u32 d = hidKeysDown();
		if (d & KEY_B)
			break;
		if (d & KEY_A) {
			result = 1;
			break;
		}
		if (d & KEY_X) {
			char m[200] = "";
			put_back(m, sizeof(m));
			const char *lines[6];
			int n = 0;
			for (char *s = strtok(m, "\n"); s && n < 6; s = strtok(NULL, "\n"))
				lines[n++] = s;
			while (aptMainLoop()) {
				sound_input();
				if (hidKeysDown() & (KEY_A | KEY_B))
					break;
				if (ui_begin()) {
					draw_waiting(&big, label, lines, n, "Put it back", 0);
					ui_end();
				}
			}
			break;
		}
		if (ui_begin()) {
			draw_waiting(&big, label, ask, 2, "The walker slot is in use", 1);
			ui_end();
		}
	}
	ui_image_free(&big);
	return result;
}
