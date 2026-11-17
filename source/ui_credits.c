// ui_credits.c - Settings -> Credits (v0.46). Same content as docs/CREDITS.md (keep the two in step). Required by
// the licences of what ships in the app (CC BY 4.0 for the walker model, MIT for PokeSprite, GPL-3.0 notice).
#include <stdio.h>

#include "sound.h"
#include "ui.h"
#include "ui_credits.h"

// '#' at the start = a heading
static const char *const k_lines[] = {
	"#PocketPKMN",
	"Fan-made homebrew. Not made, endorsed or supported by",
	"Nintendo, The Pok\xC3\xA9mon Company, Game Freak or Creatures.",
	"Free software under the GNU General Public License v3.",
	"",
	"#Connect screen 3D models",
	"Pok\xC3\xA9walker: \"Pokemon Heart Gold Soul Silver Pokewalker",
	"X512\" by Callum Lax (Sketchfab), CC BY 4.0 - textures",
	"flattened, screen replaced, posed and rendered.",
	"3DS: \"Nintendo 3DS\" by Derek (3D Warehouse).",
	"",
	"#Box icons",
	"PokeSprite by Michiel Sikma and contributors (MIT).",
	"",
	"#Walker sprites",
	"Gen 1-4: Pok\xC3\xA9mon HeartGold / SoulSilver.",
	"Gen 1/2 large pictures: based on Pok\xC3\xA9mon Crystal.",
	"Gen 5: Black / White sprites (Bulbagarden Archives).",
	"Gen 6/7 and Alolan forms: Smogon Sprite",
	"Project. Spriters:",
	"AMVictory, Arkeis, aXl, Branflakes325, Bynine, Corson,",
	"Espeon Scientist, fishbowlsoul90, Galifia, GeoisEvil,",
	"G.E.Z., HealnDeal, Layell, Legitimate Username,",
	"leParagon, Mega-Pokebattlerz, MyMarshlands, N-Kin,",
	"Noscium, paintseagull, princessofmusic, Quanyails,",
	"RedRooster, Siiilver, Sleet, TheAetherPlayer,",
	"The Cynical Poet, Wobblebuns, Wyverii, Zermonious, Zerudez.",
	"Walker menu pictures and sounds: HeartGold / SoulSilver.",
	"",
	"#Music",
	"\"Pocket Stroll\": PocketPKMN's own song (v0.79),", // v0.79: tools/compose_song2.py, docs/CREDITS.md
	"composed for the app and synthesised from code.",
	"",
	"#Code",
	"PKSM-Core by FlagBrew and contributors (GPL-3.0).",
	"PKHeX.Core by Kurt (kwsch) and contributors (GPL-3.0):", // v0.68 (owner), as docs/CREDITS.md
	"catch checks; species names in other languages.",
	"libctru, citro2d, citro3d, devkitARM by devkitPro (zlib).",
	"Luma3DS csvc stub (zlib).",
	"",
	"#Fonts",
	"Banner: Inter by Rasmus Andersson (OFL 1.1).",
	"",
	"#Pok\xC3\xA9mon",
	"Pok\xC3\xA9mon and Pok\xC3\xA9walker are trademarks of Nintendo /",
	"Creatures Inc. / GAME FREAK inc.",
};
#define NLINES ((int)(sizeof(k_lines) / sizeof(k_lines[0])))
#define ROWS 14
#define ROW_H 14

void ui_credits_run(void)
{
	int first = 0;
	while (aptMainLoop()) {
		sound_input();
		u32 d = hidKeysDown(), h = hidKeysHeld();
		if (d & (KEY_B | KEY_A))
			break;
		static u64 last;
		int step = 0;
		if (d & KEY_DOWN || ((h & KEY_DOWN) && osGetTime() - last > 90))
			step = 1;
		if (d & KEY_UP || ((h & KEY_UP) && osGetTime() - last > 90))
			step = -1;
		if (d & (KEY_R | KEY_RIGHT))
			step = ROWS;
		if (d & (KEY_L | KEY_LEFT))
			step = -ROWS;
		if (step) {
			last = osGetTime();
			first += step;
			if (first > NLINES - ROWS)
				first = NLINES - ROWS;
			if (first < 0)
				first = 0;
		}
		if (!ui_begin())
			continue;
		ui_target_top();
		ui_status_bar("Credits");
		ui_box(UI_PANEL, 8, 22, 384, 212);
		for (int r = 0; r < ROWS && first + r < NLINES; r++) {
			const char *l = k_lines[first + r];
			float y = 34 + r * ROW_H;
			if (l[0] == '#')
				ui_text(20, y, l + 1, 11, UI_BAND, 1, 'l', 1);
			else
				ui_text(20, y, l, 9.5f, UI_TEXT, 0, 'l', 1);
		}
		ui_target_bottom();
		ui_band(0, 6, 320, 22, "Credits", 12, 0);
		ui_box(UI_PANEL, 16, 40, 288, 150);
		ui_text(160, 64, "Thank you to everyone listed", 11, UI_TEXT, 1, 'c', 1);
		ui_text(160, 82, "on the top screen.", 11, UI_TEXT, 1, 'c', 1);
		char t[48];
		snprintf(t, sizeof(t), "Lines %d-%d of %d", first + 1, first + ROWS < NLINES ? first + ROWS : NLINES, NLINES);
		ui_text(160, 116, t, 10, UI_SUB, 0, 'c', 1);
		ui_text(160, 146, "Source code: GNU GPL v3 - see the", 9.5f, UI_SUB, 0, 'c', 1);
		ui_text(160, 160, "LICENSE file that comes with the app.", 9.5f, UI_SUB, 0, 'c', 1);
		ui_text(312, UI_H - 12, "Up/Down: scroll", 9, UI_SUB, 0, 'r', 1);
		static const char *const l[] = { "B Back" };
		ui_footer(l, 1, NULL, 0);
		ui_end();
	}
}
