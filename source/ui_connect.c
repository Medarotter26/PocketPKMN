// ui_connect.c - the connect screen (mock_5_connect) shown while a walker link runs, and the question popup.
//
// The flows are not changed: they keep printing their progress ("Reading  45%", "Uploading 12%", "Link lost ...")
// to the console. Everything printed to stdout also passes through tap_write() here, which keeps the current
// line and the last few finished lines; the connect screen shows them as a status line, a progress bar and a
// short log. Drawing follows the IR rule of include/ui.h: frames only when the flow already redraws (between
// packets), at most one progress frame per 200 ms, never waiting for the GPU.
#include <stdio.h>
#include <string.h>
#include <sys/iosupport.h>

#include "app_paths.h"
#include "sound.h"
#include "ui.h"

// --- stdout tap -------------------------------------------------------------------------------------------------
#define LOG_LINES 4
#define LINE_MAX_ 64
static char cur[LINE_MAX_];            // the line being printed (after the last \n or \r)
static int cur_len, cur_cr;            // cur_cr: the line was started with \r (a progress line)
static char done_lines[LOG_LINES][LINE_MAX_]; // last finished non-empty lines, oldest first
static int esc;                        // inside an escape sequence
static int changed;                    // a finished line arrived since the last frame
static devoptab_t tap;
static const devoptab_t *orig;

// v0.44: the "page" = every line printed since the last screen clear (ESC [2J), for the message screen that replaces
// the old text view (ui_core.c). Blank lines kept (one in a row).
#define PAGE_LINES 48
static char pg[PAGE_LINES][LINE_MAX_];
static int page_n;                     // lines in page (oldest dropped when full)
static char esc_buf[8];
static int esc_len;

static void page_add(const char *line)
{
	if (!line[0] && (page_n == 0 || !pg[page_n - 1][0]))
		return; // no leading or double blank lines
	if (page_n == PAGE_LINES) {
		memmove(pg[0], pg[1], sizeof(pg[0]) * (PAGE_LINES - 1));
		page_n--;
	}
	snprintf(pg[page_n++], LINE_MAX_, "%s", line);
}

int ui_page_count(void)
{
	return page_n;
}

const char *ui_page_line(int i)
{
	return i >= 0 && i < page_n ? pg[i] : "";
}

const char *ui_page_current(void)
{
	return cur;
}

static void note_line(const char *l); // connect screen state (below)

static void line_done(void)
{
	cur[cur_len] = 0;
	page_add(cur);
	note_line(cur);
	if (cur_len > 0) {
		memmove(done_lines[0], done_lines[1], sizeof(done_lines[0]) * (LOG_LINES - 1));
		snprintf(done_lines[LOG_LINES - 1], LINE_MAX_, "%s", cur);
		changed = 1;
	}
	cur_len = 0;
	cur[0] = 0;
}

static void tap_bytes(const char *p, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		char c = p[i];
		if (esc) { // ESC [ ... letter
			if (esc_len < (int)sizeof(esc_buf) - 1)
				esc_buf[esc_len++] = c;
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
				esc = 0;
				esc_buf[esc_len] = 0;
				if (!strcmp(esc_buf, "[2J")) { // consoleClear: a new page
					page_n = 0;
					cur_len = 0;
				}
			}
			continue;
		}
		if (c == 0x1b) {
			esc = 1;
			esc_len = 0;
			continue;
		}
		if (c == '\n') {
			line_done();
			cur_cr = 0;
		} else if (c == '\r') {
			cur_len = 0;
			cur_cr = 1;
		} else if (c == '~') { // the system font has no '~' ("(~25 s)" -> "(about 25 s)")
			for (const char *w = "about "; *w && cur_len < LINE_MAX_ - 1; w++)
				cur[cur_len++] = *w;
		} else if (cur_len < LINE_MAX_ - 1) {
			cur[cur_len++] = c;
		}
	}
	cur[cur_len] = 0;
}

// v0.85 (owner: on the Move Pokemon screen "pokemon platinum is glitched. the k and accent e dont render correctly
// (the debug looking screen)"): the text consoles draw one byte per character with an 8x8 font, so a UTF-8 letter
// (2-3 bytes) showed as two or three odd glyphs. What goes to the consoles is now folded to ASCII (e-acute -> e,
// sharp s -> ss, the gender signs -> M / F, curly quote -> '); the message screen (system font) still gets the real
// UTF-8 through tap_bytes. A sequence cut between two writes is kept until the next write.
static void fold_cp(unsigned cp, char *out, int *n)
{
	// U+00C0..U+00FF, one letter each ('?' where there is no plain form; sharp s handled below)
	static const char lat1[] = "AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
	const char *s = NULL;
	char one[2] = {0, 0};
	if (cp == 0xDF) s = "ss";
	else if (cp >= 0xC0 && cp <= 0xFF) { one[0] = lat1[cp - 0xC0]; s = one; }
	else if (cp == 0x152) s = "OE";
	else if (cp == 0x153) s = "oe";
	else if (cp == 0x2642 || cp == 0xE08E) s = "M";
	else if (cp == 0x2640 || cp == 0xE08F) s = "F";
	else if (cp == 0x2018 || cp == 0x2019) s = "'";
	else if (cp == 0x201C || cp == 0x201D) s = "\"";
	else if (cp == 0x2026) s = "...";
	else if (cp == 0xB7 || cp == 0x2013 || cp == 0x2014) s = "-";
	else s = "?";
	while (*s)
		out[(*n)++] = *s++;
}

static ssize_t tap_write(struct _reent *r, void *fd, const char *ptr, size_t len)
{
	tap_bytes(ptr, len);
	static unsigned char part[4];
	static int part_n, part_need;
	char out[256];
	int n = 0;
	for (size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)ptr[i];
		if (part_need) {
			if ((c & 0xC0) == 0x80) {
				part[part_n++] = c;
				if (part_n == part_need) {
					unsigned cp = part_need == 2 ? ((part[0] & 0x1Fu) << 6) | (part[1] & 0x3Fu)
					                             : ((part[0] & 0x0Fu) << 12) | ((part[1] & 0x3Fu) << 6) | (part[2] & 0x3Fu);
					fold_cp(cp, out, &n);
					part_need = part_n = 0;
				}
				goto flush;
			}
			out[n++] = '?'; // a broken sequence
			part_need = part_n = 0;
		}
		if (c >= 0xC2 && c <= 0xDF) { part[0] = c; part_n = 1; part_need = 2; }
		else if (c >= 0xE0 && c <= 0xEF) { part[0] = c; part_n = 1; part_need = 3; }
		else if (c >= 0x80) out[n++] = '?';
		else out[n++] = (char)c;
	flush:
		if (n > (int)sizeof(out) - 8) {
			orig->write_r(r, fd, out, (size_t)n);
			n = 0;
		}
	}
	if (n)
		orig->write_r(r, fd, out, (size_t)n);
	return (ssize_t)len; // all bytes taken (some held for the next write)
}

void ui_tap_stdout(void)
{
	orig = devoptab_list[STD_OUT];
	tap = *orig;
	tap.write_r = tap_write;
	devoptab_list[STD_OUT] = &tap;
}

// --- connect screen ---------------------------------------------------------------------------------------------
static int active;
// the v0.46 state below, declared early (reset in ui_connect_begin)
static int ph_n, connected, lost;
static char keep_line[LINE_MAX_];
static char page[48], part[48];
static u64 last_frame;

static void sentence_case(const char *in, char *out, size_t n)
{
	size_t k = 0;
	for (; in[k] && in[k] != '\n' && k + 1 < n; k++) {
		char c = in[k];
		out[k] = (k == 0 || (c < 'A' || c > 'Z')) ? c : (char)(c - 'A' + 'a');
	}
	out[k] = 0;
}

void ui_connect_begin(const char *title)
{
	// "SEND TO WALKER\nPart 1 of 2: safety backup" -> page "Send to walker", part "Part 1 of 2: safety backup"
	sentence_case(title, page, sizeof(page));
	const char *nl = strchr(title, '\n');
	snprintf(part, sizeof(part), "%s", nl ? nl + 1 : "");
	memset(done_lines, 0, sizeof(done_lines));
	cur_len = 0;
	cur[0] = 0;
	active = 1;
	last_frame = 0;
	ph_n = 0;
	connected = lost = 0;
	keep_line[0] = 0;
}

void ui_connect_end(void)
{
	active = 0;
	sound_transfer(0); // v0.55
	sound_link(0);     // v0.59: the music glides back up (the link is closed by now)
}

int ui_connect_active(void)
{
	return active;
}

// "Reading  45%" -> pct 45, text "Reading"; no % -> -1
static int split_pct(const char *s, char *text, size_t n)
{
	const char *pc = strrchr(s, '%');
	snprintf(text, n, "%s", s);
	if (!pc)
		return -1;
	const char *d = pc;
	while (d > s && d[-1] >= '0' && d[-1] <= '9')
		d--;
	if (d == pc)
		return -1;
	int v = 0;
	for (const char *q = d; q < pc && v <= 1000; q++) // audit M14: a long run of digits cannot overflow
		v = v * 10 + (*q - '0');
	size_t t = (size_t)(d - s);
	while (t > 0 && s[t - 1] == ' ')
		t--;
	if (t < n)
		text[t] = 0;
	return v > 100 ? 100 : v;
}

// --- v0.46 connect screen state (user: "Keep the walker still and its percentage countdowns are important info!!!
// They should have bars though"; "Add in the 3ds and pokewalker model infographic") ---------------------------------
#define PHASES 4
static char ph_name[PHASES][24];
static int ph_pct[PHASES], ph_n, connected, lost;
static char keep_line[LINE_MAX_];

static void phase_set(const char *name, int pct)
{
	for (int i = 0; i < ph_n; i++)
		if (!strcmp(ph_name[i], name)) {
			ph_pct[i] = pct;
			return;
		}
	if (ph_n == PHASES) {
		memmove(ph_name[0], ph_name[1], sizeof(ph_name[0]) * (PHASES - 1));
		memmove(ph_pct, ph_pct + 1, sizeof(ph_pct[0]) * (PHASES - 1));
		ph_n--;
	}
	snprintf(ph_name[ph_n], sizeof(ph_name[0]), "%s", name);
	ph_pct[ph_n++] = pct;
}

static void note_line(const char *l)
{
	if (!active)
		return;
	char name[LINE_MAX_];
	int pct = split_pct(l, name, sizeof(name));
	if (pct >= 0 && name[0]) {
		phase_set(name, pct);
		return;
	}
	if (!strncmp(l, "Connected", 9)) {
		connected = 1;
		lost = 0;
		sound_transfer(1); // v0.55: the transfer sound while data moves (from memory: safe during the link)
	} else if (!strncmp(l, "Link lost", 9)) {
		lost = 1;
		connected = 0;
		sound_transfer(0);
	} else if (!strncmp(l, "Keep the walker still", 21)) {
		connected = 1;
		sound_transfer(1);
		const char *k = l + 21; // " (about 25 s)." -> "about 25 seconds"
		while (*k == ' ' || *k == '(')
			k++;
		snprintf(keep_line, sizeof(keep_line), "%s", k);
		size_t n = strlen(keep_line);
		while (n && (keep_line[n - 1] == '.' || keep_line[n - 1] == ')' || keep_line[n - 1] == ' '))
			keep_line[--n] = 0;
		if (n > 2 && !strcmp(keep_line + n - 2, " s"))
			snprintf(keep_line + n - 1, sizeof(keep_line) - (n - 1), "seconds");
	}
}

static const char *phase_label(const char *n)
{
	if (!strcmp(n, "Reading"))
		return "Reading the walker";
	if (!strcmp(n, "Checking"))
		return "Checking the copy";
	if (!strcmp(n, "Uploading"))
		return "Sending the walk";
	if (!strcmp(n, "Writing"))
		return "Writing to the walker";
	return n;
}

static void bar_row(float y, const char *label, int pct, int current)
{
	char p[16];
	ui_text(24, y, label, 11, current ? UI_TEXT : UI_SUB, current, 'l', 1);
	ui_bar(168, y - 6, 160, 12, (pct > 100 ? 100 : pct) / 100.0f, current); // v0.83: the shared bar
	snprintf(p, sizeof(p), "%d %%", pct > 100 ? 100 : pct);
	ui_text(376, y, p, 11, UI_SUB, 1, 'r', 1);
}

static void draw_connect(void)
{
	// the progress line being printed now (e.g. "\rReading  45%")
	char cname[LINE_MAX_];
	int cpct = cur_len > 0 ? split_pct(cur, cname, sizeof(cname)) : -1;
	if (cpct >= 0 && cname[0])
		phase_set(cname, cpct);

	ui_target_top();
	char right[112];
	snprintf(right, sizeof(right), "%s%s%s", page, part[0] ? " - " : "", part);
	ui_status_bar(right);
	ui_box(UI_PANEL, 12, 26, 376, 140);
	// the 3DS from behind with the walker at its IR window (Blender render + IR arcs); v0.50: while looking for the
	// walker the arcs flash, once connected they build up one by one (user)
	ui_connect_scene(16, 30, lost || !connected);
	ui_text(200, 156, "Point the walker's top at the 3DS's IR window, 2-5 cm away", 10, UI_TEXT, 0, 'c', 1);
	ui_box(UI_PANEL, 12, 174, 376, 58);
	if (ph_n == 0) {
		ui_text(200, 203, lost ? "Link lost \xE2\x80\x94 waiting for the walker again\xE2\x80\xA6"
		                       : connected ? "Connected" : "Waiting for the walker\xE2\x80\xA6",
		        12, lost ? UI_ACCENT : UI_TEXT, 1, 'c', 1);
	} else {
		int first = ph_n > 2 ? ph_n - 2 : 0; // the two newest steps, one bar each
		for (int i = first, row = 0; i < ph_n; i++, row++)
			bar_row(ph_n - first == 1 ? 203 : 191 + row * 24, phase_label(ph_name[i]), ph_pct[i], i == ph_n - 1);
	}

	ui_target_bottom();
	int steps_box = !lost && !connected; // the written directions, until the walker answers
	ui_box(UI_PANEL, 16, steps_box ? 10 : 14, 288, steps_box ? 180 : 120);
	if (lost) {
		ui_band(16, 14, 288, 22, "Link lost", 12, 1);
		ui_text(160, 64, "On the walker choose", 12, UI_TEXT, 0, 'c', 1);
		ui_text(160, 82, "Connect again.", 12, UI_TEXT, 1, 'c', 1);
		ui_text(160, 110, "It continues where it stopped.", 10, UI_SUB, 0, 'c', 1);
	} else if (connected) {
		ui_text(160, 50, "Keep the walker still", 16, UI_BAND, 1, 'c', 1);
		if (keep_line[0]) { // v0.50 (user: "about 25 seconds" alone "is also a weird sentence")
			char kl[sizeof(keep_line) + 16];
			snprintf(kl, sizeof(kl), "This takes %s.", keep_line);
			ui_text(160, 76, kl, 12, UI_TEXT, 0, 'c', 1);
		}
		ui_text(160, 106, "You can move it when this says Done.", 10, UI_SUB, 0, 'c', 1);
	} else {
		// v0.47 (user: "I do miss the box where it had written out directions" / "With the 1 and 2"): the two steps
		// of the earlier connect screen, numbered, until the walker answers.
		ui_text(40, 42, "1", 22, UI_BAND, 1, 'c', 1);
		ui_text(62, 34, "On the walker, press the middle", 11, UI_TEXT, 0, 'l', 1);
		ui_text(62, 50, "button and choose Connect.", 11, UI_TEXT, 0, 'l', 1);
		ui_rect(30, 70, 260, 1, C2D_Color32(176, 208, 156, 255));
		ui_text(40, 104, "2", 22, UI_BAND, 1, 'c', 1);
		ui_text(62, 88, "Point the walker's top edge at", 11, UI_TEXT, 0, 'l', 1);
		ui_text(62, 104, "the dark IR window on the back", 11, UI_TEXT, 0, 'l', 1);
		ui_text(62, 120, "edge of the 3DS, 2-5 cm away.", 11, UI_TEXT, 0, 'l', 1);
		ui_rect(30, 140, 260, 1, C2D_Color32(176, 208, 156, 255));
		ui_text(160, 166, "Keep it still until this says Done.", 10.5f, UI_SUB, 0, 'c', 1);
	}
	// the newest message that matters (not link details or progress), e.g. "A different walker answered!"
	const char *msg = "";
	for (int i = LOG_LINES - 1; i >= 0 && !msg[0]; i--) {
		const char *l = done_lines[i];
		char tmp[LINE_MAX_];
		if (l[0] && !ui_msg_technical(l) && split_pct(l, tmp, sizeof(tmp)) < 0 && strncmp(l, "Keep the walker", 15) &&
		    strncmp(l, "Link lost", 9) && strncmp(l, "On the walker", 13) && strncmp(l, "again within", 12) &&
		    strncmp(l, "continues where", 15) && strncmp(l, "B cancels", 9))
			msg = l;
	}
	if (msg[0])
		ui_text(160, steps_box ? 203 : 160, msg, 10.5f, UI_TEXT, 0, 'c', 1);
	static const char *const lb[] = { "B Cancel" };
	ui_footer(lb, 1, NULL, 0);
}

// Called from ui_legacy_present while a link runs. Returns 1 if it drew (or skipped on purpose).
int ui_connect_present(int ir_busy)
{
	u64 now = osGetTime();
	// progress-only updates at most every 200 ms; a finished line (a message) is always drawn
	if (ir_busy && !changed && now - last_frame < 200)
		return 1;
	if (!ui_begin())
		return 0;
	changed = 0;
	last_frame = now;
	draw_connect();
	ui_end();
	return 1;
}
