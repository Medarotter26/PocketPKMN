// ui_result.c - End walk result screen: the Pokemon that is back, what the walk gave it, what it found.
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ui_result.h"

static const char *const k_ev_long[6] = { "HP", "Attack", "Defense", "Sp. Atk", "Sp. Def", "Speed" };

// v0.50 (user: "The pepperoni is back screen seems kind of empty the font seems a bit small"): larger facts
static void fact(float x, float y, const char *k, const char *v)
{
	ui_text(x, y, k, 10, UI_SUB, 0, 'l', 0);
	ui_text(x, y + 12, v, 14, UI_TEXT, 1, 'l', 0);
}

void ui_result_run(const ui_result_info *r)
{
	ui_image big[2], cname[3], csmall[3], iname[13];
	memset(cname, 0, sizeof(cname));
	memset(csmall, 0, sizeof(csmall));
	memset(iname, 0, sizeof(iname));
	ui_image_from_walker(&big[0], r->sprite + WBX_SMALL_SIZE, 64, 48, 0);
	ui_image_from_walker(&big[1], r->sprite + WBX_SMALL_SIZE + 0x300, 64, 48, 0);
	int animate = memcmp(r->sprite + WBX_SMALL_SIZE, r->sprite + WBX_SMALL_SIZE + 0x300, 0x300) != 0;
	// v0.50: names are text now (caught_text / item_text); the walker's name pictures are not drawn any more
	for (int i = 0; i < r->ncaught && i < 3; i++)
		if (r->caught_small[i])
			ui_image_from_walker(&csmall[i], r->caught_small[i], 32, 24, 1);
	int frame = 0, first_row = 0;
	u64 flip = osGetTime();
	char t[96];
	int nc = r->ncaught < 3 ? r->ncaught : 3; // a walker holds at most 3 caught Pokemon
	int ni = r->nitems < 13 ? r->nitems : 13; // 3 dowsed + up to 10 gifts
	// items panel: two names per row, scrolled with the D-pad when they do not all fit
	float iy = nc ? 140 : 36;
	int rows_vis = (int)((206 - iy - 28) / 18), rows = (ni + 1) / 2;
	while (aptMainLoop()) {
		hidScanInput();
		u32 d = hidKeysDown();
		if (d & (KEY_A | KEY_B | KEY_TOUCH))
			break;
		if ((d & KEY_DOWN) && first_row + rows_vis < rows)
			first_row++;
		if ((d & KEY_UP) && first_row > 0)
			first_row--;
		if (animate && osGetTime() - flip >= 500) {
			frame ^= 1;
			flip = osGetTime();
		}
		if (!ui_begin())
			continue;
		ui_target_top();
		ui_status_bar("End walk");
		ui_box(UI_PANEL, 8, 22, 384, 210);
		snprintf(t, sizeof(t), "%s is back!", r->nick);
		ui_band(8, 22, 384, 20, t, 12, 1);
		ui_lcd_frame(24, 56, 128, 96);
		ui_lcd(24, 56, 64, 48, 2);
		ui_image_draw(&big[animate ? frame : 0], 24, 56, 2);
		ui_lcd_grid(24, 56, 64, 48, 2);
		snprintf(t, sizeof(t), "%u", r->steps);
		fact(172, 52, "Steps on this walk", t);
		snprintf(t, sizeof(t), "%u", r->watts);
		fact(290, 52, "Watts", t);
		int ev_shown = 0;
		if (r->gains) {
			if (r->level_after > r->level_before)
				snprintf(t, sizeof(t), "%u \xE2\x86\x92 %u!", r->level_before, r->level_after);
			else
				snprintf(t, sizeof(t), "%u", r->level_after);
			fact(172, 90, "Level", t);
			snprintf(t, sizeof(t), "+%u", r->exp_gain);
			fact(290, 90, "EXP", t);
			if (r->has_friendship) {
				snprintf(t, sizeof(t), "%u \xE2\x86\x92 %u", r->friend_before, r->friend_after);
				fact(172, 128, "Friendship", t);
			}
			if (r->ev_stat >= 0 && r->ev_stat < 6) {
				snprintf(t, sizeof(t), "%u \xE2\x86\x92 %u", r->ev_before, r->ev_after);
				char k[32];
				snprintf(k, sizeof(k), "%s EVs", k_ev_long[r->ev_stat]);
				fact(290, 128, k, t);
				ev_shown = 1;
			}
		}
		if (!ev_shown) { // what the walk brought (the bottom screen lists it)
			snprintf(t, sizeof(t), "%d / %d", nc, ni);
			fact(290, 128, "Pok\xC3\xA9mon / items", t);
		}
		// the "back in its game" message, line by line, under the picture
		ui_rect(20, 172, 360, 1, C2D_Color32(150, 180, 140, 160));
		char buf[160];
		snprintf(buf, sizeof(buf), "%s", r->back);
		// v0.53 (user: "make sure to have a message explaining where items/pokemon are sent to"): under the Pokemon's
		// own line, where its catches and items went
		int extra = (r->sent_line[0][0] != 0) + (r->sent_line[1][0] != 0);
		float y = extra ? 184 : 190;
		for (char *s = strtok(buf, "\n"); s && y < 228; s = strtok(NULL, "\n"), y += 17)
			ui_text(200, y, s, extra ? 11.5f : 12.5f, UI_TEXT, 1, 'c', 1);
		for (int i = 0; i < 2 && y < 232; i++)
			if (r->sent_line[i][0]) {
				ui_text(200, y, r->sent_line[i], 10, UI_SUB, 0, 'c', 1);
				y += 14;
			}

		ui_target_bottom();
		ui_band(0, 6, 320, 22, "Found on the walk", 12, 0);
		// v0.50 (user: "I don't really like the found on the walk screen. The Pokemon mini sprite box is also missing
		// its border and lcd grid"): a card per caught Pokemon (framed walker screen at 2x with the LCD grid, name,
		// level), then the items in their own panel
		if (nc) {
			const float cw = 96, gap = 8;
			float x0 = 160 - (nc * cw + (nc - 1) * gap) / 2;
			for (int i = 0; i < nc; i++) {
				float x = x0 + i * (cw + gap);
				ui_box(UI_PANEL, x, 36, cw, 98);
				ui_lcd_frame(x + 16, 46, 64, 48);
				ui_lcd(x + 16, 46, 32, 24, 2);
				if (csmall[i].ok)
					ui_image_draw(&csmall[i], x + 16, 46, 2);
				ui_lcd_grid(x + 16, 46, 32, 24, 2);
				if (r->caught_text[i][0])
					snprintf(t, sizeof(t), "%s", r->caught_text[i]);
				else
					snprintf(t, sizeof(t), "#%u", r->caught_species[i]);
				ui_text(x + cw / 2, 110, t, 10.5f, UI_TEXT, 1, 'c', 1);
				snprintf(t, sizeof(t), "Lv.%u", r->caught_level[i]); // v0.50 (user: no "caught")
				ui_text(x + cw / 2, 124, t, 9, UI_SUB, 0, 'c', 1);
			}
		}
		if (ni) {
			// v0.50 (user: "I don't like the text font in the item box"; "you should be able to scroll with your dpad
			// if it's too many"): names in the app's font, two per row, D-pad up/down scrolls
			ui_box(UI_PANEL, 10, iy, 300, 206 - iy);
			snprintf(t, sizeof(t), "Items (%d)", ni);
			ui_text(22, iy + 12, t, 9.5f, UI_SUB, 1, 'l', 1);
			for (int row = 0; row < rows_vis && first_row + row < rows; row++)
				for (int c = 0; c < 2; c++) {
					int i = (first_row + row) * 2 + c;
					if (i >= ni)
						break;
					if (r->item_text[i][0])
						snprintf(t, sizeof(t), "%s", r->item_text[i]);
					else
						snprintf(t, sizeof(t), "Item #%u", r->item[i]);
					float x = c ? 164 : 24, ry = iy + 32 + row * 18;
					ui_rect(x, ry - 2, 4, 4, UI_BAND);
					ui_text(x + 9, ry, t, 11, UI_TEXT, 1, 'l', 1);
				}
			if (first_row > 0)
				ui_arrow(296, iy + 12, 'u', 4, UI_SUB);
			if (first_row + rows_vis < rows)
				ui_arrow(296, 198, 'd', 4, UI_SUB);
		}
		if (!nc && !ni) {
			ui_box(UI_PANEL, 10, 36, 300, 170);
			ui_text(160, 120, "Nothing this time.", 11, UI_SUB, 0, 'c', 1);
		}
		static const char *const rr[] = { "A OK" };
		if (first_row > 0 || rows_vis < rows) {
			static const char *const l2[] = { "+ Scroll" }; // "+" in the key circle = the D-pad
			ui_footer(l2, 1, rr, 1);
		} else
			ui_footer(NULL, 0, rr, 1);
		ui_end();
	}
	ui_image_free(&big[0]);
	ui_image_free(&big[1]);
	for (int i = 0; i < 3; i++) {
		ui_image_free(&cname[i]);
		ui_image_free(&csmall[i]);
	}
	for (int i = 0; i < 13; i++)
		ui_image_free(&iname[i]);
}
