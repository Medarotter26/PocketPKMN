# IR link review (2026-11-14)

Scope: read-only review of `source/ir_raw.c`, `source/pw_proto.c`, `source/main.c`, `RESUME.md`,
`docs/RESEARCH.md` and the cumulative hardware log `hw/test10/link_log_service_try.txt` (it contains tests 1-10:
10,608 sends, 10,694 receives). Nothing was built or run on the console. Outside sources: libctru (`hid.c`,
`irrst.c`, `appInit.c`), pwalkerHax (`i2c.c`, `ir.c`, `pokewalker.c`, `main.c`, `ui.c`, issues), Dmitry Grinberg's
Pokewalker article, picowalker-core (`ir.c`, `ir.h`, `actions.c`). Anything not confirmed by a log or a source is
marked **[unverified]**.

## 1. Short answer

1. **The damaged replies are made on the 3DS's I2C bus, not on the air** (confidence ~80 %). Their shape cannot
   come from infrared noise: bytes read as wire `FF` or wire `00` in groups of exactly as many bytes as we asked the
   chip for, and clean single-byte losses.
2. **Our own app switches on the thing we collide with.** libctru's `hidInit()` (called by the default
   `__appInit`) calls `irrstInit()` on a New 3DS, which calls `IRRST_Initialize(10, 0)`. WalkerBox never overrides
   this. That the IR module stops reading the C-stick when this is not done is **[unverified]** - test A measures it.
3. **Not one missed reply has ever recovered**: 18 "no reply" episodes in tests 1-10, 0 recovered. Every retry that
   followed a missed reply (after 100 ms or 500 ms) went unanswered. Retries only ever helped when the walker's
   *reply* was damaged, never when the walker did not answer.
4. **Two of the three upload drops began with a 136-byte send that ended ~250 us EARLY**, not late. The send loop
   trusts a single `LSR` read to decide the transmitter is idle; a corrupted read ends the send early and
   `arm_receive()` switches the transmitter off with bytes still in the FIFO, so the walker gets a packet with its
   tail cut off. This is a bug in our code that a collision triggers.
5. pwalkerHax has no countermeasure for any of this (no retries, no ir:rst handling, transmitter on while
   filling); its author has no New 3DS and suggests timing tweaks. Nothing to take from it except that the real
   walker accepts command `0A` writes of any length.

## 2. Evidence

### 2.1 What the damaged replies look like

317 damaged replies out of ~10,600 exchanges (3.0 %): 201 bad checksum, 111 wrong length, 5 too short. Each damaged
128-byte read reply was aligned against the EEPROM backups (`hw/test*/eeprom_*.bin`). Result for the ~300 replies
of 124-139 bytes:

| Reply length | Count | What happened |
|---|---|---|
| 135 | 132 | exactly one data byte missing, everything else intact |
| 136 | 29 | one data byte replaced by decoded `55` (= wire `FF`) |
| 137 | 26 | one data byte replaced by two wire `FF` |
| 138 | 22 | one data byte replaced by three wire `FF` |
| 139 | 32 | three decoded `AA` (= wire `00`) inserted, nothing lost |
| 133/134 | 13 | 2-3 bytes in a row missing |
| rest | ~45 | mixes of the above; ~15 with long runs (8-52 bytes) shifted |

Inserted byte values (decoded): `00` 251, `FF` 195, `AA` 98, `55` 59, all others together 11
(the `00`/`FF` ones are mostly the long-run cases in blank EEPROM areas). Damage positions are spread evenly over
the reply.

Why this is not infrared:
- A stray light pulse can create a wire `FF` byte, but it cannot at the same moment remove one real byte and
  leave all later bytes intact. A missed start pulse garbles a byte (the receiver re-syncs on a later 0 bit); it
  does not delete it cleanly. Three wire `00` bytes (27 pulses in the right places) cannot be noise.
- "One real byte gone, N x `FF` in its place, N = 1..3" matches one hijacked read: we read `RXLVL` = N, start a
  read of N bytes from `RHR`; the chip pops one byte for the transfer; the bus is taken away; our N byte reads
  return `FF` from an unaddressed bus. "Three `00` inserted, nothing lost" is the same with the bus taken before
  the chip was addressed. The exact interleaving is **[unverified]**; the bus-level origin is the conclusion.
- The same log shows bus corruption directly: `IER` read back as `df`, `43`, `df` (lines 8181, 13794, 26081) with
  every other register normal and `IER 00` on the next read; 15 "FIFO write/load failed" events (a NACK on our own
  write).

Note: "LSR 60, so no line errors" is **not** evidence for a clean line. `raw_recv` reads `LSR` once, after the
FIFO is empty; framing/parity flags belong to the byte at the top of the FIFO and bit 7 clears when the FIFO is
drained. Only the overrun bit (bit 1) is sticky, so the one thing the log does prove is **no receive overrun**
(10,623 x `60`). The 71 x `LSR 00` are impossible values (bit 5/6 must be set with an idle transmitter) = more
corrupted reads.

### 2.2 Where the collisions come from

- The app core and the system modules run on different cores, so the IR module's I2C transfers (via the i2c
  module) can interleave with ours between any two bus bytes. `bus_wait()` only waits for the current byte; it is
  no lock. Thread priority or pinning cannot prevent this.
- libctru `hid.c`: `usingIrrst = hidShouldUseIrrst();` / `if(usingIrrst) ret = irrstInit();`;
  `hidShouldUseIrrst` is a **weak** function returning true on New 3DS. `irrst.c`: `irrstInit` calls
  `IRRST_Initialize(10, 0)` (arguments unnamed in the source; 3dbrew describes the first as the update period -
  **[unverified]**); `irrstExit` calls `IRRST_Shutdown()`. `appInit.c`: `__appInit` calls `hidInit()`.
  WalkerBox's `main.c` has no `irrst*` call and no override, so ir:rst is running for the whole session.
- Send times show foreign bus traffic: 11-byte sends have a tail of +20..+60 us in ~10 % of sends and +100..+400 us
  in ~1 %; most are harmless.
- Rate: about one damaged reply per 0.7 s of receive polling. If the C-stick is read every ~10 ms
  **[unverified]**, only a few percent of the collisions do visible damage - plausible, since most land between
  our transactions.

### 2.3 What precedes each permanent drop

All 18 no-reply episodes are final. First failing send of each, against the median for that version and size:

| Test / version | Packet | Send time vs normal | Notes |
|---|---|---|---|
| v0.1 x2 | 11 B | +392, +178 us | |
| v0.3 | 11 B | +134 us | |
| v0.5 x3 | 11 B | 0 | chip asleep (fixed in v0.6) |
| v0.6 x3 | 8 B ping | ~0 | idle walker had already left (fixed in v0.7) |
| v0.6 | **136 B** | **-255 us** | upload |
| v0.7 | 11 B | +128 us | |
| v0.7 | 136 B | +46 us | upload, nothing unusual |
| v0.8 x2 | 11 B | +168, +128 us | |
| v0.9 | 11 B | +1192 us | then "transmitter never went idle" |
| v0.9 | **136 B** | **-248 us** | upload |
| v0.10 | 11 B | +128 us | **with the v0.9 pre-load in place** |

Readings:
- **11-byte drops: a ~130-400 us delay precedes nearly all of them, and still does in v0.10**, where the packet
  is loaded with the transmitter off. A gap inside the packet is no longer possible there, so the "stall puts a
  gap in the packet" explanation (RESUME, test 10) is at best half the story. The delay is the length of a
  foreign bus transfer (6-7 bus bytes at ~21 us); the damage is what that transfer does to *our* transfer at that
  moment (a lost `EFCR`/`THR` write whose ACK we misread, or an early "idle" - see next point). ~130 such delayed
  sends were fine; ~9 were fatal.
- **136-byte drops: two of three were sends that finished ~250 us early = ~3 bytes of air time.** The wait loop
  in `raw_send` leaves on the first `LSR` read with bit 6 set. A corrupted read (`FF`, or a foreign device's byte)
  has bit 6 set often enough. `arm_receive()` then writes `EFCR = TX off`: the last bytes never leave. Two more
  early sends in the log confirm the mechanism: 9439 us (-3045, line 10011) and 7424 us (-5077, line 12886), both
  followed by `LSR 00` and a junk reply (1 byte; 9 bytes containing a `04` ack) - there the link survived and the
  retry worked.
- The top-up path has the same weakness: one corrupted `TXLVL` read that is too large makes us write more than
  the FIFO can take, and the chip drops the excess silently **[unverified as an observed event]**.
- 136-byte packets: 191 acked, 3 fatal episodes (1.5 %); 11-byte read requests: ~10,300 sent, ~9 fatal (0.09 %).
  Per packet the long writes are ~15x more dangerous - they spend ~12 ms in loops that trust single register
  reads, the short ones ~1 ms.

### 2.4 Why no retry ever works

Two explanations fit the logs equally; no source settles it:
- (i) the walker ends the connection when it receives a damaged packet, or
- (ii) the walker's idle limit is below ~100 ms, so our retry (sent after `reply_ms` = 100 ms) is already too late.
  Known bound: < 0.4 s (test 6).

Dmitry's article: packets with the wrong session id are ignored; behaviour on a bad checksum and the connected
timeout are not given. picowalker-core returns an error without replying; its timeout lives in the driver, not
read. The "50 ms" in RESUME is **[unverified]**. Against (i): the two badly truncated writes above did not kill
the link. Replies normally start 0.5-5 ms after our packet (identity ~26 ms), so waiting 100 ms for the first
byte is 4-20x longer than needed. Test C separates (i) from (ii).

## 3. Causes, by place

| Place | Damaged replies (a) | Permanent drops (b) |
|---|---|---|
| On the air | Small share at most. Not excluded for some of the plain bad-checksum cases. ~10 % | Not indicated. The "cause on the air" note for test 9 drop 2 is not supported: that send was 248 us short. ~10 % |
| In the walker | None | It never answers again after one bad or missed packet (fact). Whether that is "quits on a bad packet" or "idle limit < 100 ms" is open (50/50) |
| Our I2C access | **Main cause**: reads hijacked by the IR module's transfers on the same bus (~80 %) | **Trigger**: a collision during a send corrupts the packet (early "idle", lost write, bad level read) (~70 %) |
| Our timing | None (no overrun ever) | Contributes: 100 ms first-byte wait before a retry; 6 x 100 ms before the link is declared lost. The pre-load fix was right for v0.2-0.8 but does not cover the remaining drops |

## 4. Fixes, ranked

| # | Fix | Expected benefit | Risk | Confidence |
|---|---|---|---|---|
| 1 | **Do not start ir:rst**: define `bool hidShouldUseIrrst(void) { return false; }` in the app (libctru's is weak). Alternative: `irrstExit()` before a transfer and `irrstInit()` after | Removes the source of both problems if the IR module only polls device 17 while ir:rst is initialised | Low. Loses ZL/ZR/C-stick, which the app does not use. Do not use `hidWaitForAnyEvent` afterwards (it waits on the ir:rst event). Some other process could still keep the polling alive **[unverified]** | 60 % that it stops the polling; if it does, ~80 % that damaged replies fall below 0.5 % |
| 2 | **Never trust a single register read during a send.** (a) Accept "transmitter idle" only after the computed air time has passed since TX enable (`len` x 86.8 us) AND on two `LSR` reads in a row that agree and look valid (bits 5+6 set, bit 7 clear). (b) Top-up: cap the bytes written by what time allows (64 - bytes still unsent by the clock) and require two equal `TXLVL` reads. (c) Read `IER` twice before re-running `chip_configure()` mid-link | Removes the "send ended early" drops (2 of 3 upload drops) and overflow; no cost when the bus is quiet | Very low; ~0.1-0.2 ms per packet | 85 % that the -250 us class disappears |
| 3 | **Fast retry and fast give-up.** First-byte wait per command: reads/writes/ping 25-30 ms, identity (20/52/40/32) 60 ms, one-shots unchanged. Resend at once on a miss. After 2-3 misses declare the link lost and ask for CONNECT (now: 6 x 100 ms + ping) | Recovers drops if the walker's idle limit is 30-100 ms; in any case the user is told ~0.5 s sooner | Low-medium: a resent 02/82 write or read is harmless (same data, same address); if a reply were only late we would talk over it - the 136-byte reply never took longer than 21.5 ms end-to-end in 9,800 cases (one 40 ms outlier) | 40 % that drops become recoverable |
| 4 | **Write packets that fit the FIFO**: command `0A` (any address/length; real walker only, pwalkerHax uses it with 127 bytes; picowalker does not handle it) with <= 55 data bytes -> packet <= 64 bytes, loaded with TX off, no top-up at all. 128 bytes = 3 packets | Removes the top-up path. Only worth it if fixes 1+2 leave upload drops | Medium: 3x the packets and acks per block; `0A` untested by us on this walker; keep 02/82 for picowalker | 50 % useful after 1+2 |
| 5 | Fewer packets per upload: write all blocks, then one read pass, instead of a read-back after every block | Halves the exposure of an upload | Low | certain but small |
| 6 | Log `LSR` *before* draining the FIFO (bit 7 / bits 2-4) and count early-idle / unequal double reads in the stats line | Tells air errors from bus errors for good | None | diagnostic |
| 7 | Raise thread priority / pin the core | Small. I2C is clocked by us, so a stall cannot corrupt a transfer; it only matters for a top-up underrun (> 5.5 ms stall) and receive overrun (never seen) | Low | 10 % |
| 8 | Chip interrupt / flow control | None: the interrupt line belongs to the IR module; RTS/CTS means nothing on IrDA | - | reject |
| 9 | `i2c::IR` service, `ir:u` | Dead ends (tests 1 and 10) | - | reject |
| 10 | Stopping/patching the IR system module (Luma) | Would remove the polling for sure, but invasive and system-wide | High | not proposed **[unverified]** |

Recommended order: 1 + 2 + 6 in one build (independent, all low risk), measure; then 3; 4 and 5 only if uploads
still drop.

## 5. Hardware test plan

Each test is one run on the console. A and B need one build; all are read-only for the walker.

**Test A - is there foreign bus traffic, and does ir:rst control it? (no walker, ~10 s)**
With WalkerBox doing no I2C of its own, sample `REG_CNT` bit 7 (busy) in a tight loop for 3 s and log: number of
busy periods, their length, the time between them. Three passes: (1) as started (ir:rst initialised by libctru),
(2) after `irrstExit()` (count reaches 0 -> `IRRST_Shutdown`), (3) after `irrstInit()` again. Reading the
register changes nothing.
- Confirms: pass 1 shows regular transfers (expect one every ~10 ms, 100-150 us long **[unverified]**), pass 2
  shows none, pass 3 shows them again.
- Rejects: pass 2 still shows traffic (something else keeps the polling on -> fix 1 is useless, go to fixes 2-4),
  or pass 1 shows none (then the collisions come from elsewhere and section 2.2 is wrong).

**Test B - backup with ir:rst off + hardened send (walker, read-only, 2 backups ~2,000 packets)**
Build with fixes 1, 2, 6. Run "Back up walker" twice.
- Confirms: "retried reads" falls from 30-59 per backup to <= 5; no "FIFO load failed", no "chip changed while
  idle" with odd `IER` values, no `LSR 00`; 0 reconnects in both; the new stats show 0 early-idle / unequal reads.
- Partly: retried reads unchanged but the new pre-drain `LSR` shows bit 7 set on the damaged replies -> the
  damage is on the air after all; section 2.1 is wrong.
- Rejects: retried reads and reconnects as before with clean pre-drain `LSR` and test A pass 2 quiet.
Before B, baseline for comparison (existing logs): 35, 40, 30, 46, 59 retried reads; 1 reconnect in 5 of 8 backups.

**Test C - can a missed reply be recovered? (walker, read-only, same 2 backups, later build)**
Fix 3 on top. Log for every miss: time since the walker's last byte when the retry went out, and whether it was
answered.
- Confirms idle-limit explanation: at least one line "retry answered after N ms". The largest N that works and
  the smallest that fails bound the walker's limit.
- Confirms "walker quits on a bad packet": retries sent 30-40 ms after the miss are never answered, across
  several drops. Then only prevention (fixes 1, 2, 4) helps, and fast give-up is the only gain from fix 3.
If test B removes the drops, C may need many backups to see a single miss; run it only if drops remain.

Uploads (walk start) are the real target but write to the walker; judge them after B on the next normal walk
start: no send flagged shorter than `len` x 87 us, no reconnect in part 2.

## 6. Corrections to earlier notes

- RESUME test 10 "a stall while feeding a running transmitter put a gap inside the packet": true as a risk for
  v0.2-0.8, but the same +128 us signature precedes the v0.10 drop with the pre-load in place; the delay marks a
  bus collision.
- RESUME test 9 "cause on the air (distance/angle/light)": not supported; that send was 248 us short.
- RESUME test 9 "35 failed exchanges (2.8 %)" and "~5 % line noise": the rate is right, "line noise" is not.
- "LSR 60 = no line errors": see 2.1.

## 7. Build brief: v0.11 (fixes 1, 2, 6 + test A)

For the session that builds. Read sections 1, 2.3, 4 and 5 first. Do NOT add fast retry (fix 3), `0A` writes
(fix 4) or the single read pass (fix 5) in this build: one change set per hardware test, or the result cannot be
read. `pw_proto.c` stays untouched; the PC tests (`tests/run.sh`) must still pass. pwalkerHax is GPL-3.0: do not
copy code from it. Set `APP_VERSION` to "0.11".

### 7.1 Fix 1: ir:rst off by default, switchable (`source/main.c`)

- Add `bool hidShouldUseIrrst(void) { return false; }` (overrides libctru's weak function; `hidInit` then never
  calls `irrstInit`). Check in the libctru version inside the Docker image that `hidShouldUseIrrst` exists in
  `hid.c`; if that libctru is older and has no such hook, call `irrstExit()` once at the top of `main()` instead
  and say so in the log.
- The app must not call `hidWaitForAnyEvent` (it does not today; `hidScanInput` is fine).
- SELECT no longer toggles the dead `i2c::IR` route (leave that code in place, unreachable from the menu). SELECT
  now toggles "C-stick service: off (default) / on". "On" = `irrstInit()` before `ir_raw_open`, `irrstExit()`
  after `ir_raw_close`; this gives an A/B comparison in one build. Show the state on the menu and in the log
  header line (`==== ... | ir:rst off|on | model N ====`). Log the result code of `irrstInit()`.

### 7.2 Test A: "Bus watch" menu item (no walker needed)

New menu entry "Bus watch" (text: "Counts other I2C traffic. Needs no walker. Changes nothing."). Three passes of
3 s each; in each, do no I2C of our own and no printing, only read `REG_CNT` (I2C_BASE + 1) bit 7 in a tight loop
with `dbg_now_us()`:
1. ir:rst off (as started), 2. after `irrstInit()` + 200 ms settle, 3. after `irrstExit()` + 200 ms settle.
Per pass log one line: number of busy periods, min / median / max length (us), min / median / max time between
starts (us), and the start times of the first 20. Keep the samples in a fixed array (cap 2000); log after the
loop. `ir_raw.c` needs a small exported helper for this (the register macros live there); it must not write any
register. Flush the log to SD at the end as the other actions do.
Caveat to print in the log: a busy period shorter than one loop turn can be missed; report the loop turn time
(iterations / 3 s).

### 7.3 Fix 2: no single register read is trusted during a send (`source/ir_raw.c`)

a. `fill_fifo` records `tx_on_us = dbg_now_us()` right after the `wr(R_EFCR, EFCR_RX_OFF)` that starts the
   transmitter (file-static). 
b. "Transmitter idle" wait in `raw_send`: leave the loop only when BOTH hold: `now >= tx_on_us + len * 86`
   (115200 baud 8N1 = 86.8 us per byte) and two `LSR` reads in a row succeeded with `(lsr & 0x60) == 0x60`.
   A read with bit 6 set before that time counts as `st_early_idle` and is ignored. Keep the existing 300 ms
   deadline and the bus-fault fallback (`air_until`).
c. Top-up loop in `fill_fifo` (packets > 64 bytes): read `TXLVL` twice; use it only if both reads succeeded and
   are equal or the second is larger by at most 2 (bytes leave while we read) - then use the FIRST (smaller)
   value; otherwise count `st_unequal_reads` and read again. Additionally cap by the clock:
   `left = (now - tx_on_us) / 88` (bytes surely gone, never more than `done`), `room = 64 - (done - left)`;
   write `min(space, room, remaining)`; if that is <= 0, loop. 
d. Before-send check: reconfigure the chip only if two `IER` reads in a row agree and are non-zero; a single odd
   value counts as `st_unequal_reads` and is otherwise ignored.
e. After the send, flag `** short` in the "raw: sent" line when `took < len * 86` (cannot be a full packet), next
   to the existing `** slow`.
f. Do not change `raw_recv`'s read logic in this build (a hijacked `RHR` read cannot be detected there; fix 1 is
   what should remove it).

### 7.4 Fix 6: logging

- `raw_recv`: read `LSR` before each `RHR` chunk read (a failed read is skipped, not fatal), OR the values into
  one byte, and print it in the existing line: `raw: recv N byte(s) after T us (LSR xx, during yy)`. Bits to
  look at later: 7 (error in FIFO), 4-2 (break/framing/parity), 1 (overrun). This adds one bus transfer per poll
  turn (~100 us); the FIFO is 64 deep, so no overrun is expected - if `LSR` bit 1 appears, drop this to one read
  per reply.
- Stats line: add `early idle N, unequal reads N, short sends N`; reset with the others.

### 7.5 Run on the console (user)

1. "Bus watch" once (10 s, no walker).
2. "Back up walker" twice with C-stick service off (default).
3. Optional, same sitting: SELECT -> on, "Back up walker" once (the old behaviour, for comparison).
4. Send back `link_log.txt` and the new backups.
What the log must show: section 5, tests A and B. Baseline: 30-59 retried reads per backup, 1 reconnect in 5 of 8.

### 7.6 Housekeeping

- Build with `.\build.ps1` (Docker in WSL; if it is slow, wait - never start a second copy).
- Tell the user every change made; update `RESUME.md` (state, v0.11 contents, test plan) at the end.
- Copy `walkerbox.3dsx` to the SD card only when the user says the card is in (earlier builds went to
  `I:\3ds\walkerbox.3dsx`).

## 8. Test 11 (v0.11): what it showed

Source: `hw/test11/link_log.txt` from line 26239.

- **Fix 1 rejected.** Bus watch: the same traffic with ir:rst off, on and off again: 9 busy periods of ~24 us
  (one bus byte each at ~400 kHz), 38-47 us apart, ~0.34 ms in all, **every 10.0 ms** (starts 7647 / 17632 /
  27630 us: intervals 9985 and 9998 us). `irrstInit` returned 0, so the service was really on in pass 2.
  ir:rst is not the switch for this traffic.
- **The traffic is strictly periodic**, so it can be predicted.
- **Collisions with it are what kills the link** - now measured directly. 11-byte sends in test 11 (normal send
  time 2100 us +-15):

  | Send time | Sends | Began a permanent drop |
  |---|---|---|
  | normal (within +-15 us) or faster | ~3,050 | 1 (0.03 %) |
  | delayed by more than 15 us (a burst fell inside the send) | ~410 | 6 (1.5 %) |

  6 of the 7 drops began with a delayed send; a delayed send is ~40x more likely to be fatal. (The 7th drop began
  with "reply too short" after a 3310 us send.)
- Damaged replies: `during` LSR is 60/61 on all 84 -> the chip received clean bytes; the damage happens when we
  read them out. 12 of 3212 `during` values are `ff` = a hijacked read caught in the act. Section 2.1 stands.
- Fix 2 (send hardening) triggered 2 early-idle and 2 unequal reads in ~3,500 sends, 0 short sends: it does its
  job, but it was never the main path.
- One sample at line 26319: every chip register read 00 (`IER 00 LCR 00 MCR 00 EFR 00 ... divisor 0`) - a whole
  `log_chip` sequence corrupted or the chip reset **[unverified]**; watch for it.

### 8.1 Question 1: what is the traffic, can it be stopped?

Not known yet. What the shape allows: nine single bus bytes every 10 ms is either one 9-byte transfer (address,
register, repeated address, 6 data bytes) or one address byte that is not acknowledged plus 8 retries. Candidates
on this bus **[all unverified]**: the IR module reading ZL/ZR/C-stick (device 17) whether or not ir:rst is
initialised by us, or the HID module probing/reading another device on bus 3 (e.g. the debug pad). pwalkerHax
works on an Old 3DS XL at the first try and fails on New 3DS XL (its issue #4), so it is something only the New
3DS does - which points at the C-stick/ZL/ZR read.
Stopping it: no safe way is known from an app. It would mean stopping or patching a system module (system-wide,
input may stop working, behaviour on exit unknown) - not proposed. The v0.12 "bus sniff" (9.3) identifies the
device for the record at no risk; the fix does not depend on the answer.

### 8.2 Question 2: time our transfers into the quiet gaps?

**Yes - this is the v0.12 fix** (confidence ~70 % that damaged replies fall below 0.5 % and drops become rare).
The bus is foreign-busy 0.34 ms out of every 10.0 ms and strictly periodic. Our longest single transfer can be
kept under 0.5 ms, and a whole short-packet send (FIFO pre-loaded, air time <= 5.6 ms) fits inside one gap. The
other side waits for the busy flag like we do, so if we are never on the bus when its burst is due, the two never
interleave. Remaining risks: (a) our thread is descheduled between the check and the transfer (rare; counted in
the stats); (b) the burst jitters more than the margin (measured in the stats); (c) there is second, non-periodic
traffic that bus watch did not catch (3 x 3 s showed none).

## 9. Build brief: v0.12 (burst guard + bus sniff)

For the session that builds. One change set again: **no fast retry, no `0A` writes, no single read pass** in
this build. `pw_proto.c` untouched; PC tests must pass. `APP_VERSION` "0.12". Keep everything from v0.11
(hardened send, `during` LSR, stats, `hidShouldUseIrrst` = false). All work is in `source/ir_raw.c` plus the menu
in `source/main.c`.

### 9.1 Burst guard: the rule

No bus transfer of ours may be in progress when a foreign burst is due. Before EVERY transfer (in `chip_read`
and `chip_write_once`, the lowest level, direct-bus path only): if the transfer cannot finish at least
`GUARD_BEFORE_US` before the predicted burst start, wait for the burst to pass, then go.

Constants (file top, easy to tune): burst period measured at open (expect ~9990 us), `BURST_QUIET_US 120`
(no busy for this long = burst over), `GUARD_BEFORE_US 150`, `BURST_LATE_US 600` (give up waiting this long after
the predicted start), `MAX_XFER_DATA 16`, per-transfer time estimate `(data bytes + 3) * 25 + 30` us.

### 9.2 Burst guard: parts

a. **Sync at open** (`ir_raw_open`, after the chip setup, no I2C of ours meanwhile): watch `REG_CNT` bit 7 for
   up to 60 ms; a busy edge after >= 2 ms of quiet is a burst start; the burst ends after `BURST_QUIET_US`
   without busy. Take 4 bursts: period = mean of the 3 intervals; also log pulses per burst and burst length.
   Accept only if the 3 intervals agree within 200 us and lie in 5-20 ms; otherwise guard = off for this
   connection and say so in the log (`raw: burst guard: no periodic traffic found, off`). Log on success:
   `raw: burst guard: period P us, N pulses, length L us`.
b. **`guard_reserve(need_us)`**: if the guard is off, return. If `now` is already past `next_burst` on entry (we
   were away, e.g. between actions), first advance `next_burst` by whole periods. If
   `now + need_us + GUARD_BEFORE_US < next_burst`, return. Else spin on `REG_CNT` (no transfers): note the time
   of the first busy edge (`seen`), wait for `BURST_QUIET_US` of quiet, then `next_burst = seen + period`,
   `st_guard_waits++`, record `err = seen - predicted` into min/max. If no busy edge by
   `predicted + BURST_LATE_US`: `st_guard_missed++`, `next_burst = predicted + period`, return. After 20 misses
   in a row: one fresh sync (a) at the next call.
c. **Transfer size cap** while the guard is on: at most `MAX_XFER_DATA` data bytes per transfer.
   - FIFO pre-load: in pieces of <= 16 bytes (the transmitter is off, so pieces cost nothing on the air). The
     existing "retry only while no byte went in" rule now applies per piece: a failed piece after earlier pieces
     went in = `FCR` clear and start the load again (nothing was on the air yet).
   - Top-up (packets > 64 bytes): pieces of <= 16 bytes; v0.11 double `TXLVL` read and clock cap unchanged.
   - `RHR` reads in `raw_recv`: `n = min(level, 16, cap - total)`; loop again for the rest.
d. **Short packets (<= 64 bytes) go out inside one gap.** After the pre-load and before the `EFCR` write that
   switches the transmitter on, call `guard_reserve(len * 87 + 500)` - room for the air time, the idle check and
   `arm_receive()`. (Max 64 * 87 + 500 = 6.1 ms; the gap is ~9.6 ms.) This is the important one: the SYN reply
   and pong arrive ~0.5 ms after our packet, so the receiver must be armed without a hold-off in between.
   The LSR-idle polls and `arm_receive` that follow still call `guard_reserve` like every transfer; with the
   reservation they will not have to wait.
e. **Long packets (136 bytes, 11.8 ms on the air)** must cross one or two bursts. Nothing special: every top-up
   transfer is guarded, a hold-off is ~1.1 ms and the FIFO holds 5.5 ms. One safety: when a hold-off is about
   to start and the FIFO (by the clock) holds fewer than 24 bytes while bytes remain to be loaded, top up first
   if a <= 16-byte piece still fits before the guard time. Count `st_fifo_low` when the clock says the FIFO
   would have run empty during a hold-off (that would be a gap in the packet).
f. **Receive**: no change besides (c) and the guard inside `chip_read`. A hold-off (~1.1 ms) is shorter than
   `IDLE_GAP_US` (4 ms), so end-of-packet detection is unaffected; take the time for the idle check after the
   level read, not before a hold-off.
g. **`bus_wait` is unchanged.** Do not add waiting-on-foreign-busy logic inside a transfer.
h. **Log**: in the "raw: sent" line add the time spent in guard waits: `raw: sent 11 byte(s) in 2100 us (+ 0 us
   guard)` and make the first number exclude it, so the send-time column stays comparable with tests 1-11.
   Stats line adds: `guard waits N, missed M, burst error min/max a/b us, late K, fifo low F`, where `late` = a
   transfer of ours that ended after `next_burst` (we were descheduled or the estimate was too small).
i. **Menu**: SELECT now toggles "Burst guard: on (default) / off" (the ir:rst toggle is removed; ir:rst stays
   off). Header line: `==== ... | guard on|off | model N ====`.

### 9.3 Bus sniff (extends "Bus watch", no walker)

Reduce the three-pass watch to one pass of 3 s (ir:rst is no longer varied). Add a second pass of 200 ms that, at
every busy -> idle edge, also reads `REG_DATA` and `REG_CNT` and stores them (cap 200). Both are plain reads of
the controller's registers and write nothing. Log one line per burst: `sniff: burst at T us: dd/cc dd/cc ...`
(data byte / control byte after it). Also print jitter from the 3 s pass: min/median/max of the burst-start
interval over all bursts - this decides whether `GUARD_BEFORE_US` 150 is enough.
Reading the result (for the review, not the app): the first data byte of a burst is the device address with the
read/write bit; the same address 9 times with the ack bit clear = a device that does not answer; address /
register / address+1 / 6 data bytes = a read of 6 bytes. Look the address up on 3dbrew's I2C page for bus 3 (the
device numbers in section 8.1 are unverified).

### 9.4 Run on the console (user)

1. "Bus watch" once (no walker, ~4 s).
2. "Back up walker" twice with the guard on (default).
3. SELECT -> guard off, "Back up walker" once (same build, old behaviour - the comparison).
4. Send back `link_log.txt` and the backups.

### 9.5 What the log must show

- **Confirms**: guard on: retried reads <= 5 per backup (now 41-47), 0 reconnects, no `during ff`, no "FIFO
  load failed", no odd `IER`; the "sent" times of 11-byte packets all within +-15 us of normal; `missed` and
  `late` near 0; burst error within +-100 us. Guard off in the same sitting: the old numbers.
- **Partly** (guard needs tuning): damage falls but not to near zero AND `late` > 0 or burst error beyond
  +-150 us -> widen `GUARD_BEFORE_US` / raise the thread priority; or `missed` is large -> the traffic is not as
  regular as the watch showed.
- **Rejects**: guard on, `missed`/`late` ~0, burst error small, and retried reads + reconnects unchanged. Then the
  periodic burst is not what damages us and section 2 is wrong about the mechanism; next step would be a capture
  of what else is on the bus during a link.
- Either way the drops that remain are the input for the next build (fast retry, fix 3).

### 9.6 Housekeeping

As 7.6: build with `.\build.ps1`, tell the user every change, update `RESUME.md`, copy to the SD card only when
the user says so.

## 10. Test 12 (v0.12): what it showed

Source: `hw/test12/link_log.txt` from line 33836.

- **The burst guard is confirmed.** Guard on: two backups with 0 retried reads, 0 reconnects, 0 chunks differing,
  24.5 s each (~4,100 exchanges in all). Guard off in the same sitting: 40 retried reads, 2 reconnects, an odd
  `IER df`, 45 s. This is the section 9.5 "confirms" case, and it settles section 2: the damage and the drops came
  from colliding with the periodic traffic. With 0 damaged in ~4,100 the damage rate is below ~0.07 % (95 %
  bound); before it was 3 %.
- Guard waits 4172 / 2431 = one per burst for the whole time the chip was in use (the first figure includes the
  wait for the walker), so essentially every burst was sat out. Missed 4 (0.1 %) and 21 (0.9 %).
- The run with 21 misses is the one where the sync measured the period as 10047 us. Bus watch says the true
  period is 9991 us (single intervals 9947-10041, so jitter is about +-50 us). A period 55 us too long puts every
  prediction 55 us late and uses up a third of the 150 us safety margin. The 3-interval mean at open is too
  coarse.
- `burst error`, `late` and `fifo low` are cut off in the log (256-byte line buffer in `dbg.c`), so the margin
  actually left is not known.

### 10.1 Question 1: the sniff decoded

Every burst: `xx/52 xx/50 xx/52 ff/60 A/60 00/60 B/60 ff/60 C/61`.
- Control bytes: `52` = start + address, acknowledged, write direction; `50` = one more written byte,
  acknowledged; `52` = start again + address, acknowledged; `60` x5 and `61` = six bytes read, stop on the last.
  So it is **one transfer: select a register, then read 6 bytes, from a device that is present and answers.**
  Not a failed probe.
- The address cannot be read from the sniff: after a written byte the data register still shows the last byte
  that was *read* (each burst's first three data values equal the previous burst's last one).
- The 6 data bytes are three signed 16-bit numbers, high byte first, e.g. `fff6 0010 fff2` = -10, +16, -14, then
  (-12, 16, -14), (-15, 14, -15), (-22, 12, -14), (-27, 11, -13) ... : small values around zero on all three axes,
  drifting smoothly, sampled 100 times a second.
- **That is a 3-axis rate sensor: almost certainly the gyroscope, read by the HID module** (confidence ~80 %,
  **[unverified]** without the address). It is not the C-stick/ZL/ZR (no three 16-bit axes, and ir:rst made no
  difference) and not the accelerometer (one axis would show gravity as a large steady value). Section 8.1's
  candidates were both wrong.
- Optional proof, 4 s, no walker: run "Bus watch" once while slowly turning the console. If the three numbers
  swing by thousands and return to near zero when it lies still, it is the gyroscope.
- Stopping it: not pursued. Our app never switches the gyroscope on, so the HID module reads it on its own; the
  guard makes it harmless.

### 10.2 Question 2: tuning before relying on the guard

The result is already good; these close the known weak spots (all low risk, all in `ir_raw.c`):
1. **Period**: measure over more bursts and keep correcting it (11.2a, b).
2. **Margin**: jitter is +-50 us against a 150 us margin; raise `GUARD_BEFORE_US` to 250 (costs ~1 % of bus time).
3. **Misses**: a miss means the burst was not where we looked, so for that one burst we were unprotected. Say why
   in the log and re-sync sooner (11.2c, d).
4. **Get the missing numbers** (`burst error`, `late`, `fifo low`) - they decide whether 250 us is right.
The wider spread of 11-byte send times with the guard on is expected (16-byte pieces and the reservation before
the transmitter is switched on) and is not a problem: with the guard on, the send time no longer says anything
about collisions. Use `missed`, `late` and the retry counts instead.

### 10.3 Question 3: what is still needed

- **Fast retry (fix 3): not needed as a link fix.** There are no misses left to retry. Keep the existing
  reconnect path as the safety net. A quicker "link lost" message is a comfort item for later.
- **`0A` split writes (fix 4): shelved.** Only reopen if uploads with the guard show `fifo low` > 0, short sends
  or drops. Uploads are the one path the guard has not been tested on: a 136-byte packet is on the air for
  11.8 ms and has to cross one or two bursts.
- **Single read pass (fix 5): not needed for the link.** It would still make uploads ~40 % faster; a speed
  feature for later, not part of this work.
- **Back to features**, with the first feature runs doubling as the upload test (11.4).

## 11. Build brief: v0.13 (log fix + guard tuning, then features)

For the session that builds. Small build; `pw_proto.c` untouched; PC tests must pass; `APP_VERSION` "0.13".
No fast retry, no `0A` writes, no single read pass.

### 11.1 Log

a. Split the stats into two lines so neither can exceed the buffer: `raw: stats: ...` (everything up to
   `short sends`) and `raw: guard: waits N, missed M (early E, none O), burst error min/median/max a/b/c us,
   late K, fifo low F, period P us, resyncs R`.
b. `dbg_printf`: when `vsnprintf` reports more than fits, end the stored line with ` [cut]` so a truncated line
   can never again look complete. Raise the line buffer to 384 bytes.

### 11.2 Guard tuning (`source/ir_raw.c`)

a. **Sync at open**: watch 13 bursts (~130 ms), drop the first (it may be caught part-way: the log shows
   "8/9/9/9 pulses"), period = MEDIAN of the 11 remaining intervals. Accept if the median is 5-20 ms and at
   least 9 of the 11 intervals lie within 150 us of it.
b. **Track the period**: at every burst actually seen, `err = seen - predicted`; `period += err / 16` (signed),
   clamped to +-1 % of the period measured at open. Phase stays `next_burst = seen + period` as now.
c. **Classify a miss**: `early` = the bus was already busy at the first look when the wait began (the burst
   started before we got out of the way); `none` = nothing seen by `predicted + BURST_LATE_US`. Log the first 10
   misses of each action in full: predicted time, entry time, `need_us`, kind, and whether a send was in progress.
d. **Re-sync sooner**: after 3 misses in a row (now 20) do a short re-sync (4 bursts, median of 3, ~40 ms) - but
   only at a packet boundary: at the top of `raw_send` before the pre-load, or in `raw_recv` before the first
   byte has arrived. Never inside a send or inside a reply. Count `resyncs`.
e. `GUARD_BEFORE_US` 250. `burst error` keeps min/max and adds a median (a 64-entry ring of the last errors is
   enough).
f. SELECT keeps toggling the guard (default on) as a diagnostic.

### 11.3 Nothing else changes

Feature code is used as it is. Every action already runs through the transport, so every log from now on carries
the two stats lines.

### 11.4 Run on the console (user): first feature runs = upload test

In this order, guard on; stop and send the log if a step fails:
1. "Link test".
2. "Back up walker" once (regression check for the tuning: expect 0 retried reads again).
3. "Write test" (0x100 bytes at 0xD700, restored afterwards - 4 packets of 136 bytes).
4. "Walk start test" (uploads 0x2B80 bytes = 87 packets of 136 bytes, each read back) and later "End walk" -
   the user's call as before; these change what is on the walker.
Optional: the gyroscope proof from 10.1.

### 11.5 What the log must show

- Backup: 0-2 retried reads, 0 reconnects; `missed` below 0.2 % of waits; `late` 0; burst error within +-100 us;
  `period` staying within ~20 us of 9991.
- Uploads (the new information): every 136-byte packet acknowledged; `fifo low 0`; `short sends 0`; 0
  reconnects in part 2; retries 0-2.
- If uploads are clean: the link work is closed; sections 4's fixes 3-5 stay shelved and work returns to
  RESUME's "Next steps" (walk start/end for real, then storage).
- If uploads show `fifo low` > 0 or drops while backups stay clean: the top-up across a burst is the weak spot;
  next build = `0A` writes of <= 55 data bytes (fix 4), real walker only.
- If `late` > 0 with damage: raise the thread priority for the time a walker is connected and widen the margin.

### 11.6 Housekeeping

As 7.6.
