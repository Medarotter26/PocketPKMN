// ui.h - WalkerBox's GPU screens (citro2d), v0.32+. Spec: tools/ui_mockups.py (drawn at 1x here).
//
// The old text consoles are kept, but they write into RAM ("text view") instead of the screens, so every flow
// that still prints works word for word; ui_legacy_present() shows that text inside the new frame.
//
// IR rule (docs/UI_PLAN.md section 3): between ui_set_ir_busy(1) and ui_set_ir_busy(0) nothing here touches the
// SD card or romfs, and a frame never waits for the GPU or vblank (a busy GPU = that update is skipped).
#ifndef WBX_UI_H
#define WBX_UI_H

#include <3ds.h>
#include <citro2d.h>

#ifdef __cplusplus
extern "C" {
#endif

// Font switch: 0 = the 3DS system font (no file), 1 = Console Square Beta (needs the user's OK on its licence;
// not built in until then).
#define UI_FONT_CONSOLE_SQUARE 0

#define UI_TOP_W 400
#define UI_BOT_W 320
#define UI_H     240

// palette (tools/ui_mockups.py)
#define UI_TEXT    C2D_Color32(40, 60, 36, 255)
#define UI_SUB     C2D_Color32(96, 120, 88, 255)
#define UI_ACCENT  C2D_Color32(226, 72, 40, 255)
#define UI_BAND    C2D_Color32(58, 112, 50, 255)
#define UI_WHITE   C2D_Color32(255, 255, 255, 255)
#define UI_LCD_BG  C2D_Color32(196, 206, 168, 255)

// Start-up: gfx must be initialised. Sets up citro2d, the pieces, the font and the two RAM consoles.
void ui_init(PrintConsole *top, PrintConsole *bottom);
void ui_exit(void);

// --- frames -------------------------------------------------------------------------------------------
// ui_begin returns 0 if no frame may be drawn now (IR busy and the GPU still working): skip drawing then.
int ui_begin(void);
void ui_end(void);
void ui_target_top(void);    // draw on the top screen (background drawn)
void ui_target_bottom(void); // draw on the bottom screen (background drawn)

void ui_set_ir_busy(int busy); // logs the frame stats of the IR time when it ends

// v0.47: the background (Settings -> Background): 0 Classic, 1 Poke Ball pattern, 2 Walker screen, 3 Route scene.
#define UI_BACKGROUNDS 4
void ui_set_background(int index);
int ui_background(void);
const char *ui_background_name(int index);
int ui_background_moves(void); // v0.48: 1-3 scroll

// One frame on both screens: a small panel with this text (v0.43: shown before slow work such as the game scan, so
// the app does not look frozen). Not for IR time.
void ui_loading(const char *text);

// --- text view (old consoles in RAM) -----------------------------------------------------------------
void ui_legacy_present(void);          // = the old refresh(): show both consoles' text
void ui_legacy_retry(void);            // in wait loops: draw again if the last present was skipped (GPU busy)
u16 *ui_legacy_fb(gfxScreen_t screen); // the RAM "framebuffer" (column-major RGB565, like the real one)
// Walker shades for pictures drawn into the text view (0 and the console's text colour are re-coloured).
#define UI_LEGACY_WHITE 0xFFFF
#define UI_LEGACY_BLACK 0x0001
void ui_debug_view(void); // full-screen text view until B (Settings -> Debug view)

// v0.44: the message screen. Outside the connect screen, what the flows print is shown in the app's own style (the
// lines since the last screen clear, header = the flow's title) instead of the old 8x8 text. Set by main.c:
void ui_msg_ask(const char *question, int dangerous); // bottom: the question, A Yes / B No (or hold L+R)
void ui_msg_ok(void);                                  // bottom: A OK
void ui_msg_clear(void);                               // bottom: "One moment..."
void ui_legacy_raw(int on); // 1 = the old text view as it was (screens laid out in columns or with pictures)
// The printed page (source/ui_connect.c): lines since the last clear, oldest first, and the line being printed.
int ui_page_count(void);
const char *ui_page_line(int i);
const char *ui_page_current(void);

// --- drawing (1x, positions as in the mockups) -------------------------------------------------------
enum ui_piece {
	UI_PANEL, UI_GLOSS6, UI_GLOSS6_SEL, UI_GLOSS3, UI_GLOSS3_SEL, UI_INFO, UI_ICONBOX, UI_GAMEICON, UI_LCDFRAME,
	UI_PIECE_COUNT
};
void ui_box(int piece, float x, float y, float w, float h);            // 9-slice piece over this rect
void ui_band(float x, float y, float w, float h, const char *text, float size, int red);
void ui_status_bar(const char *right);                                  // top screen header
void ui_footer(const char *const *left, int nleft, const char *const *right, int nright); // "B Back", ...
void ui_key(float cx, float cy, char key, int r);                       // r = 7, 8 or 10
void ui_stripe(float x, float y, float h);
void ui_brackets(float x0, float y0, float x1, float y1);               // selection marks left/right
void ui_rect(float x, float y, float w, float h, u32 color);
void ui_arrow(float cx, float cy, char dir, float r, u32 color); // v0.49: 'l' 'r' 'u' 'd' triangle
void ui_unknown_icon(float x, float y); // v0.50: the 32x32 "?" picture for a Pokemon not seen yet
void ui_lcd(float x, float y, int w, int h, int scale);                // LCD glass (draw the picture over it)
void ui_lcd_frame(float x, float y, int w, int h);                      // frame around an LCD area
void ui_lcd_grid(float x, float y, int w, int h, int scale); // v0.50: LCD pixel grid over a picture (scale >= 2)
void ui_connect_scene(float x, float y, int searching); // v0.46 render 368 x 116; v0.50 arcs flash (searching) or build up
int ui_msg_technical(const char *line);   // link details / file paths that only the debug view shows
void ui_info_box(float x, float y, float w, float h, const char *title, const char *value, float size);

// text: size = the mockup's font size; align 'l' / 'c' / 'r'; y = top of the text, or its middle with mid=1
void ui_text(float x, float y, const char *s, float size, u32 color, int bold, char align, int mid);
float ui_text_width(const char *s, float size);

// Picture from 2-bit walker data, kept as a texture until freed. Returns 0 on failure.
typedef struct ui_image {
	C3D_Tex tex;
	Tex3DS_SubTexture sub;
	C2D_Image img;
	int ok;
} ui_image;
int ui_image_from_walker(ui_image *im, const u8 *data, int w, int h, int transparent_white);
int ui_image_from_rgb565(ui_image *im, const u16 *px, int w, int h); // linear rows, top first
void ui_image_free(ui_image *im);
void ui_image_draw(const ui_image *im, float x, float y, float scale);

// --- connect screen (source/ui_connect.c, v0.36) -----------------------------------------------------------
// While a link is open (session_open .. session_close) the text view is replaced by mock_5_connect's screen:
// steps, the flow's current progress line + bar, its last few messages. The flows keep printing as before.
void ui_tap_stdout(void);              // once at start-up, after consoleInit
void ui_connect_begin(const char *title); // "SEND TO WALKER\nPart 1 of 2: safety backup"
void ui_connect_end(void);
int ui_connect_active(void);
int ui_connect_present(int ir_busy);   // 0 = could not draw (GPU busy)

// --- HOME menu data of a title (SMDH) --------------------------------------------------------------------
typedef struct ui_title {
	u64 title_id;
	char name[64];  // English short name ("Pokemon X"), UTF-8
	ui_image icon;  // 48x48
	int ok;
} ui_title;
int ui_title_load(ui_title *t, u64 title_id); // tries SD, then the game card
void ui_title_free(ui_title *t);

#ifdef __cplusplus
}
#endif

#endif
