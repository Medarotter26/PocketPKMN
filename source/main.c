// WalkerBox (working title) - Pokewalker tools over the 3DS's IR port.
//
// Menu of operations on a real Pokewalker:
//   link test, backup (read only), write test (staging area only),
//   restore a backup, walk start test (re-sends the walker's current walk),
//   walk end (brings the walk back and records what it found).
// Every operation that writes makes a full safety backup first.
// A detailed log of every exchange goes to sdmc:/3ds/pocketpkmn/link_log.txt (v0.33; walkerbox/ before).

#include <3ds.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app_migrate.h"
#include "app_paths.h"
#include "dbg.h"
#include "ir_link.h"
#include "pw_image.h"
#include "pw_menu_art.h"
#include "pw_proto.h"
#include "storage.h"
#include "ui.h"
#include "ui_home.h"
#include "ui_pick.h"
#include "ui_send.h"
#include "ui_result.h"
#include "ui_credits.h"
#include "routes.h"
#include "walk_compose.h"
#include "wbx_sprites.h"
#include "wbx_trainer.h"

#define APP_VERSION "0.51"
#define LOG_PATH    APP_DIR "/link_log.txt"
#define LEDGER_PATH APP_DIR "/ledger.txt"

#define CHUNK          0x80   // bytes per EEPROM packet
#define MAX_RECONNECTS 5

// EEPROM layout used here (see docs/RESEARCH.md)
#define EE_UNIQUE_ID     0x0083  // UniqueIdentityData, 0x28 bytes
#define EE_RESTORE_FROM  0x0280  // restore never touches the walker-managed header below this
#define EE_ROUTE         0x8F00  // RouteInfo + walk images, 0x2900 bytes
#define EE_WALK_SIZE     0x2900
#define EE_STAGE_WALK    0xD700
#define EE_TEAM          0xCC00  // TeamData (written as 0x280 bytes)
#define EE_STAGE_TEAM    0xD480
#define EE_TEAM_SIZE     0x280
#define EE_WATTS         0xCE8A  // u16 BE
#define EE_CAUGHT        0xCE8C  // 3 x PokemonSummary
#define EE_DOWSED        0xCEBC  // 3 x {u16 item, u16}
#define EE_GIFTED        0xCEC8  // 10 x {u16 item, u16}
#define EE_SPECIAL_FLAGS 0xB800
#define EE_EVENT_POKE    0xBA44  // PokemonSummary
#define EE_EVENT_ITEM    0xBD46  // u16 item (after 6 zero bytes)
#define WRITE_TEST_ADDR  0xD700  // walk-start staging area: only read by the walker on command 5A
#define WRITE_TEST_SIZE  0x100

static PrintConsole top, bottom;
static int trace_quiet;
static u8 eeprom[PW_EEPROM_SIZE];   // live copy read from the walker
static u8 image[PW_EEPROM_SIZE];    // a backup file loaded for restore / set-up

// v0.11 (docs/LINK_REVIEW.md 7.1): do not start ir:rst. libctru's hidInit()
// calls irrstInit() on a New 3DS unless this weak hook says no; with ir:rst
// running, the IR module reads ZL/ZR/C-stick on the same I2C bus as the IR
// chip and our transfers collide with it. The app does not use those controls
// and never calls hidWaitForAnyEvent (which would wait on the ir:rst event).
bool hidShouldUseIrrst(void)
{
	return false;
}

// SELECT switch: 0 = ir:rst off (default), 1 = irrstInit() for each connection
// (the old behaviour), for an A/B comparison in one build.
static int cstick_on;

static int checked_block_ok(const u8 *ee);
static int walk_valid(const u8 *ee);
static void put_u32le(u8 *p, u32 v);
static void put_u32be(u8 *p, u32 v);
#define EE_NAME_IMAGE    0x1250  // trainer name, 80x16

// The WalkerBox trainer (include/wbx_trainer.h), loaded or made at start-up.
static wbx_trainer trainer;
static int have_trainer;

// --- screen helpers ----------------------------------------------------------

// v0.32: the consoles write into RAM; this shows their text on the GPU screens (include/ui.h).
static void refresh(void)
{
	ui_legacy_present();
}

static void trace_cb(void *user, int dir, const uint8_t *data, size_t len, int status)
{
	(void)user;
	if (trace_quiet && status == 0)
		return;
	if (status)
		dbg_printf("%s %u byte(s)  ** %s", dir ? "RX" : "TX", (unsigned)len, pw_strerror(status));
	else
		dbg_printf("%s %u byte(s)", dir ? "RX" : "TX", (unsigned)len);
	if (len)
		dbg_hex(dir ? "rx" : "tx", data, len, 160);
}

static int cancel_cb(void *user)
{
	// v0.50 (animated IR arcs): while still waiting for the walker (between 100 ms listening slices, no packet in
	// flight) the connect screen is drawn again so its arcs move; throttled to 200 ms and never waits on the GPU
	// (ui_connect_present). Once connected, frames come only from the flows' progress lines, as before.
	const pw_link *l = (const pw_link *)user;
	if (l && !l->connected && ui_connect_active())
		ui_legacy_present();
	hidScanInput();
	return (hidKeysDown() & KEY_B) != 0;
}

// Wait for a decision. Dangerous questions need L+R held while pressing A.
static int ask(const char *question, int dangerous)
{
	ui_msg_ask(question, dangerous); // v0.44: the question on the bottom screen (message screen)
	printf("\n%s\n", question);
	printf(dangerous ? "Hold L+R and press A = yes, B = no\n" : "A = yes, B = no\n");
	refresh();
	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown(), held = hidKeysHeld();
		if (down & KEY_B) {
			ui_msg_clear();
			return 0;
		}
		if ((down & KEY_A) && (!dangerous || ((held & KEY_L) && (held & KEY_R)))) {
			ui_msg_clear();
			return 1;
		}
		ui_legacy_retry();
		gspWaitForVBlank();
	}
	return 0;
}

static void wait_key(void)
{
	ui_msg_ok(); // v0.44: "A OK" on the bottom screen
	printf("\nPress A to go back.\n");
	refresh();
	while (aptMainLoop()) {
		hidScanInput();
		if (hidKeysDown() & (KEY_A | KEY_B)) {
			ui_msg_clear();
			return;
		}
		ui_legacy_retry();
		gspWaitForVBlank();
	}
	ui_msg_clear();
}

static void show_instructions(const char *title)
{
	consoleSelect(&top);
	consoleClear();
	printf("%s\n\n", title);
	printf("1. On the walker, press the centre\n   button and choose CONNECT.\n");
	printf("2. Point the walker's top edge at\n   the dark IR window on the back\n   edge of the 3DS, 2-5 cm away.\n\n");
	printf("Waiting up to 20 s (B cancels)...\n\n");
	ui_connect_begin(title); // v0.36: connect screen until session_close (the text above stays in the debug view)
	refresh();
}

static void ensure_dir(void)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(APP_DIR, 0777);
}

static void log_header(const char *what)
{
	u8 model = 0xFF;
	cfguInit();
	CFGU_GetSystemModel(&model);
	cfguExit();
	dbg_printf("==== %s | " APP_NAME " " APP_VERSION " | guard %s | model %u ====", what,
	           ir_raw_guard_enabled ? "on" : "off", model);
}

static void flush_log(void)
{
	ensure_dir();
	dbg_printf("---- end ----");
	if (dbg_flush(LOG_PATH) != 0)
		printf("\nCould not write the log file!\n");
	refresh();
}

// Write what the log holds so far to the SD card (no walker may be waiting).
static void log_checkpoint(void)
{
	ensure_dir();
	dbg_flush(LOG_PATH);
}

// v0.50 (audit S5): while a walker link is open, ledger lines wait in RAM and are written by ledger_flush() right
// after the link closes (no SD card access in IR time). Returns 0 when written (or kept for later), -1 on failure
// (audit B2 c: the caller can tell the user).
static int link_open_now;
static char ledger_pending[4096];
static size_t ledger_pending_len;

static int ledger_write(const char *text)
{
	ensure_dir();
	FILE *f = fopen(LEDGER_PATH, "a");
	if (!f)
		return -1;
	int bad = fputs(text, f) < 0;
	if (fclose(f) != 0)
		bad = 1;
	return bad ? -1 : 0;
}

static void ledger_flush(void)
{
	if (!ledger_pending_len)
		return;
	if (ledger_write(ledger_pending) != 0)
		dbg_printf("ledger: writing %u held line byte(s) FAILED", (unsigned)ledger_pending_len);
	ledger_pending_len = 0;
	ledger_pending[0] = 0;
}

static int ledger(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static int ledger(const char *fmt, ...)
{
	char line[512];
	size_t n = 0;
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	if (tm)
		n = (size_t)snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d  ", tm->tm_year + 1900, tm->tm_mon + 1,
		                     tm->tm_mday, tm->tm_hour, tm->tm_min);
	va_list ap;
	va_start(ap, fmt);
	int k = vsnprintf(line + n, sizeof(line) - n - 1, fmt, ap);
	va_end(ap);
	n = k < 0 ? n : n + (size_t)k < sizeof(line) - 2 ? n + (size_t)k : sizeof(line) - 2;
	line[n++] = '\n';
	line[n] = 0;
	if (link_open_now) {
		if (ledger_pending_len + n >= sizeof(ledger_pending)) {
			dbg_printf("ledger: held lines full, line dropped: %s", line);
			return -1;
		}
		memcpy(ledger_pending + ledger_pending_len, line, n + 1);
		ledger_pending_len += n;
		return 0;
	}
	int r = ledger_write(line);
	if (r != 0)
		dbg_printf("ledger: write FAILED: %s", line);
	return r;
}

static u32 fnv1a(const u8 *p, size_t n)
{
	u32 h = 2166136261u;
	for (size_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

static void hex_of(const u8 *p, size_t n, char *out)
{
	for (size_t i = 0; i < n; i++)
		sprintf(out + 2 * i, "%02x", p[i]);
	out[2 * n] = '\0';
}

// --- link ----------------------------------------------------------------------

typedef struct session {
	pw_link link;
	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	unsigned reconnects;
	int open;
	int irrst; // irrstInit succeeded for this session (C-stick service "on")
} session;

static int session_connect(session *s, uint32_t wait_ms)
{
	int r = pw_connect(&s->link, wait_ms, (uint32_t)svcGetSystemTick());
	if (r < 0) {
		printf("Connect failed: %s\n", pw_strerror(r));
		dbg_printf("connect failed: %s", pw_strerror(r));
		return r;
	}
	dbg_printf("connected, session %02x%02x%02x%02x", s->link.session[0], s->link.session[1],
	           s->link.session[2], s->link.session[3]);
	r = pw_get_identity(&s->link, &s->id, s->id_raw);
	if (r < 0) {
		printf("Identity read failed: %s\n", pw_strerror(r));
		dbg_printf("identity failed: %s", pw_strerror(r));
	}
	return r;
}

// Open the IR chip and connect. Returns 0 when connected and identified.
static int session_open(session *s, const char *title, const char *log_what)
{
	memset(s, 0, sizeof(*s));
	show_instructions(title);
	log_header(log_what);
	trace_quiet = 0;

	if (cstick_on) {
		// The "on" choice of the A/B switch: the old behaviour (ir:rst running).
		Result rr = irrstInit();
		s->irrst = R_SUCCEEDED(rr);
		dbg_printf("irrstInit -> %08lx", (unsigned long)rr);
	}
	pw_transport io;
	ui_set_ir_busy(1); // v0.32: from here to session_close no file access and no waiting on the GPU
	link_open_now = 1; // v0.50 (audit S5): ledger lines wait in RAM until session_close
	if (ir_raw_open(&io) < 0) {
		ui_set_ir_busy(0);
		link_open_now = 0;
		ledger_flush();
		printf("Could not start the IR hardware.\nSee the log file for the reason.\n");
		return -1;
	}
	s->open = 1;
	pw_link_init(&s->link, &io);
	s->link.trace = trace_cb;
	s->link.cancel = cancel_cb;
	s->link.user = &s->link; // v0.50: cancel_cb sees whether the walker is connected yet
	int r = session_connect(s, 20000);
	if (r == 0)
		printf("Connected.\n");
	return r;
}

static void session_close(session *s, int disconnect)
{
	if (disconnect)
		pw_disconnect(&s->link);
	if (s->open) {
		ir_raw_stats_log();
		ir_raw_close();
	}
	ui_set_ir_busy(0);
	link_open_now = 0;
	ledger_flush(); // v0.50 (audit S5): the lines held during the link
	if (s->irrst) {
		irrstExit();
		dbg_printf("irrstExit");
	}
	s->irrst = 0;
	s->open = 0;
	trace_quiet = 0;
	flush_log();
	ui_connect_end(); // v0.36: back to the text view for the flow's results
}

// The walker gave up on the link: ask the user to connect again, check that the
// same walker answered. Returns 0 when the link is back.
static int session_reconnect(session *s, int why)
{
	if (s->reconnects >= MAX_RECONNECTS)
		return why;
	s->reconnects++;
	int quiet = trace_quiet;
	pw_disconnect(&s->link);
	trace_quiet = 0;
	printf("\n\nLink lost (%s).\n", pw_strerror(why));
	printf("On the walker choose CONNECT\nagain within 60 s; PocketPKMN\ncontinues where it stopped.\nB cancels.\n");
	refresh();
	dbg_printf("link lost (%s), reconnect %u", pw_strerror(why), s->reconnects);

	u8 want[0x28];
	memcpy(want, s->id.unique_id, sizeof(want));
	int r = session_connect(s, 60000);
	if (r == 0 && memcmp(s->id.unique_id, want, sizeof(want)) != 0) {
		printf("A different walker answered!\n");
		dbg_printf("reconnect: unique id differs");
		r = PW_ERR_UNEXPECTED;
	}
	trace_quiet = quiet;
	refresh();
	return r;
}

static int read_chunk(session *s, unsigned addr, u8 *out)
{
	for (;;) {
		int r = pw_eeprom_read(&s->link, (uint16_t)addr, out, CHUNK);
		if (r == 0 || r == PW_ERR_CANCELLED)
			return r;
		r = session_reconnect(s, r);
		if (r < 0)
			return r;
	}
}

static int write_chunk(session *s, unsigned addr, const u8 *data)
{
	for (;;) {
		int r = pw_eeprom_write(&s->link, (uint16_t)addr, data, CHUNK, 1);
		if (r == 0 || r == PW_ERR_CANCELLED || r == PW_ERR_VERIFY)
			return r;
		r = session_reconnect(s, r);
		if (r < 0)
			return r;
	}
}

static int read_range(session *s, unsigned addr, u8 *out, size_t len)
{
	for (size_t done = 0; done < len; done += CHUNK) {
		int r = read_chunk(s, addr + done, out + done);
		if (r < 0)
			return r;
	}
	return 0;
}

// --- walker information ------------------------------------------------------------

static void print_identity(const pw_identity *id)
{
	char name[16];
	pw_text_to_ascii(id->trainer_name, 8, name, sizeof(name));
	printf("Trainer: %s  ID %05u / %05u\n", name, id->tid, id->sid);
	printf("Paired: %s  Has Pokemon: %s\n", (id->flags & PW_IDFLAG_PAIRED) ? "yes" : "no",
	       (id->flags & PW_IDFLAG_HAS_POKE) ? "yes" : "no");
	printf("Total steps: %lu\n", (unsigned long)id->total_steps);
	dbg_printf("identity: unk0 %lu unk1 %lu unk2 %u unk3 %u flags %02x proto %u.%u",
	           (unsigned long)id->unk0, (unsigned long)id->unk1, id->unk2, id->unk3, id->flags,
	           id->proto_ver, id->proto_subver);
}

static void route_text(const u8 *route, unsigned off, int chars, char *out, size_t out_size)
{
	uint16_t text[24];
	for (int i = 0; i < chars; i++)
		text[i] = (uint16_t)(route[off + 2 * i] | (route[off + 1 + 2 * i] << 8));
	pw_text_to_ascii(text, (size_t)chars, out, out_size);
}

static void print_stroll(const u8 *route)
{
	pw_poke_summary p;
	pw_poke_summary_parse(route, &p);
	char nick[16], rname[24];
	route_text(route, 0x10, 11, nick, sizeof(nick));
	route_text(route, 0x28, 21, rname, sizeof(rname));
	printf("On stroll: #%u \"%s\" Lv %u%s\n", p.species, nick, p.level, (p.flags & 0x02) ? " (shiny)" : "");
	printf("Route: %s\n", rname);
}

// Is the walk on this walker one that WalkerBox started? Looks for the walker's
// unique id and the hash of its route data in the ledger.
// Short name for a walker in the ledger: hash of its whole 40-byte unique id.
static unsigned long walker_key(const pw_identity *id)
{
	return (unsigned long)fnv1a(id->unique_id, 0x28);
}

static int walk_is_ours(const pw_identity *id, const u8 *route)
{
	char uid[0x28 * 2 + 1], line[256], want[128], want_old[128];
	hex_of(id->unique_id, 0x28, uid);
	unsigned long rh = (unsigned long)fnv1a(route, 0xBE);
	snprintf(want, sizeof(want), "walk-start walker=%08lx route=%08lx", walker_key(id), rh);
	// v0.7 wrote only the first 8 bytes of the unique id.
	snprintf(want_old, sizeof(want_old), "walk-start walker=%.16s route=%08lx", uid, rh);
	FILE *f = fopen(LEDGER_PATH, "r");
	if (!f)
		return 0;
	int found = 0;
	while (fgets(line, sizeof(line), f))
		if (strstr(line, want) || strstr(line, want_old))
			found = 1;
	fclose(f);
	return found;
}

// If the walker carries a Pokemon from a game (not from WalkerBox), make the
// user confirm. Asked while no walker is connected. Returns 1 to go ahead.
static int confirm_takeover(const pw_identity *id, const u8 *route, const char *action)
{
	if (!(id->flags & PW_IDFLAG_HAS_POKE))
		return 1;
	print_stroll(route);
	if (walk_is_ours(id, route)) {
		printf("This walk was started by PocketPKMN.\n");
		return 1;
	}
	printf("\nThis Pokemon came from a game\n(HeartGold/SoulSilver). If you\n%s, that game can NOT take\nit back afterwards.\n", action);
	return ask("Continue anyway?", 1);
}

// --- full backup -------------------------------------------------------------------

// Two complete passes; chunks that differ are re-read until two reads agree.
static int full_read(session *s, u8 *buf)
{
	int r = 0;
	unsigned mismatches = 0;
	u64 t0 = dbg_now_us();
	s->link.read_retries = 0;
	trace_quiet = 1;
	for (unsigned a = 0; a < PW_EEPROM_SIZE && r == 0; a += CHUNK) {
		r = read_chunk(s, a, buf + a);
		if ((a & 0x7FF) == 0x780) {
			printf("\rReading  %3u%%", (a + CHUNK) * 100 / PW_EEPROM_SIZE);
			refresh();
		}
	}
	if (r == 0)
		printf("\n");
	for (unsigned a = 0; a < PW_EEPROM_SIZE && r == 0; a += CHUNK) {
		u8 again[CHUNK], third[CHUNK];
		r = read_chunk(s, a, again);
		if (r == 0 && memcmp(again, buf + a, CHUNK) != 0) {
			mismatches++;
			dbg_printf("verify: 0x%04x differs between two reads", a);
			int agreed = 0;
			for (int k = 0; k < 6 && r == 0 && !agreed; k++) {
				r = read_chunk(s, a, third);
				if (r == 0 && memcmp(third, again, CHUNK) == 0) {
					memcpy(buf + a, third, CHUNK);
					agreed = 1;
				}
				memcpy(again, third, CHUNK);
			}
			if (r == 0 && !agreed) {
				dbg_printf("verify: 0x%04x never read the same twice", a);
				r = PW_ERR_CHECKSUM;
			}
		}
		if ((a & 0x7FF) == 0x780) {
			printf("\rChecking %3u%%", (a + CHUNK) * 100 / PW_EEPROM_SIZE);
			refresh();
		}
	}
	printf("\n");
	trace_quiet = 0;
	dbg_printf("full read finished with %s after %lu ms: %u retried reads, %u reconnects, %u chunk(s) "
	           "differed on the check pass", pw_strerror(r), (unsigned long)((dbg_now_us() - t0) / 1000),
	           s->link.read_retries, s->reconnects, mismatches);
	return r;
}

// Save buf as the next free eeprom_NNN.bin. Returns 0 and the path.
static int save_backup(const u8 *buf, char *path, size_t path_size)
{
	ensure_dir();
	FILE *f = NULL;
	for (int n = 1; n < 1000; n++) {
		snprintf(path, path_size, APP_DIR "/eeprom_%03d.bin", n);
		f = fopen(path, "rb");
		if (!f)
			break;
		fclose(f);
		f = NULL;
		path[0] = '\0';
	}
	if (!path[0])
		return -1;
	f = fopen(path, "wb");
	if (!f)
		return -1;
	int ok = fwrite(buf, 1, PW_EEPROM_SIZE, f) == PW_EEPROM_SIZE;
	if (fclose(f) != 0)
		ok = 0;
	return ok ? 0 : -1;
}

// Every operation that writes runs in two connections. A real walker drops a
// link that is silent for well under half a second (test 6: no answer 0.4 s
// after its last reply), so nothing slow - saving to the SD card, waiting for
// the user - may happen while it is connected.
//   Part 1: connect, full backup, let the walker go; then save the backup and
//           ask the questions with no walker connected.
//   Part 2: the user chooses CONNECT again; the app does the writes in one go.

static int part1_backup(const char *title, const char *what, pw_identity *id, u8 *id_raw)
{
	session s;
	char t[64];
	snprintf(t, sizeof(t), "%s\nPart 1 of 2: safety backup", title);
	int r = session_open(&s, t, what);
	if (r == 0) {
		print_identity(&s.id);
		printf("\nKeep the walker still (~25 s).\n");
		refresh();
		r = full_read(&s, eeprom);
		if (r == 0) {
			*id = s.id;
			memcpy(id_raw, s.id_raw, PW_IDENTITY_SIZE);
		}
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	if (r < 0) {
		printf("Backup failed: %s\nNothing was written.\n", pw_strerror(r));
		return r;
	}
	char path[96];
	if (save_backup(eeprom, path, sizeof(path)) < 0) {
		printf("Could not save the backup!\nNothing was written.\n");
		return -1;
	}
	printf("Saved %s\nYou can put the walker down.\n", path + 5);
	dbg_printf("safety backup saved %s", path);
	flush_log();
	return 0;
}

// Part 2: connect again and check that it is the same walker as in part 1.
static int part2_open(session *s, const char *title, const char *what, const pw_identity *want)
{
	char t[64];
	snprintf(t, sizeof(t), "%s\nPart 2 of 2", title);
	int r = session_open(s, t, what);
	if (r == 0 && memcmp(s->id.unique_id, want->unique_id, 0x28) != 0) {
		printf("A different walker answered!\nNothing was written.\n");
		dbg_printf("part 2: unique id differs from part 1");
		r = PW_ERR_UNEXPECTED;
	}
	if (r == 0)
		printf("Connected. Keep the walker still.\n");
	return r;
}

// --- operations --------------------------------------------------------------------

static void do_link_test(void)
{
	session s;
	int r = session_open(&s, "LINK TEST (read only)", "link test");
	if (r == 0) {
		print_identity(&s.id);
		refresh();
		u8 health[0x18], watts[2], route[0xBE];
		r = pw_eeprom_read(&s.link, PW_EE_HEALTH_DATA, health, sizeof(health));
		if (r == 0)
			r = pw_eeprom_read(&s.link, EE_WATTS, watts, sizeof(watts));
		if (r == 0)
			r = pw_eeprom_read(&s.link, EE_ROUTE, route, sizeof(route));
		int has_poke = 0;
		if (r == 0) {
			printf("Watts: %u\n", (unsigned)((watts[0] << 8) | watts[1]));
			if (s.id.flags & PW_IDFLAG_HAS_POKE) {
				print_stroll(route);
				has_poke = 1;
			} else {
				printf("No Pokemon on the walker.\n");
			}
			r = pw_ping(&s.link);
		}
		if (r < 0) {
			printf("Read failed: %s\n", pw_strerror(r));
			dbg_printf("read phase failed: %s", pw_strerror(r));
		} else {
			printf("\nLINK TEST PASSED\n");
			dbg_printf("link test passed");
		}
		pw_identity idc = s.id;
		session_close(&s, r != PW_ERR_CANCELLED);
		// v0.50 (audit S5): the ledger is read only after the link is closed
		if (has_poke)
			printf(walk_is_ours(&idc, route) ? "(walk started by PocketPKMN)\n" : "(walk from a game)\n");
		wait_key();
		return;
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	wait_key();
}

static void do_backup(void)
{
	session s;
	int r = session_open(&s, "BACKUP (read only)", "eeprom backup");
	if (r == 0) {
		print_identity(&s.id);
		printf("\nKeep the walker still.\n");
		refresh();
		r = full_read(&s, eeprom);
		pw_disconnect(&s.link);
		session_close(&s, 0); // v0.50 (audit S5): the file is written after the link is closed
		if (r == 0) {
			char path[96];
			if (save_backup(eeprom, path, sizeof(path)) == 0) {
				printf("Saved %s\n", path + 5);
				dbg_printf("saved %s", path);
				int menus = checked_block_ok(eeprom), walk = walk_valid(eeprom);
				printf("Walker files in it: menus+sounds %s,\nwalk %s\n", menus ? "yes" : "no",
				       walk ? "yes" : "no");
			} else {
				printf("Could not write the backup file!\n");
			}
			printf("Retried reads: %u  Reconnects: %u\n", s.link.read_retries, s.reconnects);
		} else {
			printf("Backup failed: %s\n", pw_strerror(r));
		}
		wait_key();
		return;
	}
	session_close(&s, 0);
	wait_key();
}

// Writes a pattern into the walk-start staging area, reads it back, then puts
// the original bytes back. The walker only reads that area on command 5A.
static void do_write_test(void)
{
	session s;
	int r = session_open(&s, "WRITE TEST (staging area only)", "write test");
	if (r == 0) {
		print_identity(&s.id);
		u8 orig[WRITE_TEST_SIZE], pattern[WRITE_TEST_SIZE], back[WRITE_TEST_SIZE];
		for (unsigned i = 0; i < WRITE_TEST_SIZE; i++)
			pattern[i] = (u8)(0x5A ^ (i * 29));
		r = read_range(&s, WRITE_TEST_ADDR, orig, WRITE_TEST_SIZE);
		dbg_printf("write test: original read %s", pw_strerror(r));
		if (r == 0) {
			s.link.read_retries = 0;
			r = pw_eeprom_write(&s.link, WRITE_TEST_ADDR, pattern, WRITE_TEST_SIZE, 1);
			dbg_printf("write test: pattern write %s (%u retries)", pw_strerror(r), s.link.read_retries);
			printf("Pattern written and checked: %s\n", r == 0 ? "OK" : pw_strerror(r));
		}
		if (r == 0) {
			r = read_range(&s, WRITE_TEST_ADDR, back, WRITE_TEST_SIZE);
			if (r == 0 && memcmp(back, pattern, WRITE_TEST_SIZE) != 0)
				r = PW_ERR_VERIFY;
			printf("Read back separately: %s\n", r == 0 ? "OK" : pw_strerror(r));
		}
		// Put the original bytes back whatever happened above (if we can still talk).
		if (r != PW_ERR_CANCELLED) {
			int r2 = pw_eeprom_write(&s.link, WRITE_TEST_ADDR, orig, WRITE_TEST_SIZE, 1);
			dbg_printf("write test: restore %s", pw_strerror(r2));
			printf("Original bytes put back: %s\n", r2 == 0 ? "OK" : pw_strerror(r2));
			if (r == 0)
				r = r2;
		}
		if (r == 0)
			printf("\nWRITE TEST PASSED\n");
		else
			printf("\nWrite test failed: %s\n", pw_strerror(r));
		dbg_printf("write test %s", r == 0 ? "passed" : pw_strerror(r));
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	wait_key();
}

// --- restore -----------------------------------------------------------------------

#define MAX_FILES 64
static char files[MAX_FILES][32];
static int file_count;

static int cmp_names(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

static void list_backups(void)
{
	file_count = 0;
	DIR *d = opendir(APP_DIR);
	if (!d)
		return;
	struct dirent *e;
	while ((e = readdir(d)) && file_count < MAX_FILES)
		if (!strncmp(e->d_name, "eeprom_", 7) && strstr(e->d_name, ".bin") && strlen(e->d_name) < 32)
			strcpy(files[file_count++], e->d_name);
	closedir(d);
	qsort(files, (size_t)file_count, sizeof(files[0]), cmp_names);
}

static int load_backup(const char *name)
{
	char path[96];
	snprintf(path, sizeof(path), APP_DIR "/%s", name);
	FILE *f = fopen(path, "rb");
	if (!f)
		return -1;
	size_t n = fread(image, 1, PW_EEPROM_SIZE, f);
	fclose(f);
	return (n == PW_EEPROM_SIZE && !memcmp(image, "nintendo", 8)) ? 0 : -1;
}

static int choose_backup_ex(const char *title, const char *help, int allow_y);

static int choose_backup(void)
{
	return choose_backup_ex("RESTORE A BACKUP",
	                        "Writes a backup file back to the\nwalker (everything from 0x0280 up;\n"
	                        "the walker's own ID/steps header\nis left alone).\n",
	                        0);
}

// Pick a backup file (loaded into `image`). Returns its index, -1 for B, and
// -2 for Y when allow_y is set.
static int choose_backup_list(const char *title, const char *help, int allow_y);
static int choose_backup_ex(const char *title, const char *help, int allow_y)
{
	ui_legacy_raw(1); // v0.44: a cursor list in console columns - the old text view
	int r = choose_backup_list(title, help, allow_y);
	ui_legacy_raw(0);
	return r;
}

static int choose_backup_list(const char *title, const char *help, int allow_y)
{
	list_backups();
	if (!file_count) {
		if (allow_y)
			return -2;
		printf("No backups found in /3ds/pocketpkmn.\n");
		return -1;
	}
	int sel = file_count - 1; // newest
	for (;;) {
		consoleSelect(&top);
		consoleClear();
		printf("%s\n\n%s\n", title, help);
		printf("File:  < %s >  (%d of %d)\n\n", files[sel], sel + 1, file_count);
		printf("Left/Right: choose   A: use   B: back\n");
		if (allow_y)
			printf("Y: no file\n");
		refresh();
		for (;;) {
			hidScanInput();
			u32 down = hidKeysDown();
			if (down & KEY_B)
				return -1;
			if (allow_y && (down & KEY_Y))
				return -2;
			if (down & (KEY_LEFT | KEY_RIGHT)) {
				sel = (sel + ((down & KEY_RIGHT) ? 1 : file_count - 1)) % file_count;
				break;
			}
			if (down & KEY_A) {
				if (load_backup(files[sel]) < 0) {
					printf("\nThat file is not a walker backup.\n");
					refresh();
					break;
				}
				return sel;
			}
			gspWaitForVBlank();
		}
	}
}

static void do_restore(void)
{
	int sel = choose_backup();
	if (sel < 0) {
		wait_key();
		return;
	}
	if (!ask("Restore this backup to the walker?", 1))
		return;
	char chosen[32];
	snprintf(chosen, sizeof(chosen), "%s", files[sel]);
	dbg_printf("restore from %s", chosen);
	// `image` holds the chosen file; part 1 reads the walker into `eeprom`.

	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	if (part1_backup("RESTORE", "restore (backup)", &id, id_raw) < 0) {
		wait_key();
		return;
	}
	if (memcmp(image + EE_UNIQUE_ID, id.unique_id, 0x28) != 0) {
		printf("\nThis backup is from a DIFFERENT\nwalker (or a different pairing).\n");
		if (!ask("Write it to this walker anyway?", 1))
			return;
	}
	unsigned todo = 0, done = 0;
	for (unsigned a = EE_RESTORE_FROM; a < PW_EEPROM_SIZE; a += CHUNK)
		todo += memcmp(image + a, eeprom + a, CHUNK) != 0;
	printf("\n%u block(s) of 128 bytes differ.\n", todo);
	if (!todo) {
		printf("The walker already matches the\nbackup. Nothing to do.\n");
		wait_key();
		return;
	}
	if (!ask("Write them now?", 0))
		return;

	session s;
	int r = part2_open(&s, "RESTORE", "restore", &id);
	if (r == 0) {
		dbg_printf("restore: %u block(s) differ", todo);
		s.link.read_retries = 0;
		trace_quiet = 1;
		for (unsigned a = EE_RESTORE_FROM; a < PW_EEPROM_SIZE && r == 0; a += CHUNK) {
			if (!memcmp(image + a, eeprom + a, CHUNK))
				continue;
			r = write_chunk(&s, a, image + a);
			if (r < 0)
				dbg_printf("restore: block 0x%04x failed: %s", a, pw_strerror(r));
			done++;
			printf("\rWriting %3u%%", done * 100 / todo);
			refresh();
		}
		trace_quiet = 0;
		printf("\n");
		dbg_printf("restore finished: %s, %u of %u blocks, %u retries, %u reconnects", pw_strerror(r), done,
		           todo, s.link.read_retries, s.reconnects);
		if (r == 0) {
			printf("\nRESTORE DONE (%u blocks written\nand checked).\n", done);
			ledger("restore walker from %s: %u blocks", chosen, done);
		} else {
			printf("\nRestore stopped: %s\nThe safety backup has the state\nfrom before; run Restore again.\n",
			       pw_strerror(r));
		}
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	wait_key();
}

// --- walk start / walk end -----------------------------------------------------------

// Writes one block; if the link had to be made again on the way, sends the
// identity command again too (the walker takes the trainer data for the walk
// from the last identity it got, so it must come from this connection).
static int write_block_ident(session *s, unsigned addr, const u8 *data, u8 ident_cmd, const u8 *ident,
                             unsigned *ident_at)
{
	int r = write_chunk(s, addr, data);
	if (r == 0 && s->reconnects != *ident_at) {
		r = pw_send_identity(&s->link, ident_cmd, ident);
		dbg_printf("identity %02x again after a reconnect: %s", ident_cmd, pw_strerror(r));
		*ident_at = s->reconnects;
	}
	return r;
}

// Sends `ident_cmd` again if the link was re-made since it was last sent, then
// a ping; the game always pings before its final command.
static int ready_for_final(session *s, u8 ident_cmd, const u8 *ident, unsigned *ident_at)
{
	for (int tries = 0; tries < 2; tries++) {
		int r = 0;
		if (s->reconnects != *ident_at) {
			r = pw_send_identity(&s->link, ident_cmd, ident);
			*ident_at = s->reconnects;
		}
		if (r == 0)
			r = pw_ping(&s->link);
		if (r == 0)
			return 0;
		r = session_reconnect(s, r);
		if (r < 0)
			return r;
	}
	return PW_ERR_TIMEOUT;
}

// Walk start test: uploads the walker's last walk (route, pictures, team)
// through the staging area and starts it with command 5A, exactly the way the
// game starts a walk. A Pokemon from the app's storage replaces this later.
static void do_walk_start(void)
{
	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	if (part1_backup("WALK START TEST", "walk start (backup)", &id, id_raw) < 0) {
		wait_key();
		return;
	}
	// The walk to send: the walker's own last walk, or - after a walk end, which
	// clears the Pokemon's species, level and moves at 0x8F00 (test 8) - the
	// newest backup of this same walker that still holds a walk.
	const u8 *walk_src = eeprom;
	char walk_file[32] = "";
	if (!walk_valid(eeprom)) {
		walk_src = NULL;
		list_backups();
		for (int i = file_count - 1; i >= 0 && !walk_src; i--) {
			if (load_backup(files[i]) == 0 && walk_valid(image) &&
			    !memcmp(image + EE_UNIQUE_ID, id.unique_id, 0x28)) {
				walk_src = image;
				snprintf(walk_file, sizeof(walk_file), "%s", files[i]);
			}
		}
		if (!walk_src) {
			printf("\nThe walker has no walk to\nre-send, and no backup of it has\none. (Sending a Pokemon from\n"
			       "the slot comes later.)\n");
			wait_key();
			return;
		}
		printf("\nThe walker's last walk was\ncleared; using the walk from\n%s.\n", walk_file);
		dbg_printf("walk start: walk taken from %s", walk_file);
	}
	const u8 *route = walk_src + EE_ROUTE;
	if (!confirm_takeover(&id, route, "start a PocketPKMN walk"))
		return;
	unsigned watts = (unsigned)((eeprom[EE_WATTS] << 8) | eeprom[EE_WATTS + 1]);
	print_stroll(route);
	printf("\nStarting a walk resets the walk's\nsteps and watts on the walker;\n"
	       "PocketPKMN keeps the %u watt(s).\n", watts);
	if (!ask("Start the walk now?", 0))
		return;

	session s;
	int r = part2_open(&s, "WALK START TEST", "walk start", &id);
	if (r == 0) {
		// Same order as the game: identity 52, staged walk data, staged team, ping, 5A.
		unsigned ident_at = s.reconnects;
		r = pw_send_identity(&s.link, PW_CMD_IDENTITY_SEND, id_raw);
		dbg_printf("walk start: identity 52 %s", pw_strerror(r));
		trace_quiet = 1;
		s.link.read_retries = 0;
		const unsigned total = (EE_WALK_SIZE + EE_TEAM_SIZE) / CHUNK;
		unsigned sent = 0;
		for (unsigned i = 0; i < EE_WALK_SIZE && r == 0; i += CHUNK) {
			r = write_block_ident(&s, EE_STAGE_WALK + i, walk_src + EE_ROUTE + i, PW_CMD_IDENTITY_SEND, id_raw,
			                      &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / total);
			refresh();
		}
		for (unsigned i = 0; i < EE_TEAM_SIZE && r == 0; i += CHUNK) {
			r = write_block_ident(&s, EE_STAGE_TEAM + i, walk_src + EE_TEAM + i, PW_CMD_IDENTITY_SEND, id_raw,
			                      &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / total);
			refresh();
		}
		trace_quiet = 0;
		printf("\n");
		dbg_printf("walk start: upload %s (%u retries, %u reconnects)", pw_strerror(r), s.link.read_retries,
		           s.reconnects);
		if (r == 0)
			r = ready_for_final(&s, PW_CMD_IDENTITY_SEND, id_raw, &ident_at);
		if (r == 0) {
			r = pw_walk_start(&s.link);
			dbg_printf("walk start: 5A %s", pw_strerror(r));
		}
		if (r == 0) {
			ledger("walk-start walker=%08lx route=%08lx watts-banked=%u", walker_key(&id),
			       (unsigned long)fnv1a(route, 0xBE), watts);
			printf("\nWALK STARTED. %u watt(s) kept\nby PocketPKMN (see ledger.txt).\n", watts);
		} else {
			printf("\nWalk start failed: %s\n", pw_strerror(r));
			printf("If it failed before the last step,\nthe walker is unchanged.\n");
		}
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	wait_key();
}

// --- send a Pokemon from the WalkerBox bank (W3) -------------------------------------

static void draw_walker_image(const u8 *img, int w, int h, int sx, int sy, int scale);

// Ledger line of a bank walk: "walk-start walker=K route=R watts-banked=W bank=B/S pk=F [steps-start=N ev=E]"
// (B, S from 1; steps-start/ev since v0.30). Finds the newest such line for this walker and route. Returns 1 and
// box/slot (from 0) + fingerprint; *steps_start = -1 and *ev = -1 when the line has none.
static int bank_walk_of_ex(const pw_identity *id, const u8 *route, int *box, int *slot, unsigned long *fp,
                           long *steps_start, int *ev);
static int bank_walk_of(const pw_identity *id, const u8 *route, int *box, int *slot, unsigned long *fp)
{
	return bank_walk_of_ex(id, route, box, slot, fp, NULL, NULL);
}

static int bank_walk_of_ex(const pw_identity *id, const u8 *route, int *box, int *slot, unsigned long *fp,
                           long *steps_start, int *ev)
{
	char want[96], line[256];
	snprintf(want, sizeof(want), "walk-start walker=%08lx route=%08lx", walker_key(id),
	         (unsigned long)fnv1a(route, 0xBE));
	FILE *f = fopen(LEDGER_PATH, "r");
	if (!f)
		return 0;
	int found = 0;
	while (fgets(line, sizeof(line), f)) {
		if (!strstr(line, want))
			continue;
		const char *b = strstr(line, " bank=");
		int bb, ss;
		unsigned long pk;
		if (b && sscanf(b, " bank=%d/%d pk=%lx", &bb, &ss, &pk) == 3) {
			*box = bb - 1;
			*slot = ss - 1;
			*fp = pk;
			found = 1;
			const char *st = strstr(line, " steps-start=");
			const char *e = strstr(line, " ev=");
			unsigned long n;
			int k;
			if (steps_start)
				*steps_start = st && sscanf(st, " steps-start=%lu", &n) == 1 ? (long)n : -1;
			if (ev)
				*ev = e && sscanf(e, " ev=%d", &k) == 1 ? k : -1;
		} else {
			found = 0; // a newer walk-start test walk with the same route replaces it
		}
	}
	fclose(f);
	return found;
}

static int team_valid(const u8 *ee)
{
	return ee[EE_TEAM] == 1 && ee[EE_TEAM + 1] == 0 && ee[EE_TEAM + 2] == 0 && ee[EE_TEAM + 3] == 0;
}

static int walk_ev = -1; // EV walking stat chosen at send: -1 off, 0 HP .. 5 Spe (storage.h)
static const char *const k_ev_names[6] = { "HP", "Atk", "Def", "SpA", "SpD", "Spe" };

#define MAX_WALK_ITEMS 900
static storage_walk_item walk_items[MAX_WALK_ITEMS];
static wbx_walk_poke walk_poke;
static wbx_route walk_route;
static route_file walk_routes; // v0.41: the course list of the walking Pokemon's game
static char walk_course[32] = "Test Path";
static u8 walk_blob[WBX_WALK_SIZE];

// Bank list on the bottom screen, the chosen Pokemon's walker pictures on the top screen.
// Returns the index in walk_items, or -1.
__attribute__((unused)) static int pick_bank_pokemon(int count)
{
	int sel = 0, shown = -1, ok = 0, from_pack = 0, frame = 0;
	char why[96] = "";
	u64 last_flip = osGetTime();
	const int rows = 18;
	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_B)
			return -1;
		if (down & KEY_A && ok && !walk_items[sel].walking)
			return sel;
		int step = (down & KEY_DOWN) ? 1 : (down & KEY_UP) ? -1 : (down & KEY_R) ? rows : (down & KEY_L) ? -rows : 0;
		if (step)
			sel = (sel + step % count + count) % count;
		int redraw = 0;
		if (down & KEY_X) { // W4: EV walking stat (Off, HP .. Spe); none for Gen 1/2
			walk_ev = storage_walk_ev_cap(walk_items[sel].box, walk_items[sel].slot) ? (walk_ev + 2) % 7 - 1 : -1;
			shown = -1;
		}
		if (sel != shown) {
			shown = sel;
			ok = storage_walk_prepare(walk_items[sel].box, walk_items[sel].slot, &walk_poke, &from_pack, why,
			                          sizeof(why)) == 0;
			redraw = 1;
			consoleSelect(&bottom);
			consoleClear();
			printf("SEND TO WALKER - choose a Pokemon\n\n");
			int first = sel / rows * rows;
			for (int i = first; i < first + rows && i < count; i++)
				printf("%c%2d/%-2d %s%s\n", i == sel ? '>' : ' ', walk_items[i].box + 1, walk_items[i].slot + 1,
				       walk_items[i].label, walk_items[i].walking ? " walking" : "");
			printf("\nUp/Down  L/R: page  A: send  B: back\n");
			if (!storage_walk_ev_cap(walk_items[sel].box, walk_items[sel].slot))
				walk_ev = -1;
			printf("X: EV training: %s%s\n", walk_ev < 0 ? "Off" : k_ev_names[walk_ev],
			       storage_walk_ev_cap(walk_items[sel].box, walk_items[sel].slot) ? " (+1 per 20 steps)"
			                                                                      : " (none in Gen 1/2)");
			consoleSelect(&top);
			consoleClear();
			printf("\x1b[23;0H"); // below the pictures (they end at y = 164)
			if (walk_items[sel].walking)
				printf("Already on a walk.\n");
			else if (!ok)
				printf("Cannot walk: %s\n", why);
			else {
				char nick[16];
				pw_text_to_ascii(walk_poke.nickname, 11, nick, sizeof(nick));
				printf("\"%s\"  Gen %d  friendship %u\n", nick, walk_items[sel].gen, walk_poke.friendship);
				char ot[24];
				unsigned otid = 0;
				if (storage_walk_origin(walk_items[sel].box, walk_items[sel].slot, ot, sizeof(ot), &otid, NULL, NULL))
					printf("From the save of %s (ID %05u)\n", ot, otid);
				else
					printf("Deposited before v0.28: no origin.\n");
				if (!from_pack)
					printf("No sprite file: PocketPKMN placeholder.\n");
			}
		}
		if (osGetTime() - last_flip >= 500) {
			frame ^= 1;
			last_flip = osGetTime();
			redraw = 1;
		}
		if (redraw && ok) {
			draw_walker_image(walk_poke.sprite + WBX_SMALL_SIZE + frame * 0x300, 64, 48, 16, 20, 3);
			draw_walker_image(walk_poke.sprite + frame * 0xC0, 32, 24, 232, 20, 3);
			draw_walker_image(walk_poke.name_img, 80, 16, 216, 110, 2);
			refresh();
		}
		gspWaitForVBlank();
	}
	return -1;
}

// v0.46: which route Pokemon have been caught on which course (the ledger's walk-start course="..." line followed by
// its walk-end "  caught #N" lines). Read before the send screen opens (never during IR).
#define SEEN_MAX 512
static struct {
	char course[32];
	unsigned short species;
} seen_list[SEEN_MAX];
static int seen_n;

static void seen_load(void)
{
	seen_n = 0;
	FILE *f = fopen(LEDGER_PATH, "r");
	if (!f)
		return;
	char line[256], course[32] = "";
	while (fgets(line, sizeof(line), f)) {
		const char *cs = strstr(line, " course=\"");
		if (strstr(line, "walk-start ")) {
			course[0] = 0;
			if (cs) {
				cs += 9;
				size_t q = 0;
				while (cs[q] && cs[q] != '"' && q + 1 < sizeof(course)) {
					course[q] = cs[q];
					q++;
				}
				course[q] = 0;
			}
			continue;
		}
		unsigned sp;
		const char *c = strstr(line, "  caught #");
		if (c && course[0] && sscanf(c, "  caught #%u", &sp) == 1 && seen_n < SEEN_MAX) {
			snprintf(seen_list[seen_n].course, sizeof(seen_list[0].course), "%s", course);
			seen_list[seen_n++].species = (unsigned short)sp;
		}
	}
	fclose(f);
}

static int seen_on(const char *course, int species)
{
	for (int i = 0; i < seen_n; i++)
		if (seen_list[i].species == species && !strcmp(seen_list[i].course, course))
			return 1;
	return 0;
}

// v0.37: the pick on GPU screens (source/ui_pick.c). Same steps as storage_pick_for_walk: the walker slot's
// state first (walking -> refuse; a Pokemon waiting from a walk -> A send it again / X put it back), then game ->
// box -> Pokemon -> confirm -> storage_pick_commit. Returns 1 when the walker slot holds the Pokemon to send.
static int pick_for_walk(void)
{
	ui_loading("Getting ready…"); // v0.43: no frozen screen while the slot and saves are read
	int count = storage_walk_list(walk_items, MAX_WALK_ITEMS);
	if (count < 0) {
		printf("SEND TO WALKER\n\nCould not load the Pokemon data\n(romfs) or the bank. See the log.\n");
		wait_key();
		return 0;
	}
	for (int i = 0; i < count; i++) {
		if (walk_items[i].box != STORAGE_WALK_BOX || walk_items[i].slot != STORAGE_WALK_SLOT)
			continue;
		if (walk_items[i].walking) {
			printf("SEND TO WALKER\n\nA Pokemon is already on a walk.\nEnd that walk first.\n");
			wait_key();
			return 0;
		}
		// v0.43: a GPU popup instead of the text screen (same choices: A send again, X put it back, B back)
		dbg_printf("send: the walker slot holds %s (waiting)", walk_items[i].label);
		char why0[96], label[48], nick[16] = "";
		int fp0 = 0;
		int have = storage_walk_prepare(walk_items[i].box, walk_items[i].slot, &walk_poke, &fp0, why0, sizeof(why0)) == 0;
		if (have)
			pw_text_to_ascii(walk_poke.nickname, 11, nick, sizeof(nick));
		snprintf(label, sizeof(label), "%s  Lv.%u", nick[0] ? nick : "It", have ? walk_poke.summary[12] : 0);
		int r = ui_slot_waiting(have ? walk_poke.sprite : NULL, label, storage_return_to_game);
		flush_log();
		return r;
	}
	char why[160];
	int r = ui_pick_for_walk(why, sizeof(why));
	if (r < 0) {
		printf("SEND TO WALKER\n\n%s\n", why);
		wait_key();
	}
	return r == 1;
}

static void do_send_walk(void)
{
	consoleSelect(&top);
	consoleClear();
	if (!have_trainer) {
		printf("SEND TO WALKER\n\nNo PocketPKMN trainer yet (the name\nasked at start-up). Restart\n"
		       "PocketPKMN to make one.\n");
		wait_key();
		return;
	}
	// One-slot flow (2026-11-15): choose a game and a Pokemon in its boxes; it moves into the walker slot.
	if (!pick_for_walk()) {
		flush_log();
		return;
	}
	flush_log();
	// The walker slot as a one-item list for the preview / EV screen.
	int count = storage_walk_list(walk_items, MAX_WALK_ITEMS), pick = -1;
	for (int i = 0; i < count && pick < 0; i++)
		if (walk_items[i].box == STORAGE_WALK_BOX && walk_items[i].slot == STORAGE_WALK_SLOT)
			pick = i;
	if (pick < 0) {
		consoleSelect(&top);
		consoleClear();
		printf("SEND TO WALKER\n\nThe walker slot could not be read.\nSee the log.\n");
		wait_key();
		return;
	}
	walk_items[0] = walk_items[pick];
	// v0.41: the send screen (mock_4): course of the Pokemon's game + EV stat. Gen 1/2: no EVs.
	int course = -1, course_gen = walk_items[0].gen;
	const char *course_ver = "all";
	pick = -1;
	{
		char why0[96];
		int fp0 = 0;
		if (storage_walk_prepare(walk_items[0].box, walk_items[0].slot, &walk_poke, &fp0, why0, sizeof(why0)) == 0) {
			ui_send_info si;
			memset(&si, 0, sizeof(si));
			si.sprite = walk_poke.sprite;
			si.ev_allowed = storage_walk_ev_cap(walk_items[0].box, walk_items[0].slot) != 0;
			char nick0[16], ot[24] = "", game[64] = "its game";
			unsigned long long tid = 0;
			pw_text_to_ascii(walk_poke.nickname, 11, nick0, sizeof(nick0));
			const char *file = NULL;
			if (storage_walk_origin(walk_items[0].box, walk_items[0].slot, ot, sizeof(ot), NULL, NULL, &tid)) {
				ui_title t;
				if (ui_title_load(&t, tid)) {
					snprintf(game, sizeof(game), "%s", t.name);
					ui_title_free(&t);
				}
				file = routes_file_for(tid, course_gen, &course_ver);
			}
			si.rf = (file && routes_load(file, &walk_routes) > 0) ? &walk_routes : NULL;
			si.gen = course_gen;
			seen_load();
			si.seen = seen_on;
			dbg_printf("send walk: routes %s (%s, version %s): %d course(s)", file ? file : "none", game, course_ver,
			           si.rf ? walk_routes.ncourse : 0);
			snprintf(si.line1, sizeof(si.line1), "%s  Lv.%u  \xC2\xB7  from %s%s%s%s", nick0, walk_poke.summary[12],
			         game, ot[0] ? " (" : "", ot, ot[0] ? ")" : "");
			if (!si.ev_allowed)
				walk_ev = -1;
			if (ui_send_run(&si, &course, &walk_ev))
				pick = 0;
			if (!si.rf)
				course = -1;
		}
	}
	consoleSelect(&top);
	consoleClear();
	if (pick < 0) {
		// Not sent after all: offer to put it straight back into its game.
		printf("Not sent.\n");
		if (ask("Put it back into its game now?\n(B: it waits in the walker slot)", 0)) {
			char m[160];
			storage_return_to_game(m, sizeof(m));
			printf("\n%s\n", m);
			flush_log();
			wait_key();
		}
		return;
	}
	const storage_walk_item it = walk_items[pick];
	char why[96];
	int from_pack = 0;
	if (storage_walk_prepare(it.box, it.slot, &walk_poke, &from_pack, why, sizeof(why)) != 0) {
		printf("Cannot walk: %s\n", why);
		wait_key();
		return;
	}
	unsigned long fp = storage_walk_fingerprint(it.box, it.slot);
	// v0.41: the chosen course of the Pokemon's game (one of two Pokemon per group, picked now), else Test Path
	snprintf(walk_course, sizeof(walk_course), "Test Path");
	if (course >= 0 && course < walk_routes.ncourse &&
	    routes_build(&walk_routes.course[course], course_ver, course_gen, (unsigned)svcGetSystemTick(), &walk_route) == 0)
		snprintf(walk_course, sizeof(walk_course), "%s", walk_routes.course[course].name);
	else
		wbx_route_test(&walk_route);
	wbx_walk_compose(NULL, &walk_poke, &walk_route, walk_blob);
	char nick[16];
	pw_text_to_ascii(walk_poke.nickname, 11, nick, sizeof(nick));
	dbg_printf("send walk: bank %d/%d \"%s\" %s, pk=%08lx, sprite %s, route %s", it.box + 1, it.slot + 1, nick,
	           it.label, fp, from_pack ? "from the pack" : "PLACEHOLDER", walk_course);
	printf("Send \"%s\" (%s)\non the route \"%s\".\n\n", nick, it.label, walk_course);

	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	if (part1_backup("SEND TO WALKER", "send walk (backup)", &id, id_raw) < 0) {
		wait_key();
		return;
	}
	// Whose walker is it? (decision 2026-11-15: walkers WalkerBox uses belong to the WalkerBox trainer)
	const int paired = (id.flags & PW_IDFLAG_PAIRED) != 0;
	if (!paired && !checked_block_ok(eeprom)) {
		printf("\nThis walker has no menu pictures.\nUse \"Set up a walker\" first.\nNothing was written.\n");
		wait_key();
		return;
	}
	if (paired && !wbx_trainer_owns(&trainer, id_raw)) {
		char owner[12];
		pw_text_to_ascii(id.trainer_name, 8, owner, sizeof(owner));
		printf("\nThis walker belongs to %s\n(ID %05u). After this walk it\nbelongs to PocketPKMN's trainer %s.\n"
		       "The game it belonged to will have\nto reset it before using it again.\n",
		       owner, id.tid, trainer.name);
		if (!ask("Make it PocketPKMN's?", 1))
			return;
	} else if (!paired) {
		printf("\nThis walker is not set up (reset).\nPocketPKMN sets it up for trainer\n%s with this walk.\n",
		       trainer.name);
	}
	// What is on the walker now?
	const u8 *route_now = eeprom + EE_ROUTE;
	if (paired && (id.flags & PW_IDFLAG_HAS_POKE)) {
		int ob, os;
		unsigned long ofp;
		if (bank_walk_of(&id, route_now, &ob, &os, &ofp)) {
			print_stroll(route_now);
			printf("\nThat is a PocketPKMN walk of the\nPokemon in box %d slot %d.\nEnd that walk first (End walk).\n",
			       ob + 1, os + 1);
			wait_key();
			return;
		}
		if (!confirm_takeover(&id, route_now, "send a PocketPKMN Pokemon"))
			return;
	}
	// TeamData: the walker's own (decision 2026-11-14), or the newest backup of this walker that has one.
	const u8 *team = eeprom + EE_TEAM;
	if (!team_valid(eeprom)) {
		team = NULL;
		list_backups();
		for (int i = file_count - 1; i >= 0 && !team; i--)
			if (load_backup(files[i]) == 0 && team_valid(image) && !memcmp(image + EE_UNIQUE_ID, id.unique_id, 0x28))
				team = image + EE_TEAM;
		if (!team) {
			printf("\nThe walker has no team data and\nno backup of it has any.\nNothing was written.\n");
			wait_key();
			return;
		}
		dbg_printf("send walk: team data taken from a backup");
	}
	unsigned watts = (unsigned)((eeprom[EE_WATTS] << 8) | eeprom[EE_WATTS + 1]);
	printf("\nStarting a walk resets the walk's\nsteps and watts on the walker;\n"
	       "PocketPKMN keeps the %u watt(s).\n", watts);
	if (!ask("Send it now?", 0))
		return;

	// Identity: the walker's own with the WalkerBox trainer put in (TID, SID, name). A reset walker is
	// paired with 32 + 38 instead of 52 + 5A; its version words get what "Set up a walker" sends.
	u8 ident[PW_IDENTITY_SIZE], base[PW_IDENTITY_SIZE];
	memcpy(base, id_raw, sizeof(base));
	if (!paired) {
		memset(base, 0, 0x0C);
		put_u32le(base + 0x00, 1);
		put_u32le(base + 0x04, 1);
		base[0x08] = 7;
		base[0x0A] = 7;
	}
	wbx_trainer_identity(&trainer, base, ident);
	const u8 ident_cmd = paired ? PW_CMD_IDENTITY_SEND : PW_CMD_IDENTITY_PAIR;
	// The trainer-name picture (0x1250, 80x16) in the walker's 0x1200-0x13FF blocks.
	u8 name_blocks[0x200];
	memcpy(name_blocks, eeprom + 0x1200, sizeof(name_blocks));
	wbx_label_image(trainer.name, 80, 16, name_blocks + (EE_NAME_IMAGE - 0x1200));
	u8 team_out[EE_TEAM_SIZE];
	memcpy(team_out, team, EE_TEAM_SIZE);
	if (!paired)
		memcpy(team_out + 0x08, id.unique_id, 0x28); // as the game does when pairing

	session s;
	int final_sent = 0;
	int r = part2_open(&s, "SEND TO WALKER", "send walk", &id);
	if (r == 0) {
		// Same order as the game: identity 52 (32 to pair), name picture, staged walk data, staged team,
		// ping, 5A (38 to pair).
		if (!paired)
			put_u32be(ident + 0x60, (u32)(time(NULL) - 946684800)); // seconds since 2000-01-01 (epoch unverified)
		unsigned ident_at = s.reconnects;
		r = pw_send_identity(&s.link, ident_cmd, ident);
		dbg_printf("send walk: identity %02x (trainer %s, %05u/%05u) %s", ident_cmd, trainer.name, trainer.tid,
		           trainer.sid, pw_strerror(r));
		trace_quiet = 1;
		s.link.read_retries = 0;
		unsigned name_todo = 0;
		for (unsigned a = 0; a < sizeof(name_blocks); a += CHUNK)
			name_todo += memcmp(name_blocks + a, eeprom + 0x1200 + a, CHUNK) != 0;
		const unsigned total = name_todo + (EE_WALK_SIZE + EE_TEAM_SIZE) / CHUNK;
		unsigned sent = 0;
		for (unsigned a = 0; a < sizeof(name_blocks) && r == 0; a += CHUNK) {
			if (!memcmp(name_blocks + a, eeprom + 0x1200 + a, CHUNK))
				continue;
			r = write_block_ident(&s, 0x1200 + a, name_blocks + a, ident_cmd, ident, &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / total);
			refresh();
		}
		for (unsigned i = 0; i < EE_WALK_SIZE && r == 0; i += CHUNK) {
			r = write_block_ident(&s, EE_STAGE_WALK + i, walk_blob + i, ident_cmd, ident, &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / total);
			refresh();
		}
		for (unsigned i = 0; i < EE_TEAM_SIZE && r == 0; i += CHUNK) {
			r = write_block_ident(&s, EE_STAGE_TEAM + i, team_out + i, ident_cmd, ident, &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / total);
			refresh();
		}
		trace_quiet = 0;
		printf("\n");
		dbg_printf("send walk: upload %s (%u name block(s), %u retries, %u reconnects)", pw_strerror(r), name_todo,
		           s.link.read_retries, s.reconnects);
		if (r == 0)
			r = ready_for_final(&s, ident_cmd, ident, &ident_at);
		if (r == 0) {
			final_sent = 1; // v0.50 (audit S6): from here a failure may still mean the walker started the walk
			r = paired ? pw_walk_start(&s.link) : pw_walk_start_pairing(&s.link);
			dbg_printf("send walk: %s %s", paired ? "5A" : "38", pw_strerror(r));
		}
		// (v0.28 read the identity here; it always shows the state from before the walk start - the walker
		// applies it after the link ends. Test v0.28 proved 52 copies the trainer: backups 022 -> 023.)
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	// v0.50 (audit S6): the start command went out but its answer was lost: the walker may have started the walk.
	// It is recorded and marked as if it had (End walk checks the walker; if the walk is not there, End walk offers
	// to take the Pokemon back without gains - audit B2).
	int maybe = r != 0 && final_sent && r != PW_ERR_CANCELLED;
	if (r == 0 || maybe) {
		// The walk is on the walker: record it, then mark the bank entry (no walker connected now).
		// steps-start = the walker's total step count now (identity +0x64): at walk end, steps = total - this
		// (the game's rule). ev = the EV stat chosen for this walk.
		int lr = -1;
		for (int k = 0; k < 3 && lr != 0; k++) // v0.50 (audit B2 c): the line must reach the card
			lr = ledger("walk-start walker=%08lx route=%08lx watts-banked=%u bank=%d/%d pk=%08lx steps-start=%lu "
			            "ev=%d course=\"%s\"%s",
			            walker_key(&id), (unsigned long)fnv1a(walk_blob, 0xBE), watts, it.box + 1, it.slot + 1, fp,
			            (unsigned long)id.total_steps, walk_ev, walk_course, maybe ? " answer-lost" : "");
		if (maybe) {
			dbg_printf("send walk: start answer lost (%s) - recorded as maybe started", pw_strerror(r));
			printf("\nThe walker did not answer the last\nstep. It may have started the walk.\n"
			       "End walk checks it: if the walk is\nnot there, PocketPKMN offers to\ntake \"%s\" back.\n", nick);
		} else {
			printf("\nWALK STARTED: \"%s\" is walking.\n%u watt(s) kept by PocketPKMN.\n", nick, watts);
			if (walk_ev >= 0)
				printf("Training %s: +1 EV every 20 steps.\n", k_ev_names[walk_ev]);
		}
		if (lr != 0)
			printf("Recording the walk on the SD card\nfailed. End walk may have to take\nit back without gains.\n");
		if (storage_walk_mark(it.box, it.slot, 1, fp, why, sizeof(why)) != 0) {
			printf("But marking it in the bank failed:\n%s\n(It stays in the bank either way.)\n", why);
			dbg_printf("send walk: walking mark failed: %s", why);
		}
	} else {
		printf("\nSend failed: %s\n", pw_strerror(r));
		printf("The walker did not start the walk.\n");
	}
	flush_log();
	wait_key();
}

// A found item for the result screen, with the route's own name picture when the route has that item.
static void res_add_item(ui_result_info *r, const wbx_route *rt, unsigned item)
{
	if (r->nitems >= 13)
		return;
	int k = r->nitems++;
	r->item[k] = item;
	r->item_name[k] = NULL;
	for (int q = 0; q < 10; q++)
		if (rt->item[q] == item)
			r->item_name[k] = rt->item_img[q];
}

// v0.50 (audit B2): a PocketPKMN Pokemon is marked walking but the connected walker does not have that walk (lost
// start answer, HeartGold/SoulSilver reset the walker, ...). Without this the mark could never be cleared. Asks
// (L+R + A); yes = the mark is cleared, no gains, ledger "walk-abandoned". Returns 1 if a question was asked
// (and its result shown), 0 if no PocketPKMN Pokemon is marked walking.
static int offer_lost_walk(const char *reason)
{
	int box, slot;
	unsigned long fp;
	if (!storage_walking_slot(&box, &slot, &fp))
		return 0;
	static wbx_walk_poke lp;
	int from_pack = 0;
	char why[96], nick[16] = "Your Pokemon";
	if (storage_walk_prepare(box, slot, &lp, &from_pack, why, sizeof(why)) == 0)
		pw_text_to_ascii(lp.nickname, 11, nick, sizeof(nick));
	printf("\n%s is marked as walking,\nbut %s.\nIts walk was lost.\n", nick, reason);
	char q[96];
	snprintf(q, sizeof(q), "Take %s back\nwithout walk gains?", nick);
	if (!ask(q, 1)) {
		printf("\nNothing was changed.\n");
		wait_key();
		return 1;
	}
	if (storage_walk_mark(box, slot, 0, fp, why, sizeof(why)) != 0) {
		printf("\nThat failed: %s\n", why);
		dbg_printf("lost walk: clearing the mark failed: %s", why);
	} else {
		ledger("walk-abandoned bank=%d/%d pk=%08lx reason=\"%s\"", box + 1, slot + 1, fp, reason);
		dbg_printf("lost walk: bank %d/%d mark cleared (%s)", box + 1, slot + 1, reason);
		printf("\n%s is back. It can go on a\nnew walk or back into its game.\n", nick);
	}
	flush_log();
	wait_key();
	return 1;
}

static void do_walk_end(void)
{
	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	if (part1_backup("WALK END", "walk end (backup)", &id, id_raw) < 0) {
		wait_key();
		return;
	}
	const u8 *route = eeprom + EE_ROUTE;
	if (!(id.flags & PW_IDFLAG_HAS_POKE)) {
		printf("\nNo Pokemon is on a walk.\n");
		if (!offer_lost_walk("the walker has no Pokemon")) // v0.50 (audit B2)
			wait_key();
		return;
	}
	int bank_box = -1, bank_slot = -1, bank_ev = -1;
	unsigned long bank_fp = 0;
	long steps_start = -1;
	int from_bank = bank_walk_of_ex(&id, route, &bank_box, &bank_slot, &bank_fp, &steps_start, &bank_ev);
	// v0.50 (audit B2): a PocketPKMN Pokemon marked walking, but this walker carries another walk (HeartGold /
	// SoulSilver reset it, or the start answer was lost and the walk never began): offer the way out first
	if (!from_bank && offer_lost_walk("the walker has a different walk"))
		return;
	if (!confirm_takeover(&id, route, "end this walk here"))
		return;
	print_stroll(route);
	// W4: steps of this walk = the walker's total now - its total at the walk start (the game's rule)
	unsigned walk_steps = 0;
	if (from_bank && steps_start >= 0 && id.total_steps >= (unsigned long)steps_start)
		walk_steps = (unsigned)(id.total_steps - (unsigned long)steps_start);
	if (from_bank) {
		printf("PocketPKMN Pokemon from box %d slot %d.\n", bank_box + 1, bank_slot + 1);
		if (steps_start >= 0)
			printf("Steps on this walk: %u\n", walk_steps);
		else
			printf("Sent before v0.30: steps unknown,\nso no EXP/EV gains this time.\n");
	}
	{ // v0.49: what the question is about, as one short paragraph (the lines above stay for the debug view)
		pw_poke_summary p;
		pw_poke_summary_parse(route, &p);
		char nick[16], rname[24];
		route_text(route, 0x10, 11, nick, sizeof(nick));
		route_text(route, 0x28, 21, rname, sizeof(rname));
		printf("\n%s (Lv %u) is walking\non %s.\n", nick, p.level, rname);
		if (from_bank && steps_start >= 0)
			printf("%u steps so far.\n", walk_steps);
	}
	if (!ask("End the walk now?", 0))
		return;
	// v0.41: the result screen's data (filled next to the printed lines below; those stay for the debug view).
	// The walk's own data (route Pokemon / item name pictures) comes from the part 1 backup.
	static ui_result_info res;
	static wbx_walk_poke res_poke;
	static wbx_route res_route;
	memset(&res, 0, sizeof(res));
	wbx_walk_extract(eeprom + EE_ROUTE, &res_poke, &res_route);
	memcpy(res.sprite, res_poke.sprite, sizeof(res.sprite));
	res.steps = walk_steps;
	res.ev_stat = -1;

	session s;
	int r = part2_open(&s, "WALK END", "walk end", &id);
	if (r == 0) {
		// The game's order: 20 (done), 40 + identity, reads, ping, 4E.
		unsigned ident_at = s.reconnects;
		r = pw_send_identity(&s.link, PW_CMD_IDENTITY_RETURN, id_raw);
		dbg_printf("walk end: identity 40 %s", pw_strerror(r));
		u8 results[0x80];
		if (r == 0)
			r = read_chunk(&s, 0xCE80, results); // fresh: watts and finds may have changed since part 1
		if (r == 0)
			r = ready_for_final(&s, PW_CMD_IDENTITY_RETURN, id_raw, &ident_at);
		if (r == 0) {
			r = pw_walk_end(&s.link);
			dbg_printf("walk end: 4E %s", pw_strerror(r));
		}
		if (r == 0) {
			unsigned watts = (unsigned)((results[EE_WATTS - 0xCE80] << 8) | results[EE_WATTS - 0xCE80 + 1]);
			char nick[16];
			route_text(route, 0x10, 11, nick, sizeof(nick));
			pw_poke_summary p;
			pw_poke_summary_parse(route, &p);
			ledger("walk-end walker=%08lx pokemon=#%u \"%s\" watts=%u steps-total=%lu", walker_key(&id), p.species,
			       nick, watts, (unsigned long)s.id.total_steps);
			printf("\nWALK ENDED. \"%s\" is back.\nWatts: %u\n", nick, watts);
			res.ok = 1;
			snprintf(res.nick, sizeof(res.nick), "%s", nick);
			res.watts = watts;
			for (int i = 0; i < 3; i++) {
				pw_poke_summary c;
				pw_poke_summary_parse(results + (EE_CAUGHT - 0xCE80) + 16 * i, &c);
				if (c.species) {
					printf("Caught: #%u Lv %u\n", c.species, c.level);
					if (res.ncaught < 3) {
						int k = res.ncaught++;
						res.caught_species[k] = c.species;
						res.caught_level[k] = c.level;
						for (int q = 0; q < 3; q++) // the route's own picture + name of that Pokemon
							if ((unsigned)(res_route.poke[q].summary[0] | (res_route.poke[q].summary[1] << 8)) == c.species) {
								res.caught_small[k] = res_route.poke[q].small;
								res.caught_name[k] = res_route.poke[q].name_img;
							}
					}
					ledger("  caught #%u lv %u item %u moves %u %u %u %u variant %02x flags %02x", c.species,
					       c.level, c.held_item, c.moves[0], c.moves[1], c.moves[2], c.moves[3], c.variant,
					       c.flags);
				}
			}
			for (int i = 0; i < 3; i++) {
				const u8 *it = results + (EE_DOWSED - 0xCE80) + 4 * i;
				unsigned item = (unsigned)(it[0] | (it[1] << 8));
				if (item) {
					printf("Found item #%u\n", item);
					res_add_item(&res, &res_route, item);
					ledger("  dowsed item %u", item);
				}
			}
			for (int i = 0; i < 10 && (EE_GIFTED - 0xCE80) + 4 * i + 1 < 0x80; i++) {
				const u8 *it = results + (EE_GIFTED - 0xCE80) + 4 * i;
				unsigned item = (unsigned)(it[0] | (it[1] << 8));
				if (item) {
					printf("Gift item #%u\n", item);
					res_add_item(&res, &res_route, item);
					ledger("  gifted item %u", item);
				}
			}
			// Event Pokemon / item (special route or gift), from the part 1 backup.
			if (eeprom[EE_SPECIAL_FLAGS] & 0x20) {
				pw_poke_summary e;
				pw_poke_summary_parse(eeprom + EE_EVENT_POKE, &e);
				printf("Event Pokemon: #%u Lv %u\n", e.species, e.level);
				ledger("  event pokemon #%u lv %u", e.species, e.level);
			}
			if (eeprom[EE_SPECIAL_FLAGS] & 0x40) {
				unsigned item = (unsigned)(eeprom[EE_EVENT_ITEM] | (eeprom[EE_EVENT_ITEM + 1] << 8));
				printf("Event item #%u\n", item);
				ledger("  event item %u", item);
			}
			printf("(Recorded in /3ds/pocketpkmn/ledger.txt)\n");
		} else {
			printf("\nWalk end failed: %s\n", pw_strerror(r));
		}
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	// v0.50: which item numbering the course's items use (GSC courses: Gen 2 ids) - asked while the Pokemon is
	// still in its slot (no walker connected now)
	int items_gen2 = from_bank && storage_walk_ev_cap(bank_box, bank_slot) == 0;
	if (r == 0 && from_bank) {
		// W4: back in the bank with what the walk gave it (no walker connected now). The walking mark is
		// cleared in the same bank write.
		char why[96];
		storage_walk_result g;
		unsigned long new_fp = 0;
		if (storage_walk_return(bank_box, bank_slot, bank_fp, walk_steps, bank_ev, &g, &new_fp, why, sizeof(why)) == 0) {
			printf("Back in PocketPKMN box %d slot %d.\n", bank_box + 1, bank_slot + 1);
			res.gains = 1;
			res.level_before = g.level_before;
			res.level_after = g.level_after;
			res.exp_gain = g.exp_after - g.exp_before;
			res.has_friendship = g.friendship;
			res.friend_before = g.friend_before;
			res.friend_after = g.friend_after;
			res.ev_stat = g.ev_stat;
			res.ev_before = g.ev_before;
			res.ev_after = g.ev_after;
			if (g.level_after > g.level_before)
				printf("Lv %u -> %u!\n", g.level_before, g.level_after);
			printf("EXP +%u", g.exp_after - g.exp_before);
			if (g.friendship)
				printf(", friendship %u -> %u", g.friend_before, g.friend_after);
			printf("\n");
			if (g.ev_stat >= 0)
				printf("%s EVs %u -> %u\n", k_ev_names[g.ev_stat], g.ev_before, g.ev_after);
			ledger("  back in bank=%d/%d pk=%08lx steps=%u lv=%u->%u exp=%u->%u friendship=%u->%u ev=%d %u->%u "
			       "pk-now=%08lx", bank_box + 1, bank_slot + 1, bank_fp, g.steps, g.level_before, g.level_after,
			       g.exp_before, g.exp_after, g.friend_before, g.friend_after, g.ev_stat, g.ev_before, g.ev_after,
			       new_fp);
			// One-slot flow: from the walker slot straight back into its game.
			if (bank_box == STORAGE_WALK_BOX && bank_slot == STORAGE_WALK_SLOT) {
				flush_log();
				printf("Putting it back into its game...\n");
				refresh();
				char m[160];
				int back = storage_return_to_game(m, sizeof(m));
				printf("%s\n", m);
				snprintf(res.back, sizeof(res.back), "%s", m);
				ledger("  return to game: %s", back == 1 ? "done" : back == 0 ? "waiting in the walker slot" : "error");
			}
		} else {
			// The gains could not be written: at least end the walking mark, so the Pokemon is free again.
			printf("Gains not applied: %s\n", why);
			snprintf(res.back, sizeof(res.back), "Gains not applied:\n%s", why);
			dbg_printf("walk end: gains not applied: %s", why);
			if (storage_walk_mark(bank_box, bank_slot, 0, bank_fp, why, sizeof(why)) == 0) {
				printf("Back in PocketPKMN box %d slot %d.\n", bank_box + 1, bank_slot + 1);
				ledger("  back in bank=%d/%d pk=%08lx (no gains)", bank_box + 1, bank_slot + 1, bank_fp);
			} else {
				printf("Clearing its walking mark failed:\n%s\n", why);
				dbg_printf("walk end: walking mark not cleared: %s", why);
			}
		}
		flush_log();
	}
	if (res.ok) {
		for (int i = 0; i < res.ncaught && i < 3; i++) // v0.50: names as text (romfs: only now, after the link)
			storage_species_name((int)res.caught_species[i], res.caught_text[i], sizeof(res.caught_text[i]));
		// v0.50: items as text. A course item uses the course's numbering; a gift (not on the course) the walker's
		// own (national) numbering.
		for (int i = 0; i < res.nitems && i < 13; i++)
			storage_item_name(res.item_name[i] ? items_gen2 : 0, (int)res.item[i], res.item_text[i],
			                  sizeof(res.item_text[i]));
		ui_result_run(&res); // v0.41 (the same lines are in the debug view)
	}
	else
		wait_key();
}

// --- reset a walker ------------------------------------------------------------------
//
// The game's reset (docs/PAIRING.md 3): one 2A with a 0x28-byte unique id. WalkerBox sends the walker's
// OWN unique id, so its id stays the same whether or not the walker stores the payload (open point R1).
// Clears trainer + pairing, watts, events, items, caught list and the current walk; keeps the menu
// pictures and lifetime steps. Afterwards "Send to walker" sets it up for the WalkerBox trainer.

static void do_reset(void)
{
	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	if (part1_backup("RESET WALKER", "reset (backup)", &id, id_raw) < 0) {
		wait_key();
		return;
	}
	char owner[12];
	pw_text_to_ascii(id.trainer_name, 8, owner, sizeof(owner));
	unsigned watts = (unsigned)((eeprom[EE_WATTS] << 8) | eeprom[EE_WATTS + 1]);
	int bank_box = -1, bank_slot = -1;
	unsigned long bank_fp = 0;
	int from_bank = (id.flags & PW_IDFLAG_HAS_POKE) && bank_walk_of(&id, eeprom + EE_ROUTE, &bank_box, &bank_slot,
	                                                                 &bank_fp);
	printf("\nTrainer: %s (ID %05u)%s\n", owner[0] ? owner : "-", id.tid,
	       (id.flags & PW_IDFLAG_PAIRED) ? "" : " - not set up");
	if (id.flags & PW_IDFLAG_HAS_POKE)
		print_stroll(eeprom + EE_ROUTE);
	printf("\nA reset ERASES on the walker:\n- its trainer and pairing\n- the walk%s\n- %u watt(s) (PocketPKMN keeps them)\n"
	       "- items, caught Pokemon, events\nKept: menu pictures, total steps.\n",
	       from_bank ? " (the Pokemon stays in the\n  PocketPKMN bank)" : (id.flags & PW_IDFLAG_HAS_POKE) ? " (a game's Pokemon on it\n  is LOST for that game)" : "",
	       watts);
	if (!ask("Reset the walker?", 1))
		return;

	session s;
	int r = part2_open(&s, "RESET WALKER", "reset", &id);
	int reset_ok = 0;
	if (r == 0) {
		r = pw_walker_reset(&s.link, id.unique_id);
		dbg_printf("reset: 2A %s", pw_strerror(r));
		if (r == PW_ERR_TIMEOUT)
			r = 0; // the game does not wait for an answer either; the check below decides
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	// The walker applies the reset after the link ends: an identity read in the same connection still
	// shows the old state (v0.28 test). So check in a new connection.
	if (r == 0) {
		printf("\nReset sent. To check it, choose\nConnect on the walker once more.\n");
		session c;
		int q = session_open(&c, "RESET WALKER\nCheck", "reset check");
		if (q == 0) {
			int same_uid = !memcmp(c.id.unique_id, id.unique_id, 0x28);
			dbg_printf("reset check: walker flags %02x tid %05u, unique id %s", c.id.flags, c.id.tid,
			           same_uid ? "unchanged" : "CHANGED");
			reset_ok = !(c.id.flags & PW_IDFLAG_PAIRED);
			printf(reset_ok ? "\nWALKER RESET.\n" : "\nThe walker still says it is set up.\nReset did not happen.\n");
		} else {
			printf("\nNo check (%s).\nUse Link test: a reset walker\nhas no trainer.\n", pw_strerror(q));
		}
		session_close(&c, q != PW_ERR_CANCELLED);
	}
	if (reset_ok) {
		ledger("reset walker=%08lx owner=\"%s\" watts-banked=%u", walker_key(&id), owner, watts);
		if (from_bank) {
			char why[96];
			if (storage_walk_mark(bank_box, bank_slot, 0, bank_fp, why, sizeof(why)) == 0) {
				printf("The Pokemon in box %d slot %d is\nno longer walking.\n", bank_box + 1, bank_slot + 1);
				ledger("  back in bank=%d/%d pk=%08lx (walk ended by reset, no gains)", bank_box + 1, bank_slot + 1,
				       bank_fp);
			} else {
				printf("Clearing its walking mark failed:\n%s\n", why);
			}
		}
		printf("Use \"Send to walker\" to set it up\nfor %s with a walk.\n", have_trainer ? trainer.name : "PocketPKMN");
	}
	flush_log();
	wait_key();
}

// --- set up (pair) a walker ----------------------------------------------------------
//
// Follows docs/PAIRING.md section 8, the way HeartGold/SoulSilver pairs: 32 with
// an owner identity, menu pictures to 0x0280-0x8ECF, a first walk to 0xD700,
// team data to 0xD480, ping, 38. Never writes below 0x0280.

#define EE_IMAGES        0x0280
#define EE_IMAGES_END    0x8ED0  // the game writes up to 0x8ECF
#define EE_CHECKED_TABLE 0x8CB0  // 16 x {u16 offset from 0x8CF0, u8 length, u8 sum}
#define EE_OUR_ART_END   0x8C70  // our own art covers 0x0280-0x8C6F; the checksummed block is never touched

// True when the walker's checksummed block (sound data) is intact: every one
// of the 16 sums matches. Such a walker was set up by a game before and still
// holds its menu pictures.
static int checked_block_ok(const u8 *ee)
{
	int valid = 0;
	for (int i = 0; i < 16; i++) {
		const u8 *d = ee + EE_CHECKED_TABLE + 4 * i;
		unsigned off = (unsigned)(d[0] | (d[1] << 8)), len = d[2];
		if (0x8CF0 + off + len > EE_IMAGES_END)
			return 0;
		u8 sum = 0;
		for (unsigned k = 0; k < len; k++)
			sum = (u8)(sum + ee[0x8CF0 + off + k]);
		if (sum != d[3])
			return 0;
		valid += len > 0;
	}
	return valid > 0;
}

// The 3DS profile name, reduced to what the walker's font has.
static void owner_name(char *out, size_t out_size)
{
	u16 utf16[0x0E] = {0};
	snprintf(out, out_size, "Trainer");
	cfguInit();
	if (R_SUCCEEDED(CFGU_GetConfigInfoBlk2(sizeof(utf16), 0x000A0000, utf16))) {
		size_t o = 0;
		for (int i = 0; i < 0x0E && utf16[i] && o + 1 < out_size && o < 7; i++) {
			u16 c = utf16[i];
			if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ' ')
				out[o++] = (char)c;
		}
		if (o)
			out[o] = '\0';
	}
	cfguExit();
}

static void put_u32le(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void put_u32be(u8 *p, u32 v) { p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v; }

static u8 compose[PW_EEPROM_SIZE]; // what the walker should hold after set-up

static int walk_valid(const u8 *ee)
{
	pw_poke_summary p;
	pw_poke_summary_parse(ee + EE_ROUTE, &p);
	return p.species != 0 && p.species <= 1025;
}

static void do_setup(void)
{
	// Walker files: a backup made with "Back up walker" (this walker or another
	// of the user's own). Chosen before connecting so the walker is not kept waiting.
	int src = choose_backup_ex("SET UP A WALKER: walker files",
	                           "Pick a backup made with \"Back up\nwalker\". Its menu pictures, sounds\n"
	                           "and last walk are used when the\nwalker being set up has none.\n",
	                           1);
	if (src == -1)
		return;
	int have_src = src >= 0; // the file is now in `image`
	char src_name[32] = "";
	if (have_src)
		snprintf(src_name, sizeof(src_name), "%s", files[src]);

	// v0.41 (user: "include the hgss pokewalker menu sprites in our release app too"): the HeartGold/SoulSilver menu
	// pictures + sounds built into the app (romfs/walker/hgss_menus.bin = EEPROM 0x0280-0x8ECF of the user's
	// HGSS-paired walker, tools/extract_hgss_menus.py), used when neither the walker nor the chosen walker files have
	// them. Read before connecting: no file access during IR.
	static u8 builtin[PW_EEPROM_SIZE];
	int have_builtin = 0;
	{
		FILE *bf = fopen("romfs:/walker/hgss_menus.bin", "rb");
		if (bf) {
			memset(builtin, 0, sizeof(builtin));
			have_builtin = fread(builtin + EE_IMAGES, 1, EE_IMAGES_END - EE_IMAGES, bf) == EE_IMAGES_END - EE_IMAGES &&
			               checked_block_ok(builtin);
			fclose(bf);
		}
		dbg_printf("setup: built-in HGSS menus %s", have_builtin ? "ok" : "missing or damaged");
	}

	pw_identity id;
	u8 id_raw[PW_IDENTITY_SIZE];
	if (part1_backup("SET UP A WALKER", "setup (backup)", &id, id_raw) < 0) {
		wait_key();
		return;
	}
	if (have_src)
		dbg_printf("setup: walker files from %s", src_name);
	if (id.flags & PW_IDFLAG_PAIRED) {
		printf("\nThis walker is already set up.\n\"Send to walker\" makes it\nPocketPKMN's with its next walk\n"
		       "(or use \"Reset walker\" first).\n");
		wait_key();
		return;
	}

	// Menu pictures + sounds: 1 = already on the walker, 2 = from the walker
	// files, 3 = the built-in HGSS set (v0.41), 0 = WalkerBox's own art (sounds then stay as they are).
	int pics = 0;
	if (checked_block_ok(eeprom))
		pics = 1;
	else if (have_src && checked_block_ok(image))
		pics = 2;
	else if (have_builtin)
		pics = 3;
	const u8 *walk_from = NULL;
	if (walk_valid(eeprom))
		walk_from = eeprom;
	else if (have_src && walk_valid(image))
		walk_from = image;
	if (!walk_from) {
		// Pairing always starts a walk.
		printf("\nNo walk to start with: neither\nthis walker nor the walker files\n"
		       "hold one. Make a backup of a\nwalker that has been on a walk,\nor send a Pokemon first.\n");
		dbg_printf("setup: no usable walk");
		wait_key();
		return;
	}
	if (!have_trainer) {
		printf("\nNo PocketPKMN trainer yet. Restart\nPocketPKMN to make one.\n");
		wait_key();
		return;
	}
	char name[16];
	snprintf(name, sizeof(name), "%s", trainer.name);
	printf("\nOwner: %s (PocketPKMN trainer)\n", name);
	printf("Menus and sounds: %s\n", pics == 1 ? "kept (already on it)"
	                             : pics == 2 ? "from the walker files"
	                             : pics == 3 ? "HeartGold/SoulSilver (built in)"
	                                         : "PocketPKMN's own art");
	print_stroll(walk_from + EE_ROUTE);
	printf("(first walk from %s)\n", walk_from == eeprom ? "this walker" : "the walker files");
	if (!ask("Set up this walker?", 1))
		return;

	// Owner identity for 32 (layout from the game, docs/PAIRING.md 1.1).
	u8 ident[PW_IDENTITY_SIZE];
	memset(ident, 0, sizeof(ident));
	put_u32le(ident + 0x00, 1);
	put_u32le(ident + 0x04, 1);
	ident[0x08] = 7; // HeartGold's version number (WalkerBox looks like HG to the walker)
	ident[0x0A] = 7;
	for (int i = 0; i < 0x28; i++)
		ident[0x10 + i] = trainer.save_id[i];
	wbx_trainer_identity(&trainer, ident, ident); // TID, SID, name (name, 0xFFFF, zeros)

	// What the walker should hold afterwards (prepared before connecting).
	memcpy(compose, eeprom, PW_EEPROM_SIZE);
	unsigned end;
	if (pics == 2 || pics == 3) {
		// 0x0280-0x8ECF exactly as in the walker files / the built-in set (incl. the checksummed
		// sound block); 0x8ED0-0x8EFF stays as the walker has it, like the game.
		memcpy(compose + EE_IMAGES, (pics == 2 ? image : builtin) + EE_IMAGES, EE_IMAGES_END - EE_IMAGES);
		end = 0x8F00;
	} else if (pics == 1) {
		end = 0x8F00; // only the name picture changes
	} else {
		pw_menu_art_fill(compose); // 0x0280-0x8C6F only
		end = EE_OUR_ART_END;
	}
	wbx_label_image(name, 80, 16, compose + EE_NAME_IMAGE); // same look as "Send to walker"
	u8 team[EE_TEAM_SIZE];
	memcpy(team, walk_from + EE_TEAM, EE_TEAM_SIZE);
	memcpy(team + 0x08, id.unique_id, 0x28); // the walker's own unique id, as the game does

	session s;
	int r = part2_open(&s, "SET UP A WALKER", "setup", &id);
	if (r == 0) {
		put_u32be(ident + 0x60, (u32)(time(NULL) - 946684800)); // seconds since 2000-01-01 (epoch unverified)
		unsigned ident_at = s.reconnects;
		r = pw_send_identity(&s.link, PW_CMD_IDENTITY_PAIR, ident);
		dbg_printf("setup: identity 32 %s", pw_strerror(r));

		trace_quiet = 1;
		s.link.read_retries = 0;
		unsigned todo = 0, sent = 0;
		for (unsigned a = EE_IMAGES; a < end; a += CHUNK)
			todo += memcmp(compose + a, eeprom + a, CHUNK) != 0;
		todo += (EE_WALK_SIZE + EE_TEAM_SIZE) / CHUNK;
		for (unsigned a = EE_IMAGES; a < end && r == 0; a += CHUNK) {
			// Only blocks that change need writing; the rest is already there.
			if (!memcmp(compose + a, eeprom + a, CHUNK))
				continue;
			r = write_block_ident(&s, a, compose + a, PW_CMD_IDENTITY_PAIR, ident, &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / todo);
			refresh();
		}
		// First walk through the staging area.
		for (unsigned i = 0; i < EE_WALK_SIZE && r == 0; i += CHUNK) {
			r = write_block_ident(&s, EE_STAGE_WALK + i, walk_from + EE_ROUTE + i, PW_CMD_IDENTITY_PAIR, ident,
			                      &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / todo);
			refresh();
		}
		for (unsigned i = 0; i < EE_TEAM_SIZE && r == 0; i += CHUNK) {
			r = write_block_ident(&s, EE_STAGE_TEAM + i, team + i, PW_CMD_IDENTITY_PAIR, ident, &ident_at);
			printf("\rUploading %3u%%", ++sent * 100 / todo);
			refresh();
		}
		trace_quiet = 0;
		printf("\n");
		dbg_printf("setup: uploads %s (%u blocks, %u retries, %u reconnects)", pw_strerror(r), sent,
		           s.link.read_retries, s.reconnects);
		if (r == 0)
			r = ready_for_final(&s, PW_CMD_IDENTITY_PAIR, ident, &ident_at);
		if (r == 0) {
			r = pw_walk_start_pairing(&s.link);
			dbg_printf("setup: 38 %s", pw_strerror(r));
		}
		if (r == 0) {
			ledger("setup walker as \"%s\" menus=%s walk=%s", name,
			       pics == 1 ? "kept" : pics == 2 ? src_name : pics == 3 ? "hgss-builtin" : "walkerbox",
			       walk_from == eeprom ? "own" : src_name);
			printf("\nWALKER SET UP as \"%s\".\nCheck it with Link test.\n", name);
		} else {
			printf("\nSet-up failed: %s\nThe walker is not paired yet; the\nsafety backup has its old state.\n",
			       pw_strerror(r));
		}
	}
	session_close(&s, r != PW_ERR_CANCELLED);
	wait_key();
}

// --- bus watch (test A, docs/LINK_REVIEW.md 5 and 7.2) --------------------------------
//
// Is there other traffic on the IR chip's I2C bus, and does ir:rst switch it on
// and off? Three passes of 3 s; during a pass the app does no I2C of its own and
// prints nothing, it only reads the bus control register. Needs no walker.

#define WATCH_CAP 4000 // v0.12: test 11 saw ~2,700 periods in 3 s
static uint32_t w_starts[WATCH_CAP], w_lens[WATCH_CAP], w_tmp[WATCH_CAP];

static int cmp_u32(const void *a, const void *b)
{
	uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
	return x < y ? -1 : x > y;
}

static int watch_pass(const char *label)
{
	uint32_t turns = 0, dropped = 0;
	int n = ir_raw_bus_watch(3000000, w_starts, w_lens, WATCH_CAP, &turns, &dropped);
	if (n < 0) {
		dbg_printf("bus watch %s: I/O registers are not mapped (needs Luma3DS)", label);
		return -1;
	}
	unsigned long lmin = 0, lmed = 0, lmax = 0, gmin = 0, gmed = 0, gmax = 0;
	if (n > 0) {
		memcpy(w_tmp, w_lens, (size_t)n * sizeof(uint32_t));
		qsort(w_tmp, (size_t)n, sizeof(uint32_t), cmp_u32);
		lmin = w_tmp[0];
		lmed = w_tmp[n / 2];
		lmax = w_tmp[n - 1];
	}
	if (n > 1) {
		for (int i = 0; i < n - 1; i++)
			w_tmp[i] = w_starts[i + 1] - w_starts[i];
		qsort(w_tmp, (size_t)(n - 1), sizeof(uint32_t), cmp_u32);
		gmin = w_tmp[0];
		gmed = w_tmp[(n - 1) / 2];
		gmax = w_tmp[n - 2];
	}
	unsigned long turn_ns = turns ? (unsigned long)(3000000000ULL / turns) : 0;
	dbg_printf("bus watch %s: %d busy period(s)%s; length min/median/max %lu/%lu/%lu us; "
	           "between starts min/median/max %lu/%lu/%lu us; loop turn %lu ns (%lu turns in 3 s)",
	           label, n, dropped ? " (more not stored)" : "", lmin, lmed, lmax, gmin, gmed, gmax, turn_ns,
	           (unsigned long)turns);
	if (dropped)
		dbg_printf("bus watch %s: %lu further period(s) past the %d-entry limit", label, (unsigned long)dropped,
		           WATCH_CAP);
	char line[20 * 9 + 8];
	size_t o = 0;
	for (int i = 0; i < n && i < 20; i++)
		o += (size_t)snprintf(line + o, sizeof(line) - o, " %lu", (unsigned long)w_starts[i]);
	line[o] = '\0';
	dbg_printf("bus watch %s: first starts (us):%s", label, n ? line : " none");

	// v0.12 (9.3): group the periods into bursts (a new burst starts after >= 2 ms
	// of quiet) and report the jitter of the burst-start interval - this decides
	// whether the guard's 150 us margin is enough.
	int nb = 0;
	for (int i = 0; i < n; i++) {
		if (i == 0 || w_starts[i] - (w_starts[i - 1] + w_lens[i - 1]) >= 2000) {
			w_tmp[nb++] = w_starts[i];
		}
	}
	if (nb > 1) {
		for (int i = 0; i < nb - 1; i++)
			w_tmp[i] = w_tmp[i + 1] - w_tmp[i];
		qsort(w_tmp, (size_t)(nb - 1), sizeof(uint32_t), cmp_u32);
		dbg_printf("bus watch %s: %d burst(s); burst-start interval min/median/max %lu/%lu/%lu us", label, nb,
		           (unsigned long)w_tmp[0], (unsigned long)w_tmp[(nb - 1) / 2], (unsigned long)w_tmp[nb - 2]);
	} else {
		dbg_printf("bus watch %s: %d burst(s), no interval to measure", label, nb);
	}
	return n + (int)dropped;
}

// v0.12 (9.3): 200 ms sniff - the controller's data and control registers at
// every busy -> idle edge, logged one line per burst.
#define SNIFF_CAP 200
static uint32_t s_times[SNIFF_CAP];
static uint8_t s_data[SNIFF_CAP], s_cnt[SNIFF_CAP];

static int sniff_pass(void)
{
	int n = ir_raw_bus_sniff(200000, s_times, s_data, s_cnt, SNIFF_CAP);
	if (n < 0) {
		dbg_printf("sniff: I/O registers are not mapped (needs Luma3DS)");
		return -1;
	}
	dbg_printf("sniff: %d edge(s) in 200 ms (data byte / control byte after it, per burst)", n);
	char line[512];
	size_t o = 0;
	uint32_t burst_t = 0;
	for (int i = 0; i < n; i++) {
		if (i == 0 || s_times[i] - s_times[i - 1] >= 2000) {
			if (o)
				dbg_printf("%s", line);
			burst_t = s_times[i];
			o = (size_t)snprintf(line, sizeof(line), "sniff: burst at %lu us:", (unsigned long)burst_t);
		}
		if (o + 8 < sizeof(line))
			o += (size_t)snprintf(line + o, sizeof(line) - o, " %02x/%02x", s_data[i], s_cnt[i]);
	}
	if (o)
		dbg_printf("%s", line);
	return n;
}

static void do_bus_watch(void)
{
	consoleSelect(&top);
	consoleClear();
	printf("BUS WATCH\n\nCounts other I2C traffic.\nNeeds no walker. Changes nothing.\n\n");
	printf("One 3 s pass, then a 0.2 s sniff.\nPlease wait about 4 s.\n\n");
	refresh();
	log_header("bus watch");
	dbg_printf("bus watch: only reads the I2C controller registers; a busy period shorter than one loop turn "
	           "can be missed (loop turn time is in the line)");

	// v0.12 (9.3): ir:rst is no longer varied (test 11: it changes nothing).
	int n1 = watch_pass("(ir:rst off)");
	printf("Transfers seen in 3 s: %d\n", n1);
	refresh();
	int n2 = sniff_pass();
	printf("Sniffed bus bytes:     %d\n", n2);
	printf("\n(details in link_log.txt)\n");
	flush_log();
	wait_key();
}

// --- sprite viewer (pack on the SD card, source/wbx_sprites.c) -------------------------

// Draws a walker image on the top screen (RGB565 console framebuffer, column-major, y flipped),
// each walker pixel as scale x scale screen pixels.
static void draw_walker_image(const u8 *img, int w, int h, int sx, int sy, int scale)
{
	// 255 / 168 / 80 / 0 (v0.32: black is UI_LEGACY_BLACK so the text view keeps it apart from its background)
	static const u16 grey[4] = { UI_LEGACY_WHITE, 0xAD55, 0x52AA, UI_LEGACY_BLACK };
	u8 shades[64 * 48];
	pw_img_decode(img, w, h, shades);
	u16 *fb = ui_legacy_fb(GFX_TOP); // v0.32: the top console's RAM buffer
	for (int y = 0; y < h * scale; y++) {
		int py = sy + y;
		if (py < 0 || py >= 240)
			continue;
		for (int x = 0; x < w * scale; x++) {
			int px = sx + x;
			if (px < 0 || px >= 400)
				continue;
			fb[px * 240 + (239 - py)] = grey[shades[(y / scale) * w + x / scale] & 3];
		}
	}
}

static void do_sprites_view(void);
static void do_sprites(void)
{
	ui_legacy_raw(1); // v0.44: pictures drawn into the old text view
	do_sprites_view();
	ui_legacy_raw(0);
}

static void do_sprites_view(void)
{
	static u8 spr[WBX_SPRITE_SIZE];
	int dex = 1, frame = 0, found = 0, loaded = -1;
	u64 last_flip = osGetTime();
	while (aptMainLoop()) {
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_B)
			break;
		int step = (down & KEY_RIGHT) ? 1 : (down & KEY_LEFT) ? -1 : (down & KEY_UP) ? 10 : (down & KEY_DOWN) ? -10
		         : (down & KEY_R) ? 100 : (down & KEY_L) ? -100 : 0;
		if (step)
			dex = ((dex - 1 + step) % WBX_SPRITE_LAST + WBX_SPRITE_LAST) % WBX_SPRITE_LAST + 1;
		int redraw = 0;
		if (dex != loaded) {
			found = wbx_sprite_load(dex, spr);
			loaded = dex;
			redraw = 1;
			consoleSelect(&bottom);
			consoleClear();
			printf("Sprites\n\n#%03d  %s\n\n", dex, found ? "from the sprite pack" : "PLACEHOLDER (no file)");
			printf("Pack: inside the app (romfs),\nSD /3ds/pocketpkmn/sprites/ as fallback\n\n");
			printf("Left/Right: -1/+1\nDown/Up:    -10/+10\nL/R:        -100/+100\nB: back\n");
			consoleSelect(&top);
			consoleClear();
		}
		if (osGetTime() - last_flip >= 500) { // the walker alternates the frames
			frame ^= 1;
			last_flip = osGetTime();
			redraw = 1;
		}
		if (redraw) {
			draw_walker_image(spr + WBX_SPRITE_SMALL_SIZE + frame * 0x300, 64, 48, 24, 48, 3);
			draw_walker_image(spr + frame * 0xC0, 32, 24, 248, 84, 3);
			refresh();
		}
		gspWaitForVBlank();
	}
	consoleSelect(&top);
	consoleClear();
}

// --- storage (source/storage.cpp, PKSM-Core) ------------------------------------------

static int games_dirty; // (defined with its value further down)

static void do_storage(void)
{
	ui_legacy_raw(1); // v0.44: this browser is laid out in console columns
	storage_browse(&top, &bottom);
	ui_legacy_raw(0);
	games_dirty = 1; // v0.50: moves change what the Home games row shows
}

// --- menu ------------------------------------------------------------------------------

typedef struct item {
	const char *label;
	const char *help;
	void (*run)(void);
} item;

static const item menu[] = {
	{ "Link test", "Reads trainer, steps, watts and\nthe walk. Changes nothing.", do_link_test },
	{ "Back up walker", "Saves the walker's whole memory\nto /3ds/pocketpkmn/eeprom_NNN.bin:\n"
	                    "your walker files (menus, sounds,\nwalk) for setting up walkers.\nChanges nothing.", do_backup },
	{ "Write test", "Writes to a scratch area the\nwalker only uses when a walk\nstarts, checks it, then puts the\noriginal bytes back.", do_write_test },
	{ "Restore a backup", "Writes a backup file back to the\nwalker. Makes a safety backup\nfirst.", do_restore },
	{ "Walk start test", "Starts the walker's last walk\nagain the way the game does.\nMakes a safety backup first.", do_walk_start },
	{ "Send to walker", "Choose a game and a Pokemon in\nits boxes; it walks the route\n\"Test Path\" and goes back into\n"
	                    "its game at End walk (with what\nit gained). Backs up first.", do_send_walk },
	{ "End walk", "Brings the walking Pokemon back\nand records what it found.\nMakes a safety backup first.", do_walk_end },
	{ "Reset walker", "Erases the walker's trainer,\nwalk, watts and items (keeps its\npictures and total steps).\n"
	                  "Makes a safety backup first.", do_reset },
	{ "Set up a walker", "Pairs a reset or blank walker\nwith PocketPKMN, no game needed.\n"
	                     "Uses walker files from a backup\nif it has no menus or walk.\nMakes a safety backup first.", do_setup },
	{ "Bus watch", "Counts other I2C traffic (~4 s).\nNeeds no walker. Changes nothing.", do_bus_watch },
	{ "Move Pok\xC3\xA9mon", "Moves Pokemon between your games\nand PocketPKMN's own boxes. In its\n"
	             "game list X restores a game's\nnewest backup. Backs up the save\nbefore the first move.", do_storage },
	{ "Sprites", "Shows the walker sprites built\ninto the app (both sizes, both\nframes). Needs no "
	             "walker.\nChanges nothing.", do_sprites },
};
#define MENU_COUNT ((int)(sizeof(menu) / sizeof(menu[0])))

static u32 trainer_rand(void)
{
	return ((u32)rand() << 16) ^ (u32)rand() ^ (u32)svcGetSystemTick();
}

// First launch: ask for the WalkerBox trainer's name (3DS profile name offered), random TID/SID.
static void ensure_trainer(void)
{
	if (wbx_trainer_load(&trainer, WBX_TRAINER_PATH) == 0) {
		have_trainer = 1;
		return;
	}
	ensure_dir();
	char suggest[16];
	owner_name(suggest, sizeof(suggest));
	for (int tries = 0; tries < 3 && !have_trainer; tries++) {
		consoleSelect(&top);
		consoleClear();
		printf("WELCOME TO POCKETPKMN\n\nChoose your trainer name.\n\nEvery walker PocketPKMN uses will\n"
		       "belong to this trainer (name and\nID shown on the walker). Up to 7\nletters or digits.\n");
		refresh();
		SwkbdState kb;
		char buf[32] = "";
		swkbdInit(&kb, SWKBD_TYPE_WESTERN, 2, WBX_TRAINER_NAME_MAX);
		swkbdSetHintText(&kb, "Trainer name (up to 7)");
		swkbdSetInitialText(&kb, suggest);
		swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
		if (swkbdInputText(&kb, buf, sizeof(buf)) != SWKBD_BUTTON_CONFIRM)
			continue;
		char clean[WBX_TRAINER_NAME_MAX + 1];
		wbx_trainer_clean_name(buf, clean);
		if (!clean[0])
			continue;
		wbx_trainer_new(&trainer, clean, trainer_rand);
		if (wbx_trainer_save(&trainer, WBX_TRAINER_PATH) == 0) {
			have_trainer = 1;
			dbg_printf("trainer made: \"%s\" %05u/%05u", trainer.name, trainer.tid, trainer.sid);
			flush_log();
		} else {
			printf("\nCould not save %s.\n", WBX_TRAINER_PATH + 5);
			wait_key();
		}
	}
}

// --- Home (v0.32, GPU screens; the old text menu is now the Walker tools list) ----------------------------------

static ui_home_data home;
static int home_ready;

// Before a flow runs: empty text view (the flows print into it as before).
static void clear_consoles(void)
{
	consoleSelect(&bottom);
	consoleClear();
	consoleSelect(&top);
	consoleClear();
}

// The ledger's last walk start from the walker slot: date (MM/DD/YY), watts banked, EV stat. 1 if found.
static char last_course[32];

static int last_slot_walk(char *since, size_t since_size, unsigned *watts, int *ev)
{
	char want[24], line[256];
	snprintf(want, sizeof(want), " bank=%d/%d ", STORAGE_WALK_BOX + 1, STORAGE_WALK_SLOT + 1);
	FILE *f = fopen(LEDGER_PATH, "r");
	if (!f)
		return 0;
	int found = 0;
	while (fgets(line, sizeof(line), f)) {
		const char *w = strstr(line, "walk-start ");
		if (!w || !strstr(line, want))
			continue;
		int yy, mm, dd, k;
		unsigned wt;
		const char *a = strstr(line, " watts-banked="), *e = strstr(line, " ev=");
		if (sscanf(line, "%d-%d-%d", &yy, &mm, &dd) == 3)
			snprintf(since, since_size, "%02d/%02d/%02d", mm, dd, yy % 100);
		else
			snprintf(since, since_size, "-");
		*watts = a && sscanf(a, " watts-banked=%u", &wt) == 1 ? wt : 0;
		*ev = e && sscanf(e, " ev=%d", &k) == 1 ? k : -1;
		const char *cs = strstr(line, " course=\"");
		snprintf(last_course, sizeof(last_course), "Test Path"); // walks sent before v0.41
		if (cs) {
			cs += 9;
			size_t q = 0;
			while (cs[q] && cs[q] != '"' && q + 1 < sizeof(last_course)) {
				last_course[q] = cs[q];
				q++;
			}
			last_course[q] = 0;
		}
		found = 1;
	}
	fclose(f);
	return found;
}

// v0.43: "Games on this 3DS" - the same scan as Send to walker's game list, done at start and again only after
// something may have changed the saves (never during IR). The scan's saves are let go right away.
static ui_home_games home_games;
static int games_dirty = 1;

static void games_scan(void)
{
	static storage_game_info info[24];
	for (int i = 0; i < home_games.n; i++)
		ui_title_free(&home_games.icon[i]);
	memset(&home_games, 0, sizeof(home_games));
	ui_loading("Looking for your games\xE2\x80\xA6");
	char why[96] = "";
	int n = storage_games_scan(info, 24, why, sizeof(why));
	storage_games_release();
	const char *bank_note = storage_take_bank_note(); // v0.50 (audit S2): the box was recovered at load
	if (bank_note) {
		clear_consoles();
		printf("POCKETPKMN BOX\n\n%s\n", bank_note);
		wait_key();
	}
	home_games.scanned = 1;
	home_games.total = n > 0 ? n : 0;
	home_games.n = home_games.total < UI_HOME_GAMES ? home_games.total : UI_HOME_GAMES;
	for (int i = 0; i < home_games.n; i++) {
		snprintf(home_games.name[i], sizeof(home_games.name[i]), "%s", info[i].name);
		ui_title_load(&home_games.icon[i], info[i].title_id);
	}
	dbg_printf("home: %d game(s) found%s%s", n, n < 0 ? ": " : "", n < 0 ? why : "");
	games_dirty = 0;
}

static void home_free(void)
{
	if (!home_ready)
		return;
	ui_image_free(&home.big[0]);
	ui_image_free(&home.big[1]);
	ui_image_free(&home.small);
	ui_title_free(&home.game);
	home_ready = 0;
}

// Reads what Home shows: the walker slot (bank 1/1), its origin game, the ledger's walk start. No IR.
static void home_build(void)
{
	static const char *const ev_long[6] = { "HP", "Attack", "Defense", "Sp. Atk", "Sp. Def", "Speed" };
	home_free();
	memset(&home, 0, sizeof(home));
	home_ready = 1;
	if (games_dirty)
		games_scan();
	home.games = &home_games;
	if (have_trainer)
		snprintf(home.trainer, sizeof(home.trainer), "Trainer %s \xC2\xB7 ID %05u", trainer.name, trainer.tid);
	else
		snprintf(home.trainer, sizeof(home.trainer), "No trainer yet");
	int count = storage_walk_list(walk_items, MAX_WALK_ITEMS), at = -1;
	for (int i = 0; i < count && at < 0; i++)
		if (walk_items[i].box == STORAGE_WALK_BOX && walk_items[i].slot == STORAGE_WALK_SLOT)
			at = i;
	char why[96] = "", nick[16] = "";
	int from_pack = 0;
	if (at >= 0 && storage_walk_prepare(STORAGE_WALK_BOX, STORAGE_WALK_SLOT, &walk_poke, &from_pack, why,
	                                    sizeof(why)) == 0) {
		home.slot = walk_items[at].walking ? UI_SLOT_WALKING : UI_SLOT_WAITING;
		pw_text_to_ascii(walk_poke.nickname, 11, nick, sizeof(nick));
		const u8 *spr = walk_poke.sprite;
		ui_image_from_walker(&home.big[0], spr + WBX_SMALL_SIZE, 64, 48, 0);
		ui_image_from_walker(&home.big[1], spr + WBX_SMALL_SIZE + 0x300, 64, 48, 0);
		ui_image_from_walker(&home.small, spr, 32, 24, 1);
		home.animate = memcmp(spr + WBX_SMALL_SIZE, spr + WBX_SMALL_SIZE + 0x300, 0x300) != 0;
		// the colour box icon on the main button (source/ui_icons.c; docs/SESSION_ICONS.md)
		home.species = walk_poke.summary[0] | walk_poke.summary[1] << 8;
		home.form = (walk_poke.summary[14] & 0x01) ? (walk_poke.summary[13] & 0x1F) : 0; // variant: form | 0x20 female
		home.female = (walk_poke.summary[13] & 0x20) != 0;
		home.gen = walk_items[at].gen;
	} else {
		home.slot = UI_SLOT_EMPTY;
	}
	char ot[24] = "", game[64] = "its game";
	unsigned long long tid = 0;
	if (home.slot != UI_SLOT_EMPTY && storage_walk_origin(STORAGE_WALK_BOX, STORAGE_WALK_SLOT, ot, sizeof(ot), NULL,
	                                                      NULL, &tid)) {
		if (ui_title_load(&home.game, tid))
			snprintf(game, sizeof(game), "%s", home.game.name);
		snprintf(home.goes_back, sizeof(home.goes_back), "Goes back to %s (%s) at walk end", game, ot);
	}
	switch (home.slot) {
	case UI_SLOT_WALKING: {
		unsigned watts = 0;
		int ev = -1;
		snprintf(home.headline, sizeof(home.headline), "%s is on a walk", nick);
		snprintf(home.route, sizeof(home.route), "Test Path");
		if (last_slot_walk(home.since, sizeof(home.since), &watts, &ev)) {
			snprintf(home.route, sizeof(home.route), "%s", last_course);
			snprintf(home.watts, sizeof(home.watts), "%u", watts);
			if (ev >= 0 && ev < 6)
				snprintf(home.training, sizeof(home.training), "%s (+1 per 20 steps)", ev_long[ev]);
			else
				snprintf(home.training, sizeof(home.training), "Off");
		} else {
			snprintf(home.since, sizeof(home.since), "-");
			snprintf(home.watts, sizeof(home.watts), "-");
			snprintf(home.training, sizeof(home.training), "-");
		}
		snprintf(home.main_title, sizeof(home.main_title), "End walk");
		snprintf(home.main_sub, sizeof(home.main_sub), "Bring %s back to %s", nick, game);
		break;
	}
	case UI_SLOT_WAITING:
		snprintf(home.headline, sizeof(home.headline), "%s waits in the walker slot", nick);
		snprintf(home.line1, sizeof(home.line1), "Back from a walk; its game was not");
		snprintf(home.line2, sizeof(home.line2), "found then. Send it again or put it back.");
		snprintf(home.main_title, sizeof(home.main_title), "Send to walker");
		snprintf(home.main_sub, sizeof(home.main_sub), "%s, or put it back in its game", nick);
		break;
	default:
		snprintf(home.headline, sizeof(home.headline), count < 0 ? "The walker slot cannot be read"
		                                                          : "No Pok\xC3\xA9mon is on a walk");
		snprintf(home.line1, sizeof(home.line1), count < 0 ? "See the debug view (Settings)." : "Choose a game and a");
		snprintf(home.line2, sizeof(home.line2), count < 0 ? "" : "Pok\xC3\xA9mon to send on a walk.");
		snprintf(home.main_title, sizeof(home.main_title), "Send to walker");
		snprintf(home.main_sub, sizeof(home.main_sub), "Choose a game and a Pok\xC3\xA9mon");
		break;
	}
	flush_log();
}

static void run_flow(void (*fn)(void))
{
	clear_consoles();
	fn();
}

// v0.50: a list of some of the menu[] entries (by label) plus an optional extra entry, run until B.
// extra_run = NULL: no extra entry.
static void menu_list(const char *title, const char *const *names, int n, int *sel, const char *extra_label,
                      const char *extra_help, void (*extra_run)(void))
{
	const char *labels[MENU_COUNT + 1], *help[MENU_COUNT + 1];
	void (*run[MENU_COUNT + 1])(void);
	int m = 0;
	for (int k = 0; k < n; k++)
		for (int i = 0; i < MENU_COUNT; i++)
			if (!strcmp(menu[i].label, names[k]) && m < MENU_COUNT) {
				labels[m] = menu[i].label;
				help[m] = menu[i].help;
				run[m++] = menu[i].run;
			}
	int extra = -1;
	if (extra_run) {
		extra = m;
		labels[m] = extra_label;
		help[m] = extra_help;
		run[m++] = extra_run;
	}
	for (;;) {
		int k = ui_list_run(title, title, labels, help, m, sel);
		if (k < 0)
			return;
		if (k == extra)
			run[k]();
		else
			run_flow(run[k]);
	}
}

// v0.50 (user: "For the home menu let's do walker, game settings, and settings"): Home "Walker" = what is done
// with the walker itself
static void walker_tools(void)
{
	static int sel;
	static const char *const names[] = { "Link test", "Back up walker", "Restore a backup", "Set up a walker",
	                                     "Reset walker" };
	menu_list("Walker", names, 5, &sel, NULL, NULL, NULL);
}

// v0.50 Settings -> Developer: the test tools, the sprite viewer and the debug view
static void developer_tools(void)
{
	static int sel;
	static const char *const names[] = { "Write test", "Walk start test", "Bus watch", "Sprites" };
	menu_list("Developer", names, 4, &sel, "Debug view",
	          "The text of the last action as the\nold text screens showed it.\nB goes back.", ui_debug_view);
}

static void games_rescan(void)
{
	games_dirty = 1;
	games_scan(); // "Looking for your games..." then the Home games row is new
}

// v0.50 Home "Game settings": the games' saves
static void game_settings(void)
{
	static int sel;
	static const char *const names[] = { "Move Pok\xC3\xA9mon" };
	menu_list("Game settings", names, 1, &sel, "Look for games again",
	          "Scans this 3DS for Pokemon saves\nagain (after you add a game or\nsave one for the first time).",
	          games_rescan);
}

// v0.47: app settings on the SD card (APP_DIR/settings.txt, "key=value" lines). Only the background so far.
#define SETTINGS_PATH APP_DIR "/settings.txt"

static void settings_load(void)
{
	FILE *f = fopen(SETTINGS_PATH, "r");
	if (!f)
		return;
	char line[96];
	int v;
	while (fgets(line, sizeof(line), f))
		if (sscanf(line, "background=%d", &v) == 1)
			ui_set_background(v);
	fclose(f);
}

static void settings_save(void)
{
	ensure_dir();
	FILE *f = fopen(SETTINGS_PATH, "w");
	if (!f)
		return;
	fprintf(f, "# PocketPKMN settings\nbackground=%d\n", ui_background());
	fclose(f);
}

static void settings(void)
{
	static int sel;
	for (;;) {
		char guard[40], who[48];
		snprintf(guard, sizeof(guard), "Burst guard: %s", ir_raw_guard_enabled ? "on" : "off");
		if (have_trainer)
			snprintf(who, sizeof(who), "Trainer: %s (ID %05u)", trainer.name, trainer.tid);
		else
			snprintf(who, sizeof(who), "Trainer: none");
		char bg[48];
		snprintf(bg, sizeof(bg), "Background: %s", ui_background_name(ui_background()));
		// v0.50: Debug view moved into Developer (with the test tools and the sprite viewer)
		const char *labels[5] = { guard, who, bg, "Developer", "Credits" };
		const char *help[5] = {
			"Keeps PocketPKMN's IR transfers clear of\nthe console's own bus traffic (LINK_REVIEW).\n"
			"A switches it. Leave it on.\n(SELECT on Home does the same.)",
			"The PocketPKMN trainer every walker\nbelongs to (made at the first start).",
			"The picture behind every screen.\nA switches: Classic, Pok\xC3\xA9 Ball,\nWalker screen, Route scene.",
			"Test tools, the sprite viewer and\nthe debug view (the text of the\nlast action).",
			"Everyone whose work is in PocketPKMN:\nthe 3D models, sprites, box icons and\ncode. Free software (GNU GPL v3).",
		};
		int k = ui_list_run("Settings", "Settings", labels, help, 5, &sel);
		if (k < 0)
			return;
		if (k == 0)
			ir_raw_guard_enabled = !ir_raw_guard_enabled;
		if (k == 2) { // v0.47 (user: "Add a toggle for all 3 in settings")
			ui_set_background((ui_background() + 1) % UI_BACKGROUNDS);
			settings_save();
		}
		if (k == 3)
			developer_tools();
		if (k == 4)
			ui_credits_run(); // v0.46
	}
}

static void home_loop(void)
{
	int sel = 0;
	home_build();
	while (aptMainLoop()) {
		int k = ui_home_run(&home, &sel);
		if (k == UI_HOME_EXIT)
			break;
		switch (k) {
		case UI_HOME_GUARD:
			ir_raw_guard_enabled = !ir_raw_guard_enabled;
			dbg_printf("burst guard %s (SELECT)", ir_raw_guard_enabled ? "on" : "off");
			continue; // nothing else changed
		case UI_HOME_MAIN:
			run_flow(home.slot == UI_SLOT_WALKING ? do_walk_end : do_send_walk);
			break;
		case UI_HOME_TOOLS:
			walker_tools();
			break;
		case UI_HOME_GAMESET:
			game_settings(); // v0.50
			break;
		case UI_HOME_SETTINGS:
			settings();
			break;
		}
		// a send / walk end may have changed the saves (v0.51: not after the Walker list - walker tools never touch
		// a game save, and the rescan made going back slow; Move Pokemon sets it itself)
		if (k == UI_HOME_MAIN)
			games_dirty = 1;
		home_build(); // a flow may have changed the slot
	}
	home_free();
}

int main(void)
{
	gfxInitDefault();
	ui_init(&top, &bottom); // v0.32: GPU screens; the two consoles now write into RAM (text view)

	// v0.33 rename (docs/RENAME_PLAN.md): the old folder becomes the new one, before anything reads or writes it.
	int moved = app_migrate(APP_OLD_DIR, APP_DIR);
	dbg_printf("start: folder move %s -> %s: %s", APP_OLD_DIR, APP_DIR,
	           moved == APP_MIGRATE_MOVED ? "moved" : moved == APP_MIGRATE_BOTH ? "both exist, old left alone"
	           : moved == APP_MIGRATE_FAILED ? "FAILED" : "nothing to move");
	if (moved == APP_MIGRATE_FAILED) {
		dbg_flush(APP_OLD_DIR "/link_log.txt"); // the log stays with the data; no new folder is made
		printf(APP_NAME "\n\nCould not move your files from\n/3ds/walkerbox to /3ds/pocketpkmn.\n"
		       "Nothing was changed.\n" APP_NAME " stops now (see\n/3ds/walkerbox/link_log.txt).\n");
		wait_key();
		ui_exit();
		gfxExit();
		return 0;
	}
	if (moved == APP_MIGRATE_MOVED) {
		ledger("moved /3ds/walkerbox -> /3ds/pocketpkmn (app renamed WalkerBox -> PocketPKMN)");
		printf("WalkerBox is now " APP_NAME ".\n\nYour files moved from /3ds/walkerbox\nto /3ds/pocketpkmn (trainer, bank,\n"
		       "ledger, backups, sprites).\nNothing else changed.\n");
		wait_key();
	}
	if (moved == APP_MIGRATE_BOTH) {
		printf("Both /3ds/walkerbox and\n/3ds/pocketpkmn are on the SD card.\n\n" APP_NAME " uses /3ds/pocketpkmn\n"
		       "and does not touch the old folder.\n");
		wait_key();
	}

	srand((unsigned)svcGetSystemTick());
	ir_raw_checkpoint = log_checkpoint;
	settings_load(); // v0.47 (after the folder move, before the first screen)
	ensure_trainer();
	home_loop();

	// v0.49 (user: the CIA "stuck on closing software"): each exit step goes into the link log, so the next hang
	// shows the last step reached
	dbg_printf("exit: home closed (close requested by the system: %s)", aptShouldClose() ? "yes" : "no");
	log_checkpoint();
	// v0.49: the Home games scan keeps the GBA save session (a PxiFS0 session taken from the system) open for the
	// whole run; it is now closed before the app ends (suspected cause of the HOME -> Close hang)
	storage_games_release();
	dbg_printf("exit: game saves released");
	log_checkpoint();
	ir_raw_close();
	dbg_printf("exit: IR closed");
	log_checkpoint();
	ui_exit();
	dbg_printf("exit: GPU closed");
	log_checkpoint();
	gfxExit();
	dbg_printf("exit: screens closed, leaving main");
	log_checkpoint();
	return 0;
}
