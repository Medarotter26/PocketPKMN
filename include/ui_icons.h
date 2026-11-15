// ui_icons.h - colour box icons (Gen 7 style, PokeSprite; docs/CREDITS_ICONS.md) from romfs:/icons/icons.bin
// (tools/make_icons.py). Each icon is a UI_ICON_W x UI_ICON_H picture whose bottom line is the icon's feet, as
// in the game's box; draw it centred on the slot. Reads romfs: never call during IR (box view and Home only).
#ifndef PKPK_UI_ICONS_H
#define PKPK_UI_ICONS_H

#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_ICON_W 50
#define UI_ICON_H 30

// Keep the icon file open for several loads (one box); optional - ui_icon_load opens it itself if needed.
void ui_icons_begin(void);
void ui_icons_end(void);

// species 1-807, form = the save's form number, female = 1 for a female, gen = the save's generation (a few form
// numbers differ in Gen 4/6). Unknown forms -> the species' base icon. Returns 0 (im not ok) if there is none.
int ui_icon_load(ui_image *im, int species, int form, int female, int gen);

// Draws a loaded icon centred on (cx, cy).
void ui_icon_draw(const ui_image *im, float cx, float cy);

// Neutral stand-in for a missing icon (a grey "?"), centred on (cx, cy). Never the walker sprite (user).
void ui_icon_placeholder(float cx, float cy);

#ifdef __cplusplus
}
#endif

#endif
