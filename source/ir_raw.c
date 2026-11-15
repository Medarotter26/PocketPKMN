// IR transport that drives the console's IR chip directly.
//
// The chip is an NXP SC16IS750 UART (IrDA mode) at address 0x9A on the third
// I2C bus (registers at physical 0x10148000). Luma3DS maps the I/O region
// into homebrew at 0x1EC00000, which is what makes this possible from a .3dsx.
// This bypasses the IR system module, so it can collide with that module's
// own bus traffic (notably the New 3DS C-stick). Fallback / comparison only.
//
// Register usage follows the SC16IS750 datasheet; the bus sequences follow
// the 3DS I2C register description on 3dbrew.

#include "ir_link.h"
#include "dbg.h"

#include <3ds.h>
#include <stdlib.h>
#include <string.h>

#define I2C_BASE   (OS_MMIO_VADDR + 0x48000)
#define REG_DATA   (*(volatile u8 *)(I2C_BASE + 0))
#define REG_CNT    (*(volatile u8 *)(I2C_BASE + 1))
#define DEV_IR     0x9A

// SC16IS750 register numbers (sub-address = number << 3)
enum {
	R_RHR = 0x00, R_THR = 0x00, R_IER = 0x01, R_FCR = 0x02, R_LCR = 0x03,
	R_MCR = 0x04, R_LSR = 0x05, R_TXLVL = 0x08, R_RXLVL = 0x09,
	R_SPR = 0x07, R_IODIR = 0x0A, R_IOSTATE = 0x0B, R_EFCR = 0x0F,
	R_DLL = 0x00, R_DLH = 0x01, // while LCR bit 7 is set
	R_EFR = 0x02,               // while LCR = 0xBF
};

#define EFCR_RX_OFF 0x02
#define EFCR_TX_OFF 0x04
#define IDLE_GAP_US 4000

static int raw_open_flag;
static int rx_armed;
static int bus_fault;
static u8 saved_ier, saved_iostate, saved_efcr, saved_mcr, saved_efr;
static unsigned st_ff_bursts, ff_logged, st_reconfigs;
static unsigned st_early_idle, st_unequal_reads, st_short_sends; // v0.11 (LINK_REVIEW 7.4)
static u64 tx_on_us; // when fill_fifo last switched the transmitter on

static bool chip_configure(void);
static bool all_ff(const uint8_t *buf, size_t n);
static void log_chip(const char *when);

// --- I2C bus ---------------------------------------------------------------

static bool bus_wait(void)
{
	for (u32 i = 0; i < 400000; i++)
		if (!(REG_CNT & 0x80))
			return true;
	bus_fault = 1;
	return false;
}

static bool bus_ack(void)
{
	if (!bus_wait())
		return false;
	return (REG_CNT >> 4) & 1;
}

static bool bus_start(u8 dev)
{
	if (!bus_wait())
		return false;
	REG_DATA = dev;
	REG_CNT = 0xC2; // start condition + address byte
	return bus_ack();
}

static bool bus_byte(u8 value, bool last)
{
	if (!bus_wait())
		return false;
	REG_DATA = value;
	REG_CNT = last ? 0xC1 : 0xC0; // stop after the last byte
	return bus_ack();
}

static void bus_abort(void)
{
	REG_CNT = 0xC5; // stop + clear error
	bus_wait();
}

// --- burst guard (v0.12/v0.13, docs/LINK_REVIEW.md 9 and 11) ----------------
//
// Another device on this bus (most likely the gyroscope, read by the HID
// module - LINK_REVIEW 10.1) is read in a burst of ~9 transfers (~0.35 ms in
// all) every ~9.99 ms, and collisions with it are what damaged our transfers
// (test 12: guard on 0 retries, off 40). Rule: no transfer of ours may be in
// progress when a burst is due. Before every direct-bus transfer, if it cannot
// finish GUARD_BEFORE_US before the predicted burst, wait for the burst to pass.

#define BURST_QUIET_US  120  // no busy for this long = burst over
#define GUARD_BEFORE_US 250  // keep this much clear before a predicted burst (v0.13: was 150)
#define BURST_LATE_US   600  // stop waiting this long after the predicted start
#define MAX_XFER_DATA   16   // data bytes per transfer while the guard is on
#define XFER_US(data)   ((u32)(((data) + 3) * 25 + 30))
#define SYNC_FULL       13   // bursts watched at open (the first is dropped)
#define SYNC_SHORT      4    // bursts watched by a re-sync
#define RESYNC_AFTER    3    // misses in a row before a re-sync (v0.13: was 20)
#define MISS_LOG_MAX    10   // misses logged in full per action
#define ERR_RING        64   // recent burst errors kept for the median

static int use_svc;               // defined with the i2c::IR code below
int ir_raw_guard_enabled = 1;     // set by the app (SELECT toggles it)
static int guard_on;              // active for this connection
static int resync_pending;        // RESYNC_AFTER misses in a row: re-sync at the next packet boundary
static unsigned guard_miss_run;
static s64 burst_period, period_open;
static u64 next_burst;
static u64 guard_wait_accum;      // guard waiting inside the current send (for the log)
static int tx_active;             // fill_fifo is topping up a running transmitter
static size_t tx_loaded;          // bytes loaded into the FIFO since the transmitter started
static int in_send;               // raw_send is running (for the miss log)
static int reply_expected;        // a packet went out and its reply has not been received yet
static unsigned st_guard_waits, st_guard_missed, st_miss_early, st_miss_none, st_late, st_fifo_low;
static unsigned st_resyncs, miss_logged;
static s32 err_ring[ERR_RING];
static unsigned err_count;        // errors recorded since the stats were last reset
static s32 st_err_min, st_err_max; // over the whole action
static int st_err_set;

static int cmp_s32(const void *a, const void *b)
{
	s32 x = *(const s32 *)a, y = *(const s32 *)b;
	return x < y ? -1 : x > y;
}

static int cmp_u64(const void *a, const void *b)
{
	u64 x = *(const u64 *)a, y = *(const u64 *)b;
	return x < y ? -1 : x > y;
}

// Watch the bus (no transfers of ours) and lock onto the burst rhythm: a busy
// edge after >= 2 ms of quiet starts a burst, BURST_QUIET_US of quiet ends it.
// want = SYNC_FULL (at open): the first burst may be caught part-way and is
// dropped; period = median of the 11 remaining intervals, accepted if it is
// 5-20 ms and at least 9 of 11 intervals lie within 150 us of it.
// want = SYNC_SHORT (re-sync): median of 3 intervals, at least 2 within 150 us.
// Returns 1 when locked.
static int guard_lock(int want)
{
	u64 t0 = dbg_now_us(), end = t0 + (u64)want * 10000 + 30000, last_end = t0, burst_start = 0;
	u64 starts[SYNC_FULL] = {0}, lens[SYNC_FULL] = {0};
	int pulses[SYNC_FULL] = {0}, nstarts = 0, busy = 0, in_burst = 0;
	for (;;) {
		u64 now = dbg_now_us();
		if (now >= end || (nstarts >= want && !in_burst))
			break;
		int b = (REG_CNT & 0x80) != 0;
		if (b && !busy) {
			busy = 1;
			if (!in_burst && now - last_end >= 2000 && now - t0 >= 2000 && nstarts < want) {
				in_burst = 1;
				burst_start = now;
				starts[nstarts] = now;
			}
			if (in_burst)
				pulses[nstarts]++;
		} else if (!b && busy) {
			busy = 0;
			last_end = now;
		}
		if (in_burst && !busy && now - last_end >= BURST_QUIET_US) {
			in_burst = 0;
			lens[nstarts] = last_end - burst_start;
			nstarts++;
		}
	}
	if (nstarts < want) {
		dbg_printf("raw: burst guard: %s found only %d of %d bursts", want == SYNC_FULL ? "sync" : "re-sync",
		           nstarts, want);
		return 0;
	}
	int first = want == SYNC_FULL ? 1 : 0; // drop the possibly partial first burst at open
	int ni = nstarts - 1 - first;
	u64 iv[SYNC_FULL], sorted[SYNC_FULL];
	for (int i = 0; i < ni; i++)
		iv[i] = sorted[i] = starts[first + i + 1] - starts[first + i];
	qsort(sorted, (size_t)ni, sizeof(u64), cmp_u64);
	u64 med = sorted[ni / 2];
	int close = 0;
	for (int i = 0; i < ni; i++)
		if ((iv[i] > med ? iv[i] - med : med - iv[i]) <= 150)
			close++;
	int need = want == SYNC_FULL ? 9 : 2;
	if (med < 5000 || med > 20000 || close < need) {
		dbg_printf("raw: burst guard: %s rejected: median %lu us, %d of %d intervals within 150 us (min %lu, max "
		           "%lu)", want == SYNC_FULL ? "sync" : "re-sync", (unsigned long)med, close, ni,
		           (unsigned long)sorted[0], (unsigned long)sorted[ni - 1]);
		return 0;
	}
	burst_period = (s64)med;
	next_burst = starts[nstarts - 1] + (u64)burst_period;
	dbg_printf("raw: burst guard: %s period %lu us (%d of %d intervals within 150 us, min %lu, max %lu), "
	           "pulses %d..%d, length %lu..%lu us",
	           want == SYNC_FULL ? "sync" : "re-sync", (unsigned long)med, close, ni, (unsigned long)sorted[0],
	           (unsigned long)sorted[ni - 1], pulses[first], pulses[nstarts - 1], (unsigned long)lens[first],
	           (unsigned long)lens[nstarts - 1]);
	return 1;
}

static void guard_sync(void)
{
	guard_on = 0;
	resync_pending = 0;
	guard_miss_run = 0;
	if (!ir_raw_guard_enabled || use_svc) {
		dbg_printf("raw: burst guard: off (%s)", use_svc ? "service access" : "chosen in the menu");
		return;
	}
	if (!guard_lock(SYNC_FULL)) {
		dbg_printf("raw: burst guard: no periodic traffic found, off");
		return;
	}
	period_open = burst_period;
	guard_on = 1;
}

// v0.13 (11.2d): a short re-sync, only at a packet boundary (top of raw_send,
// or raw_recv while no reply is expected). If it fails, the old prediction is
// kept and the guard stays on.
static void guard_resync_now(void)
{
	resync_pending = 0;
	guard_miss_run = 0;
	if (!guard_on)
		return;
	st_resyncs++;
	s64 old_period = burst_period;
	u64 old_next = next_burst;
	if (!guard_lock(SYNC_SHORT)) {
		burst_period = old_period;
		next_burst = old_next;
		dbg_printf("raw: burst guard: re-sync failed, keeping period %ld us", (long)burst_period);
	}
}

static void note_miss(const char *kind, u64 predicted, u64 t_enter, u32 need_us)
{
	st_guard_missed++;
	if (miss_logged < MISS_LOG_MAX) {
		miss_logged++;
		dbg_printf("raw: guard miss (%s): predicted %lu us, entered %lu us (%+ld), need %lu us, %s", kind,
		           (unsigned long)predicted, (unsigned long)t_enter, (long)((s64)t_enter - (s64)predicted),
		           (unsigned long)need_us, in_send ? "during a send" : "not in a send");
	}
	if (++guard_miss_run >= RESYNC_AFTER)
		resync_pending = 1;
}

// Make room for a transfer (or a whole short send) of need_us: return at once
// if it ends GUARD_BEFORE_US before the next burst, else wait for the burst to
// pass. Never transfers anything itself.
static void guard_reserve(u32 need_us)
{
	if (!guard_on)
		return;
	u64 now = dbg_now_us();
	while (now > next_burst + BURST_LATE_US) // we were away: skip the bursts we missed
		next_burst += (u64)burst_period;
	if (now + need_us + GUARD_BEFORE_US < next_burst)
		return;

	u64 t_enter = now, predicted = next_burst, seen = 0, last_busy = 0, t = now;
	int busy_at_entry = (REG_CNT & 0x80) != 0;
	for (;;) {
		t = dbg_now_us();
		int b = (REG_CNT & 0x80) != 0;
		if (b) {
			if (!seen)
				seen = t;
			last_busy = t;
		} else if (seen && t - last_busy >= BURST_QUIET_US) {
			break; // burst over
		}
		if (!seen && t > predicted + BURST_LATE_US)
			break; // nothing came
		if (seen && t > seen + 5000)
			break; // the bus never went quiet: stop waiting
	}
	if (!seen) {
		// v0.13 (11.2c): "none" = nothing seen by predicted + BURST_LATE_US.
		note_miss("none", predicted, t_enter, need_us);
		next_burst = predicted + (u64)burst_period;
	} else if (busy_at_entry) {
		// v0.13 (11.2c): "early" = the bus was already busy at the first look: the
		// burst started before we got out of the way. Its true start is unknown,
		// so keep the prediction.
		note_miss("early", predicted, t_enter, need_us);
		st_miss_early++;
		next_burst = predicted + (u64)burst_period;
	} else {
		st_guard_waits++;
		guard_miss_run = 0;
		s32 err = (s32)((s64)seen - (s64)predicted);
		if (!st_err_set || err < st_err_min)
			st_err_min = err;
		if (!st_err_set || err > st_err_max)
			st_err_max = err;
		st_err_set = 1;
		err_ring[err_count % ERR_RING] = err;
		err_count++;
		// v0.13 (11.2b): follow the period: period += err / 16, clamped to +-1 % of
		// the period measured at open. The phase restarts from the burst we saw.
		s64 p = burst_period + err / 16;
		s64 lim = period_open / 100;
		if (p > period_open + lim)
			p = period_open + lim;
		if (p < period_open - lim)
			p = period_open - lim;
		burst_period = p;
		next_burst = seen + (u64)burst_period;
	}
	if (!seen)
		st_miss_none++;
	u64 waited = t - t_enter;
	guard_wait_accum += waited;
	if (tx_active) {
		// Would the transmitter have run out of bytes during this hold-off?
		size_t gone = (size_t)((t_enter - tx_on_us) / 86);
		size_t queued = tx_loaded > gone ? tx_loaded - gone : 0;
		if (waited > (u64)queued * 87)
			st_fifo_low++;
	}
}

// The guard figures for the stats: min / max over the action, median of the
// last ERR_RING errors.
static void guard_err_stats(long *mn, long *md, long *mx)
{
	unsigned n = err_count < ERR_RING ? err_count : ERR_RING;
	*mn = *md = *mx = 0;
	if (!n)
		return;
	s32 tmp[ERR_RING];
	memcpy(tmp, err_ring, n * sizeof(s32));
	qsort(tmp, n, sizeof(s32), cmp_s32);
	*mn = st_err_min;
	*md = tmp[n / 2];
	*mx = st_err_max;
}

// Wrap one direct-bus transfer: reserve, run, and count it as "late" if it
// ended after the burst it was meant to stay clear of.
#define GUARDED(need, stmt)                                     \
	do {                                                        \
		guard_reserve(need);                                    \
		u64 guard_nb_ = next_burst;                             \
		stmt;                                                   \
		if (guard_on && dbg_now_us() > guard_nb_)               \
			st_late++;                                          \
	} while (0)

// --- the same register access through the system's "i2c::IR" service --------
//
// On the New 3DS the IR system module reads ZL/ZR/C-stick on the same I2C bus
// (3dbrew, I2C Services: device 17, via i2c::IR). Driving the bus registers
// directly races with those transfers; going through the i2c sysmodule makes
// it take turns. Device 13 = the IR chip. The register byte is the chip's
// sub-address (reg << 3) - confirmed at open time by comparing with direct reads.

#define I2C_DEV_IR 13
static Handle i2c_ir;
static int use_svc;               // 1 = route chip access through i2c::IR
static unsigned st_svc_errors;
int ir_raw_prefer_service = 0;    // set by the app (SELECT toggles it); off by default since test 10
void (*ir_raw_checkpoint)(void);  // app hook: write the log to the SD card now
static int svc_unavailable;       // the open attempt timed out once: never try again this run

// Test 10: with the service preferred, the whole console froze. Most likely
// srvGetServiceHandle blocked forever because i2c::IR allows one session and
// the IR module holds it. So the open runs on a helper thread and is given up
// after 500 ms (the helper may stay blocked; the app carries on).
static volatile int open_done;
static volatile Result open_result;
static void open_thread(void *arg)
{
	(void)arg;
	Handle h = 0;
	Result r = srvGetServiceHandle(&h, "i2c::IR");
	if (R_SUCCEEDED(r))
		i2c_ir = h;
	open_result = r;
	open_done = 1;
}

static Result svc_write(u8 reg, const u8 *buf, u32 size)
{
	u32 *cmd = getThreadCommandBuffer();
	cmd[0] = IPC_MakeHeader(0x000B, 3, 2); // WriteRegisters8
	cmd[1] = I2C_DEV_IR;
	cmd[2] = (u32)(reg << 3);
	cmd[3] = size;
	cmd[4] = IPC_Desc_StaticBuffer(size, 1);
	cmd[5] = (u32)buf;
	Result r = svcSendSyncRequest(i2c_ir);
	return R_FAILED(r) ? r : (Result)cmd[1];
}

static Result svc_read(u8 reg, u8 *buf, u32 size)
{
	u32 *cmd = getThreadCommandBuffer();
	u32 *sb = getThreadStaticBuffers();
	u32 save0 = sb[0], save1 = sb[1];
	sb[0] = IPC_Desc_StaticBuffer(size, 0);
	sb[1] = (u32)buf;
	cmd[0] = IPC_MakeHeader(0x000D, 3, 0); // ReadRegisters8
	cmd[1] = I2C_DEV_IR;
	cmd[2] = (u32)(reg << 3);
	cmd[3] = size;
	Result r = svcSendSyncRequest(i2c_ir);
	if (R_SUCCEEDED(r))
		r = (Result)cmd[1];
	sb[0] = save0;
	sb[1] = save1;
	return r;
}

static bool chip_read(u8 reg, u8 *buf, size_t n)
{
	if (use_svc) {
		for (int attempt = 0; attempt < 4; attempt++) {
			if (R_SUCCEEDED(svc_read(reg, buf, (u32)n)))
				return true;
			st_svc_errors++;
		}
		return false;
	}
	for (int attempt = 0; attempt < 4; attempt++) {
		bool ok = false;
		GUARDED(XFER_US(n), {
			if (bus_start(DEV_IR) && bus_byte((u8)(reg << 3), false) && bus_start(DEV_IR | 1)) {
				ok = true;
				for (size_t i = 0; i < n && ok; i++) {
					bool last = (i == n - 1);
					ok = bus_wait();
					if (!ok)
						break;
					REG_CNT = last ? 0xE1 : 0xF0; // read; ACK all but the last byte
					ok = bus_wait();
					buf[i] = REG_DATA;
				}
			}
			if (!ok)
				bus_abort();
		});
		if (ok)
			return true;
	}
	return false;
}

// Statistics for the log (reset by ir_raw_stats_reset).
static unsigned st_bus_retries, st_fifo_retries, st_packet_resends, st_lsr_faults;

// One write transaction. Returns the number of data bytes the chip accepted
// (n on success); on failure the bus is reset.
static size_t chip_write_once(u8 reg, const u8 *buf, size_t n)
{
	if (use_svc) {
		// All or nothing as far as we can tell; a failure counts as "none in".
		if (R_SUCCEEDED(svc_write(reg, buf, (u32)n)))
			return n;
		st_svc_errors++;
		return 0;
	}
	size_t done = 0;
	GUARDED(XFER_US(n), {
		if (bus_start(DEV_IR) && bus_byte((u8)(reg << 3), false)) {
			while (done < n && bus_byte(buf[done], done == n - 1))
				done++;
		}
		if (done != n)
			bus_abort();
	});
	return done;
}

// For FIFO data (retry = false) a retry is only safe while no data byte has
// been accepted yet; after that, resending would duplicate bytes on the air.
static bool chip_write(u8 reg, const u8 *buf, size_t n, bool retry)
{
	for (int attempt = 0; attempt < 4; attempt++) {
		if (attempt)
			st_bus_retries++;
		size_t done = chip_write_once(reg, buf, n);
		if (done == n)
			return true;
		if (!retry && done > 0)
			return false;
		if (!retry)
			st_fifo_retries++;
	}
	return false;
}

static u8 rd(u8 reg)
{
	u8 v = 0xFF;
	if (!chip_read(reg, &v, 1))
		bus_fault = 1;
	return v;
}

static void wr(u8 reg, u8 value)
{
	if (!chip_write(reg, &value, 1, true))
		bus_fault = 1;
}

// --- UART ------------------------------------------------------------------

static void arm_receive(void)
{
	wr(R_FCR, 0x03);          // FIFOs on, clear the receive FIFO
	wr(R_EFCR, EFCR_TX_OFF);  // receiver on, transmitter off
	rx_armed = 1;
}

// Put one packet into the transmit FIFO. Returns 1 when every byte went in,
// 0 when it failed before all bytes went in (only a fragment can have gone
// out; the walker drops it on its checksum), -1 on a stuck transmitter.
static int fill_fifo(const uint8_t *data, size_t len, u64 deadline)
{
	bus_fault = 0;
	wr(R_FCR, 0x05);                         // FIFOs on, clear the transmit FIFO
	wr(R_EFCR, EFCR_RX_OFF | EFCR_TX_OFF);  // both off while the packet is loaded
	rx_armed = 0;
	if (bus_fault) {
		dbg_printf("raw: I2C fault while preparing to send");
		return 0;
	}

	// Load as much of the packet as the 64-byte FIFO holds BEFORE the transmitter
	// starts, so it goes out without gaps. Feeding a running transmitter let a
	// short stall on the 3DS side put a gap inside a packet; the walker then
	// gave up on the whole link (tests 7 and 8: every drop began with a send
	// that took ~150 us longer than normal).
	size_t first = len < 64 ? len : 64;
	// v0.12 (9.2c): with the guard on, load in pieces of at most MAX_XFER_DATA
	// bytes so each transfer fits between bursts. The transmitter is off, so the
	// pieces cost nothing on the air; a piece that fails after earlier pieces went
	// in clears the FIFO and starts the load again (nothing was sent yet).
	size_t piece = guard_on ? MAX_XFER_DATA : first;
	int loaded = 0;
	for (int restart = 0; restart < 3 && !loaded; restart++) {
		if (restart) {
			bus_fault = 0;
			wr(R_FCR, 0x05); // clear the transmit FIFO
			if (bus_fault)
				continue;
		}
		size_t in = 0;
		while (in < first) {
			size_t n = first - in < piece ? first - in : piece;
			if (!chip_write(R_THR, data + in, n, false))
				break;
			in += n;
		}
		loaded = in == first;
	}
	if (!loaded) {
		dbg_printf("raw: FIFO load failed (nothing sent)");
		return 0;
	}
	// v0.12 (9.2d): a short packet goes out inside one gap between bursts - air
	// time, the idle check and arm_receive() - because the walker's reply (SYN
	// answer, pong) starts ~0.5 ms after our packet.
	if (len <= 64)
		guard_reserve((u32)(len * 87 + 500));
	bus_fault = 0;
	wr(R_EFCR, EFCR_RX_OFF);  // transmitter on, receiver off (we would hear ourselves)
	tx_on_us = dbg_now_us();  // v0.11 (LINK_REVIEW 7.3a): the clock the send checks rely on
	if (bus_fault) {
		dbg_printf("raw: I2C fault while starting the transmitter");
		return 0;
	}

	size_t done = first;
	tx_loaded = done;
	tx_active = done < len;
	while (done < len) {
		u64 now = dbg_now_us();
		if (now > deadline) {
			tx_active = 0;
			dbg_printf("raw: transmit FIFO never drained");
			return -1;
		}
		// v0.12 (9.2e): a burst is about to force a hold-off and the FIFO (by the
		// clock) is running low: top it up first, without the level reads, if a
		// piece still fits before the guard time.
		if (guard_on) {
			size_t gone = (size_t)((now - tx_on_us) / 88);
			if (gone > done)
				gone = done;
			size_t queued = done - gone;
			size_t n = len - done;
			if (n > MAX_XFER_DATA)
				n = MAX_XFER_DATA;
			if (n > 64 - queued)
				n = 64 - queued;
			int burst_near = now + 2 * XFER_US(1) + XFER_US(MAX_XFER_DATA) + GUARD_BEFORE_US >= next_burst;
			int fits = now + XFER_US(n) + GUARD_BEFORE_US < next_burst;
			if (burst_near && queued < 24 && n > 0 && fits) {
				if (!chip_write(R_THR, data + done, n, false)) {
					tx_active = 0;
					dbg_printf("raw: FIFO write failed at byte %u of %u", (unsigned)done, (unsigned)len);
					return 0;
				}
				done += n;
				tx_loaded = done;
				continue;
			}
		}
		// v0.11 (7.3c): a collision can corrupt a single TXLVL read. Use the level
		// only if two reads succeed and agree (the second may be up to 2 larger:
		// bytes leave while we read), then take the first, smaller one.
		bus_fault = 0;
		u8 space = rd(R_TXLVL);
		u8 space2 = rd(R_TXLVL);
		if (bus_fault || space == 0 || space > 64 || space2 < space || space2 > space + 2) {
			if (!bus_fault && space != space2)
				st_unequal_reads++;
			continue; // a failed or doubtful level read is harmless: just read it again
		}
		// ... and never more than the clock allows: bytes surely gone since the
		// transmitter started (88 us each, generous), never more than were loaded.
		size_t left = (size_t)((now - tx_on_us) / 88);
		if (left > done)
			left = done;
		size_t queued = done - left;
		if (queued >= 64)
			continue;
		size_t room = 64 - queued;
		size_t n = len - done;
		if (n > space)
			n = space;
		if (n > room)
			n = room;
		if (guard_on && n > MAX_XFER_DATA) // v0.12 (9.2c): pieces fit between bursts
			n = MAX_XFER_DATA;
		if (!chip_write(R_THR, data + done, n, false)) {
			tx_active = 0;
			dbg_printf("raw: FIFO write failed at byte %u of %u", (unsigned)done, (unsigned)len);
			return 0;
		}
		done += n;
		tx_loaded = done;
	}
	tx_active = 0;
	return 1;
}

static int raw_send_inner(void *ctx, const uint8_t *data, size_t len)
{
	(void)ctx;
	u64 t0 = dbg_now_us();
	u64 deadline = t0 + 300000;
	guard_wait_accum = 0;

	// The system can put the chip back to sleep (IER bit 4) or change its setup
	// while we are idle (test 5: LSR read 00 and the FIFO never drained after a
	// pause). One register read per packet catches that; set it up again.
	// v0.11 (7.3d): act only on two IER reads in a row that agree and are
	// non-zero; a single odd value (a collision) is counted and ignored.
	bus_fault = 0;
	u8 ier = rd(R_IER);
	u8 ier2 = rd(R_IER);
	if (!bus_fault && ier != ier2) {
		st_unequal_reads++;
	} else if (!bus_fault && ier != 0x00) {
		st_reconfigs++;
		if (ff_logged < 6) {
			ff_logged++;
			log_chip("changed while idle (before send)");
		}
		chip_configure();
	}

	int r = 0;
	for (int attempt = 0; attempt < 3 && r == 0; attempt++) {
		if (attempt) {
			st_packet_resends++;
			svcSleepThread(6000000LL); // > the walker's end-of-packet gap
			dbg_printf("raw: sending the packet again (try %d)", attempt + 1);
		}
		r = fill_fifo(data, len, deadline);
		if (r < 0 && attempt == 0) {
			// Transmitter stuck: set the chip up again and give it one more go.
			st_reconfigs++;
			log_chip("transmitter stuck");
			chip_configure();
			deadline = dbg_now_us() + 300000;
			r = 0;
		}
	}
	if (r <= 0)
		return -1;

	// Every byte is in the FIFO, so the packet goes out whatever happens on
	// the bus now. Never resend from here: the walker would answer twice.
	// Wait until the last stop bit has left the shift register; if the status
	// reads keep failing, fall back to the time the bytes need on the air.
	//
	// v0.11 (7.3b): a corrupted LSR read with bit 6 set used to end this wait
	// early; arm_receive() then switched the transmitter off with bytes still in
	// the FIFO (tests 6 and 9: 136-byte sends ~250 us short, then the link was
	// gone). Now "idle" needs BOTH the full air time since the transmitter
	// started AND two LSR reads in a row with bits 5+6 set.
	u64 air_until = dbg_now_us() + (u64)len * 87 + 500;
	u64 earliest = tx_on_us + (u64)len * 86;
	int good_reads = 0;
	for (;;) {
		bus_fault = 0;
		u8 lsr = rd(R_LSR);
		u64 now = dbg_now_us();
		if (!bus_fault && (lsr & 0x60) == 0x60) {
			if (now < earliest) {
				st_early_idle++;
				good_reads = 0;
			} else if (++good_reads >= 2) {
				break;
			}
		} else {
			good_reads = 0;
		}
		if (bus_fault)
			st_lsr_faults++;
		if (bus_fault && now > air_until)
			break;
		if (now > deadline) {
			// Test 9: once this happened it repeated on every send until the link
			// was lost. Record the chip state and set the chip up again so the
			// retry of this packet can go out.
			st_reconfigs++;
			log_chip("transmitter never went idle");
			chip_configure();
			return -1;
		}
	}
	bus_fault = 0;
	arm_receive(); // catch the reply as early as possible
	if (bus_fault) {
		// One more try: without the receiver on, the reply is lost.
		bus_fault = 0;
		arm_receive();
		if (bus_fault) {
			dbg_printf("raw: I2C fault while turning the receiver on");
			return -1;
		}
	}
	// v0.12 (9.2h): the first number excludes time spent waiting for bursts, so it
	// stays comparable with tests 1-11; the guard wait is shown separately.
	unsigned long guard_us = (unsigned long)guard_wait_accum;
	unsigned long took = (unsigned long)(dbg_now_us() - t0) - guard_us;
	// Normal: air time (87 us per byte) + ~0.7 ms of I2C work + ~1.4 ms for loading
	// the first 64 bytes before the transmitter starts.
	if (took > len * 87 + 3000) {
		dbg_printf("raw: sent %u byte(s) in %lu us (+ %lu us guard)  ** slow (stall during the send)",
		           (unsigned)len, took, guard_us);
	} else if (took < len * 86) {
		// v0.11 (7.3e): shorter than the air time - cannot have been a full packet.
		st_short_sends++;
		dbg_printf("raw: sent %u byte(s) in %lu us (+ %lu us guard)  ** short", (unsigned)len, took, guard_us);
	} else {
		dbg_printf("raw: sent %u byte(s) in %lu us (+ %lu us guard)", (unsigned)len, took, guard_us);
	}
	return 0;
}

void ir_raw_stats_log(void)
{
	// v0.13 (LINK_REVIEW 11.1a): two lines, so neither can exceed the log's line buffer.
	dbg_printf("raw: stats: bus retries %u (FIFO %u), packet resends %u, status-read faults %u, "
	           "all-0xFF bursts %u, chip set up again %u, service errors %u (access: %s), "
	           "early idle %u, unequal reads %u, short sends %u",
	           st_bus_retries, st_fifo_retries, st_packet_resends, st_lsr_faults, st_ff_bursts, st_reconfigs,
	           st_svc_errors, ir_raw_access_name(), st_early_idle, st_unequal_reads, st_short_sends);
	long emin, emed, emax;
	guard_err_stats(&emin, &emed, &emax);
	dbg_printf("raw: guard: waits %u, missed %u (early %u, none %u), burst error min/median/max %ld/%ld/%ld us, "
	           "late %u, fifo low %u, period %ld us, resyncs %u (guard %s)",
	           st_guard_waits, st_guard_missed, st_miss_early, st_miss_none, emin, emed, emax, st_late, st_fifo_low,
	           (long)burst_period, st_resyncs,
	           guard_on ? "on" : (ir_raw_guard_enabled ? "on, not locked" : "off"));
	st_bus_retries = st_fifo_retries = st_packet_resends = st_lsr_faults = st_ff_bursts = st_reconfigs = 0;
	st_svc_errors = st_early_idle = st_unequal_reads = st_short_sends = 0;
	st_guard_waits = st_guard_missed = st_miss_early = st_miss_none = st_late = st_fifo_low = 0;
	st_resyncs = miss_logged = 0;
	st_err_set = 0;
	err_count = 0;
}

static int raw_recv_inner(void *ctx, uint8_t *buf, size_t cap, uint32_t first_ms)
{
	(void)ctx;
	bus_fault = 0;
	if (!rx_armed)
		arm_receive();

	u64 t0 = dbg_now_us();
	u64 deadline = t0 + (u64)first_ms * 1000;
	u64 last_data = 0;
	size_t total = 0;
	u8 lsr_during = 0; // v0.11 (7.4): LSR read BEFORE each FIFO chunk, ORed together
	while (total < cap && !bus_fault) {
		u8 level = rd(R_RXLVL);
		u64 t = dbg_now_us();
		if (bus_fault) {
			// A failed level read loses nothing; read it again. Give up only if
			// the bus stays broken past the reply deadline.
			bus_fault = 0;
			st_bus_retries++;
			if (t > deadline + 50000)
				break;
			continue;
		}
		if (level && level <= 64) {
			size_t n = level;
			if (n > cap - total)
				n = cap - total;
			if (guard_on && n > MAX_XFER_DATA) // v0.12 (9.2c): the rest is read on the next turn
				n = MAX_XFER_DATA;
			// Line-error bits (7 FIFO error, 4-2 break/framing/parity, 1 overrun)
			// belong to bytes still in the FIFO, so read LSR before draining it.
			// A failed read is skipped, not fatal.
			u8 pre = rd(R_LSR);
			if (bus_fault)
				bus_fault = 0;
			else
				lsr_during |= pre;
			if (!chip_read(R_RHR, buf + total, n)) {
				bus_fault = 1;
				break;
			}
			total += n;
			last_data = t;
		} else if (total) {
			if (t - last_data > IDLE_GAP_US)
				break;
		} else if (t > deadline) {
			break;
		}
	}
	if (bus_fault) {
		dbg_printf("raw: I2C fault during receive");
		return -1;
	}
	if (total) {
		u8 lsr = rd(R_LSR);
		dbg_printf("raw: recv %u byte(s) after %lu us (LSR %02x, during %02x)", (unsigned)total,
		           (unsigned long)(dbg_now_us() - t0), lsr, lsr_during);
	}
	if (all_ff(buf, total)) {
		// Not decoding IrDA (test 1/2: the walker's 0xFC call arrived as 0xFF).
		// Record what the chip looks like, then set it up again.
		st_ff_bursts++;
		if (ff_logged < 3) {
			ff_logged++;
			log_chip("on an all-0xFF burst");
		}
		bus_fault = 0;
		chip_configure();
		bus_fault = 0;
		arm_receive();
	}
	return (int)total;
}

// v0.13 (LINK_REVIEW 11.2c/d): the transport entry points. A pending re-sync
// runs only at a packet boundary - before a send's pre-load, or in a receive
// while no reply is expected (e.g. waiting for the walker's call) - never
// inside a send or inside a reply.
static int raw_send(void *ctx, const uint8_t *data, size_t len)
{
	if (resync_pending)
		guard_resync_now();
	in_send = 1;
	int r = raw_send_inner(ctx, data, len);
	in_send = 0;
	reply_expected = r == 0;
	return r;
}

static int raw_recv(void *ctx, uint8_t *buf, size_t cap, uint32_t first_ms)
{
	if (resync_pending && !reply_expected)
		guard_resync_now();
	int r = raw_recv_inner(ctx, buf, cap, first_ms);
	reply_expected = 0;
	return r;
}

// --- open / close ----------------------------------------------------------

static bool io_region_mapped(void)
{
	MemInfo info;
	PageInfo page;
	if (R_FAILED(svcQueryMemory(&info, &page, I2C_BASE)))
		return false;
	return info.state == MEMSTATE_IO && (info.perm & MEMPERM_READWRITE) == MEMPERM_READWRITE;
}

static void set_divisor(u8 divisor)
{
	wr(R_LCR, 0x83);      // open the divisor latch, 8N1
	wr(R_DLL, divisor);
	wr(R_DLH, 0);
	wr(R_LCR, 0x03);      // 8 data bits, no parity, 1 stop bit
}

// EFR (enhanced features) is only reachable while LCR = 0xBF. Its bit 4 must be
// set for writes to IER bits 4-7 (bit 4 = sleep mode) and MCR bits 5-7
// (bit 6 = IrDA mode) to take effect - without it those writes are ignored.
static u8 read_efr(void)
{
	wr(R_LCR, 0xBF);
	u8 v = rd(R_EFR);
	wr(R_LCR, 0x03);
	return v;
}

static void write_efr(u8 v)
{
	wr(R_LCR, 0xBF);
	wr(R_EFR, v);
	wr(R_LCR, 0x03);
}

static u8 read_divisor(void)
{
	wr(R_LCR, 0x83);
	u8 dll = rd(R_DLL), dlh = rd(R_DLH);
	wr(R_LCR, 0x03);
	return dlh ? 0xFF : dll;
}

static void log_chip(const char *when)
{
	u8 efr = read_efr(), div = read_divisor();
	dbg_printf("raw: chip %s: IER %02x LCR %02x MCR %02x EFR %02x EFCR %02x IOSTATE %02x divisor %u", when,
	           rd(R_IER), rd(R_LCR), rd(R_MCR), efr, rd(R_EFCR), rd(R_IOSTATE), div);
}

// Put the chip into the state the link needs: 115200 baud 8N1, IrDA mode,
// sleep mode off, no interrupts, transceiver on. Returns true if a read-back
// confirms it.
static bool chip_configure(void)
{
	wr(R_EFCR, EFCR_RX_OFF | EFCR_TX_OFF);
	wr(R_FCR, 0x00);
	write_efr((u8)(read_efr() | 0x10)); // unlock IER[7:4] / MCR[7:5]
	set_divisor(10);                     // 115200 baud
	if (read_divisor() != 10)
		set_divisor(10);                 // the first write does not always stick
	wr(R_IER, 0x00);                     // no interrupts, sleep mode off
	wr(R_MCR, (u8)((rd(R_MCR) & 0x1F) | 0x40)); // IrDA on, clock prescaler 1
	wr(R_IOSTATE, 0x00);                 // transceiver out of shutdown
	rx_armed = 0;

	u8 ier = rd(R_IER), mcr = rd(R_MCR), lcr = rd(R_LCR), div = read_divisor();
	bool ok = !bus_fault && ier == 0x00 && (mcr & 0xC0) == 0x40 && lcr == 0x03 && div == 10;
	if (!ok)
		dbg_printf("raw: configure did not stick: IER %02x MCR %02x LCR %02x divisor %u%s", ier, mcr, lcr,
		           div, bus_fault ? " (bus fault)" : "");
	return ok;
}

// The signature of a chip that is not decoding IrDA (or woke from sleep on
// the byte): every received byte is 0xFF on the wire.
static bool all_ff(const uint8_t *buf, size_t n)
{
	for (size_t i = 0; i < n; i++)
		if (buf[i] != 0xFF)
			return false;
	return n > 0;
}

// v0.11 test A (LINK_REVIEW 7.2): watch the bus for other devices' transfers.
// Only READS the I2C control register (busy = bit 7); writes nothing, so it
// changes nothing on the bus. Busy periods shorter than one loop turn can be
// missed - the caller reports the loop turn time from *turns.
int ir_raw_bus_watch(uint32_t duration_us, uint32_t *starts, uint32_t *lens, int cap, uint32_t *turns,
                     uint32_t *dropped)
{
	*turns = 0;
	*dropped = 0;
	if (!io_region_mapped())
		return -1;
	u64 t0 = dbg_now_us(), end = t0 + duration_us, bstart = 0;
	int n = 0, busy = 0;
	uint32_t it = 0;
	for (;;) {
		u64 now = dbg_now_us();
		if (now >= end)
			break;
		int b = (REG_CNT & 0x80) != 0;
		it++;
		if (b && !busy) {
			busy = 1;
			bstart = now;
		} else if (!b && busy) {
			busy = 0;
			if (n < cap) {
				starts[n] = (uint32_t)(bstart - t0);
				lens[n] = (uint32_t)(now - bstart);
				n++;
			} else {
				(*dropped)++;
			}
		}
	}
	*turns = it;
	return n;
}

// v0.12 bus sniff (LINK_REVIEW 9.3): like the watch, but at every busy -> idle
// edge also read the controller's data and control registers - the byte that
// was just on the bus and the control bits after it. Plain reads, nothing is
// written. Stores up to cap edges; returns the count (-1 if not mapped).
int ir_raw_bus_sniff(uint32_t duration_us, uint32_t *times, uint8_t *data, uint8_t *cnt, int cap)
{
	if (!io_region_mapped())
		return -1;
	u64 t0 = dbg_now_us(), end = t0 + duration_us;
	int n = 0, busy = 0;
	for (;;) {
		u64 now = dbg_now_us();
		if (now >= end || n >= cap)
			break;
		u8 c = REG_CNT;
		int b = (c & 0x80) != 0;
		if (b && !busy) {
			busy = 1;
		} else if (!b && busy) {
			busy = 0;
			times[n] = (uint32_t)(now - t0);
			data[n] = REG_DATA;
			cnt[n] = REG_CNT;
			n++;
		}
	}
	return n;
}

// Decide between the i2c::IR service and direct bus access. The service is
// only used if a self-check on the chip's scratch-pad register agrees with
// direct access both ways.
static void choose_access(void)
{
	use_svc = 0;
	if (!ir_raw_prefer_service) {
		dbg_printf("raw: access = direct bus (chosen in the menu)");
		return;
	}
	if (svc_unavailable) {
		dbg_printf("raw: i2c::IR did not open earlier in this run; access = direct bus");
		return;
	}
	if (!i2c_ir) {
		dbg_printf("raw: opening i2c::IR (on a helper thread, 500 ms limit)");
		if (ir_raw_checkpoint)
			ir_raw_checkpoint();
		open_done = 0;
		s32 prio = 0x30;
		svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
		Thread t = threadCreate(open_thread, NULL, 0x2000, prio > 0x18 ? prio - 1 : prio, -2, true);
		if (!t) {
			svc_unavailable = 1;
			dbg_printf("raw: could not start the helper thread; access = direct bus");
			return;
		}
		for (int i = 0; i < 50 && !open_done; i++)
			svcSleepThread(10000000LL); // 10 ms
		if (!open_done) {
			svc_unavailable = 1;
			dbg_printf("raw: i2c::IR did not answer within 500 ms (in use by the system); access = direct bus");
			return;
		}
		if (R_FAILED(open_result) || !i2c_ir) {
			svc_unavailable = 1;
			dbg_printf("raw: i2c::IR not available (%08lx); access = direct bus", (unsigned long)open_result);
			return;
		}
		dbg_printf("raw: i2c::IR opened");
		if (ir_raw_checkpoint)
			ir_raw_checkpoint();
	}
	bus_fault = 0;
	u8 spr = rd(R_SPR);
	use_svc = 1;
	wr(R_SPR, 0x5A);
	use_svc = 0;
	u8 a = rd(R_SPR);
	wr(R_SPR, 0xC3);
	use_svc = 1;
	u8 b = rd(R_SPR);
	u8 lcr_svc = rd(R_LCR);
	use_svc = 0;
	u8 lcr_raw = rd(R_LCR);
	wr(R_SPR, spr);
	int ok = !bus_fault && a == 0x5A && b == 0xC3 && lcr_svc == lcr_raw;
	use_svc = ok;
	dbg_printf("raw: i2c::IR self-check %s (wrote 5A read %02x, wrote C3 read %02x, LCR %02x/%02x); access = %s",
	           ok ? "passed" : "FAILED", a, b, lcr_svc, lcr_raw, ok ? "i2c::IR service" : "direct bus");
	if (ir_raw_checkpoint)
		ir_raw_checkpoint();
	bus_fault = 0;
}

const char *ir_raw_access_name(void)
{
	return use_svc ? "i2c::IR service" : "direct bus";
}

int ir_raw_open(pw_transport *out)
{
	if (!io_region_mapped()) {
		dbg_printf("raw: I/O registers are not mapped (needs Luma3DS's homebrew loader)");
		return -1;
	}
	bus_fault = 0;
	choose_access();

	saved_ier = rd(R_IER);
	saved_iostate = rd(R_IOSTATE);
	saved_efcr = rd(R_EFCR);
	saved_mcr = rd(R_MCR);
	u8 lcr = rd(R_LCR), iodir = rd(R_IODIR);
	if (bus_fault) {
		dbg_printf("raw: the IR chip did not answer on the I2C bus");
		return -1;
	}
	saved_efr = read_efr();
	wr(R_LCR, lcr);
	dbg_printf("raw: chip before: IER %02x LCR %02x MCR %02x EFR %02x EFCR %02x IODIR %02x IOSTATE %02x",
	           saved_ier, lcr, saved_mcr, saved_efr, saved_efcr, iodir, saved_iostate);

	st_ff_bursts = 0;
	ff_logged = 0;
	if (!chip_configure()) {
		bus_fault = 0;
		if (!chip_configure())
			dbg_printf("raw: chip setup not confirmed; trying the link anyway");
		bus_fault = 0;
	}
	log_chip("after setup");

	// v0.12 (9.2a): lock onto the burst rhythm, with no I2C of ours meanwhile.
	guard_sync();

	rx_armed = 0;
	raw_open_flag = 1;
	out->send = raw_send;
	out->recv = raw_recv;
	out->ctx = NULL;
	return 0;
}

void ir_raw_close(void)
{
	if (!raw_open_flag)
		return;
	bus_fault = 0;
	wr(R_EFCR, EFCR_RX_OFF | EFCR_TX_OFF);
	wr(R_FCR, 0x00);
	wr(R_IOSTATE, saved_iostate);
	wr(R_IER, saved_ier);       // EFR bit 4 is still set, so sleep mode comes back too
	wr(R_MCR, saved_mcr);
	write_efr(saved_efr);
	wr(R_EFCR, saved_efcr);
	raw_open_flag = 0;
	rx_armed = 0;
	guard_on = 0;
	resync_pending = 0;
}
