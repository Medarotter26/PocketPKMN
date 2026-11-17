# Pairing (registering) a Pokewalker without HGSS

Research notes, 2026-11-14. Goal: let WalkerBox (the IR master) set up a blank or reset walker the way
HeartGold/SoulSilver does.

Confidence tags: **[game code]** = read from the HGSS disassembly (pret/pokeheartgold, `asm/overlay_112.s`,
the Pokewalker connection overlay); **[dumps]** = checked against our two dumps of the user's walker
(`hw/test1/eeprom_001.bin`, `hw/test3/eeprom_002.bin`) and a third walker's dump from reverse-pokewalker;
**[wiki]** / **[Dmitry]** / **[picowalker]** = taken from that source, not checked by us;
**[unverified]** = guess or open conflict.

Function names below (`ov112_021E....`) are pret's auto-names in that overlay, given so the findings can be
re-checked.

---

## 0. Short answers

1. **Sequence** [game code]: `20`→`22`; if the walker's paired flag is set the game sends `36` and stops
   ("already registered"); otherwise `32` (0x68-byte identity)→`34`; raw/compressed 128-byte writes of
   **0x8C50 bytes to 0x0280–0x8ECF**; then **0x28BE bytes to 0xD700**; then **0x224 bytes to 0xD480**;
   `24`→`26`; `38`→`38`. No write below 0x0280, no `F0`, no `2A` in the pairing flow.
2. **Unique ID / LCD / "nintendo" / 0x0008** are not written by pairing. "nintendo" is written by the walker
   itself when it is missing. The unique ID is 8 bytes of the DS's RTC offset + 32 bytes of DS "low entropy"
   data, made by a DS: the game generates it in the reset command `2A`/`2C`/`E0` and in `F0`/`FE` (enroll
   modes that the game contains but never calls). LCD init data and the 8 bytes at 0x0008 are only written by
   `F0` (factory). A walker after `2A` keeps LCD data and "nintendo"; whether it keeps its old unique ID or
   stores the one sent in `2A` is **[unverified]**.
3. **Reset (`2A`)** clears identity (TID/SID/name/flags/versions/last sync), watts, events, inventory/logs,
   peers. It does **not** touch the images 0x0280–0x8EFF, "nintendo", 0x0008, factory data or LCD data
   [wiki][picowalker]. Our dumps show only the trainer-name image differs between two walkers' image areas.
4. **Paired** = identity flag bit 0, set by the walker itself on `38`/`5A` (walk start), together with
   TID/SID, name, version fields copied from the last `32`/`52` identity. On later connections the game checks
   paired bit, has-Pokémon bit, **TID+SID (u32 at +0x0C)**, the u32 at +0x04 (or +0x00), the version u16 >> 2,
   and byte +0x5D. It does **not** compare unique IDs [game code].
5. **0x8CB0–0x8EFF** is identical in all three dumps and every one of the 16 descriptors' sums is correct.
   HGSS uploads 0x8CB0–0x8ECF as part of the fixed image blob; 0x8ED0–0x8EFF is never written by pairing.
   Copy it byte-for-byte; never write anything else there.
6. Also needed: the RTC value in `32` (+0x60), a route blob at 0xD700 and team data at 0xD480 (pairing always
   starts a walk), a trainer-name image. Contrast/volume/health/watts are set up by the walker itself.
7. Re-pairing an already-paired walker is possible (the walker does not refuse; only the game does), but it
   ends the current walk and makes HGSS reject the walker unless TID/SID/version are copied from the walker's
   own identity. Restoring images by raw writes is safe; restoring 0x0000–0x027F by raw writes is not
   (RAM caches overwrite it).

---

## 1. What HGSS sends when it pairs [game code]

Entry: `ov112_021E74A8(images, route, team)` sets mode 0x32 and three buffers; `ov112_021E7CC8` builds the
game's identity; `ov112_021E6A6C` sends the first packet; `ov112_021E6BF8` and `ov112_021E6F60` handle
replies; `ov112_021E6004` moves from one upload to the next; `ov112_021E5EEC` sends one write.

| # | Game sends | Walker replies | Notes |
|---|---|---|---|
| 0 | `FA` handshake | `F8` | as for every connection |
| 1 | `20`, no data, extra 0x01 | `22` + 0x68 IdentityData | game byte-swaps +0x64 (step count) |
| 2a | if walker flags bit 0 (paired) is set: `36`, no data | — | stop, error 4 ("this Pokéwalker is already registered") |
| 2b | else `32`, extra 0x01, 0x68-byte game identity (below) | `34` | |
| 3 | writes of **0x8C50 bytes to 0x0280..0x8ECF** | `04` after each | 280 full 128-byte chunks + one 0x50-byte chunk at 0x8E80 |
| 4 | writes of **0x28BE bytes to 0xD700..0xFFBD** | `04` each | route/scenario staging (same blob as "send a Pokémon"); last chunk 0x3E bytes at 0xFF80 |
| 5 | writes of **0x224 bytes to 0xD480..0xD6A3** | `04` each | team data staging; the game first copies the **walker's** unique ID (from `22` +0x10) into +0x08 |
| 6 | `24` ping | `26` | |
| 7 | `38` (walk start without UI), no data | `38` | game marks success; walker runs walk start |

Write packets (`ov112_021E5EEC`), one 128-byte chunk at a time, next chunk only after `04`:

- address aligned to 0x80: command `0x00|(addr&0x80)` (compressed) or `0x02|(addr&0x80)` (raw),
  extra = addr >> 8. The game LZ-compresses each 128-byte chunk with the NitroSDK LZ77 coder
  (`MI_CompressLZImpl`) and uses the compressed form only if it fits; a short last chunk is always sent
  raw — so the game really sends a raw `82` with only 0x50 payload bytes at 0x8E80.
- address not aligned: command `0x0A`, extra = addr >> 8, payload byte 0 = addr & 0xFF, then the data
  (fills up to the next 0x80 boundary).
- reads (`0C`) are sent with extra 0x02, payload = addr hi, addr lo, length (≤ 0x80).

Total 0xB732 bytes (the game's progress bar uses exactly this number).

### 1.1 Game identity sent in `32` (also `40`/`52`/`60`) — `ov112_021E7CC8`, `ov112_021E5E48`

| Offset | Value |
|---|---|
| +0x00 u32 | 1 |
| +0x04 u32 | 1 |
| +0x08 u16 | GAME_VERSION (HeartGold 7, SoulSilver 8) |
| +0x0A u16 | GAME_VERSION again |
| +0x0C u32 | full trainer ID (low 16 = TID, high 16 = SID) |
| +0x10 40 bytes | a 40-byte random ID kept in the save (10 × MTRandom at new game, `overlay_36.c`); zero after the game's own reset flow (it clears the save's walker block). The walker does not keep it |
| +0x38 16 bytes | 0 (event bitmap) |
| +0x48 u16[8] | trainer name, Gen-IV character codes, 0xFFFF terminated |
| +0x5A | 0 (event index) |
| +0x5B | flags, bit 0 cleared (0) |
| +0x5C..+0x5F | 0 in captures |
| +0x60 u32 BE | current time in seconds, from `RTC_ConvertDateTimeToSecond` (NitroSDK, epoch 2000-01-01). The walker sets its clock from this when it is non-zero. Other docs say "since 2001" — check on hardware **[unverified]** |
| +0x64 u32 | 0 |

The `52` capture in pokewalker-ir-linux `walk_start.log` matches this layout (1, 1, 8, 8, TID/SID, zero
+0x10 block, name, BE time).

### 1.2 What the walker does with it [wiki][picowalker][dumps]

- `32`: keeps the 0x68 bytes as "peer identity" and sets the RTC from +0x60. Nothing else.
- uploads: plain EEPROM writes.
- `38`: walk start: copy marker 0xA5, copy 0xD700→0x8F00 (0x2900 bytes) and 0xD480→0xCC00 (team data),
  clear marker, clear event log / peers / watts, then reads its IdentityData, **sets flags bits 0 and 1
  (paired, has Pokémon), clears bit 2**, copies +0x00..+0x0B (the four version words), TID/SID, trainer name,
  +0x5C/+0x5D/+0x5E from the peer identity, sets +0x5F = 2, and reliable-writes it.
  It keeps its **own** unique ID at identity +0x10. (picowalker copies the game's +0x10 block instead; our
  dumps show the real walker does not: in the user's walker identity +0x10 equals UniqueIdentityData at
  0x0083, and team data 0xCC00+0x08 holds it too.)

---

## 2. Unique ID, LCD data, "nintendo", 0x0008

- **UniqueIdentityData (0x0083/0x0183, 0x28 bytes)** = DS `OS_GetOwnerRtcOffset()` (s64 LE, 8 bytes) +
  `OS_GetLowEntropyData()` (32 bytes) [game code, `ov112_021E5E28`]. Our dumps fit: the first 8 bytes are a
  small RTC offset, and inside the entropy part is a BCD date/time with a weekday that matches (DS RTC
  format). The game generates a fresh one each time it sends `2A`/`2C` (reset), `E0`, `F0` or `FE`, and puts
  it in the payload ([game code]; the pairing modes `32/40/52/60` do **not** call this).
- **Which command stores it on the walker** — open conflict:
  - Dmitry: `F0` is "the only path" that writes UNIQUE ID DATA and LCD INIT DATA; `2A`/`2C` carry UNIQUE ID
    DATA "used to set RTC" and reply with it "now all 0xFF".
  - wiki/picowalker: `2A` keeps the stored unique ID (reset re-reads it into IdentityData).
  - The game builds a new ID for every reset and does not wait for the walker's reply, which suggests the
    walker stores the payload. **[unverified]** — see test T1.
- **LcdConfigCmds (0x00AC/0x01AC)**: byte 0 contrast/flags (0 → walker uses its ROM defaults), then a command
  stream (0xFE ends it, 0xFD = delay) [Dmitry]. Only `F0` writes it. Identical in all three dumps → factory
  data, same for every unit we have seen [dumps].
- **0x0008 (8 bytes)**: written by `F0` (its last 8 bytes) or `FE`; "never read" [Dmitry]. User's walker has
  non-zero bytes, the third dump has zeros → harmless.
- **0x0080 (2 bytes)**: factory ADC calibration, reliable format [wiki].
- **"nintendo" at 0x0000**: if missing at boot the walker runs its full init (all identity/health cleared,
  settings 0x24, last sync 2008) and writes it back [wiki][picowalker].
- `F0` in the game: payload 0x74 bytes (fresh unique ID + the rest of a static buffer; where that buffer
  is filled was not traced); `FE`: 8 bytes. We found no call in the overlay that selects these modes (only
  the reset and pairing starters are called) → probably factory/debug only **[unverified]**.

**Reliable data format** [dumps]: block + 1 checksum byte = (sum of bytes + 1) & 0xFF (checked for factory,
unique, LCD, identity, health and copy marker; Dmitry's "sum of all bytes" is off by one). Two copies,
0x100 apart.

**A walker after a normal reset still holds** "nintendo", factory data, LCD data, the images and a unique ID.
Only a walker whose EEPROM was wiped by hardware would lack them; then both copies fail and the walker fills
them with 0xFF [Dmitry]; what the LCD driver does with 0xFF flags is **[unverified]**.

---

## 3. What a reset erases

The game's reset flow (`ov112_021E7484`, called from one UI path): sends **one** `2A` (or `2C`) with a fresh
0x28-byte unique ID, does not wait for an answer, then clears the save's walker block (keeps unlocked
courses). Other modes in the code: `E0` = reset that also clears lifetime steps [wiki].

Walker side [wiki][picowalker]: `eeprom_init(clear_events, clear_steps)`:
- IdentityData: +0x00..+0x0F (versions, TID, SID) zero, trainer name zero, flags 0, event index 0,
  +0x5E 0, +0x5F 2, last sync 0; unique ID re-read into +0x10; event bitmap zeroed if clear_events.
- HealthData: watts 0, minute counter 0, steps-this-watt 0, settings forced to 0x24 bits; with clear_steps
  also total steps/days/today and last sync = 2008.
- EEPROM: inventory, event log, step history, caught list cleared (all of 0xCE80–0xDBCB with clear_steps);
  0xB800–0xBEC7 (events) if clear_events; met-peer list.
- **Not touched**: 0x0000 "nintendo", 0x0008, factory, LCD, **images 0x0280–0x8EFF**, route data 0x8F00+
  (stale but unused).

`2A` = clear events only (the one the game uses), `2C` = keep events, `E0` = clear events + lifetime data.

---

## 4. Paired state and ownership

- Paired = IdentityData (0x00ED) +0x5B bit 0. Set only by walk start (`38`/`5A`), cleared by reset.
- TID/SID, name, +0x00..+0x0B come from the last `32`/`52` identity (see 1.2).
- Game checks after `22` [game code, `ov112_021E6BF8`]:

| Mode | Checks (walker vs game) → error code |
|---|---|
| 32 pair | paired → `36`, error 4 |
| 40 return walk | not paired 5; no Pokémon 6; u32 +0x0C (TID+SID) ≠ 0xB; u32 +0x04 ≠ 8; (u16 +0x0A >> 2) ≠ 9; byte +0x5D greater 0xA → sends `44` |
| 52 send | not paired 5; has Pokémon 7; u32 +0x00 ≠ 8; (u16 +0x08 >> 2) ≠ 9; +0x0C ≠ 0xB; +0x5D greater 0xA → `56` |
| 60 gift | not paired 5; +0x5D greater 0xA → `64` |
| B0.. events | not paired 5; +0x00 8; +0x08>>2 9; +0x5D 0xA → `D8` |

So "belongs to another save" = TID+SID mismatch. No unique-ID compare in these checks. Note 7 >> 2 ≠ 8 >> 2:
the version check seems to separate HeartGold from SoulSilver **[unverified in practice]**.

---

## 5. The checksummed area 0x8CB0–0x8EFF

Decoded from all three dumps (user's two and the reverse-pokewalker one): 16 descriptors at 0x8CB0
(offset u16 LE from 0x8CF0, length u8, sum u8) cover 0x8CF0–0x8E0D contiguously; **every sum = plain byte sum
& 0xFF and matches**. The whole 0x8CB0–0x8EFF range is byte-identical in the three dumps. The wiki reads the
same table as sound data (16 sounds) — that fits the contiguous layout; Dmitry's "random garbage" checked by
0x36F2 is the same bytes. Either way:

- HGSS writes 0x8CB0–0x8ECF as part of the fixed image blob; it never writes 0x8ED0–0x8EFF.
- A master must write this range **exactly as a real walker has it** (or not at all). Dmitry's write-up says a
  change to a descriptor or covered byte may hang the walker. **Corrected 2026-11-16:** the complete matching
  decompilation of the walker firmware (github.com/lumirth/pw, CC0) has one reader of this block,
  `BeepLoadScore()` in `src/application/pw_buzzer.c`: on a wrong sum, or a last note that is not an end/repeat
  marker, it sets `g_note = 0` and returns (that sound is skipped); an over-long entry keeps the previous sound. No
  loop can hang - a bad block means **no sound, not a frozen walker** (working session's reading, confirmed by the
  advisor; the decompilation's "matching build" is that project's own claim). PocketPKMN still never changes a
  valid block (Restore rules, v0.68-v0.70).

The image area 0x0280–0x8EFF differs between two walkers only at 0x1254–0x1285 and 0x12F4–0x1327, i.e. the
trainer-name image (0x1250, 80×16) [dumps]. Everything else is the same fixed data (per game language).

---

## 6. Other things a blank walker needs

- **Images 0x0280–0x8ECF**: fixed per language, plus the trainer-name image at 0x1250 (320 bytes, 80×16, walker
  2-bitplane format). On a walker that was ever paired and then reset they are still there; we can read them
  with `0C` and need only rewrite the name image (or keep the old one).
- **Route blob 0xD700 (0x28BE)** and **team data 0xD480 (0x224)**: pairing always starts a walk. Team data:
  +0x00 u32 1, +0x04 u16 version, +0x08 walker unique ID (from `22`) [dumps: 0xCC00 in user's walker].
- **RTC**: identity +0x60 in `32`.
- **Health data / watts / contrast / volume / language**: no master action needed; the walker resets health
  (settings 0x24) itself, contrast comes from LCD data + settings. Language = language of the uploaded images
  and route text.
- To pair without leaving a Pokémon on it: pair as above, then `4E` (walk end) in a later connection
  **[unverified that a fake route blob is accepted]**.

---

## 7. Re-pairing a paired walker

- The walker does not check its paired flag on `32`/`38` (only the game refuses) [picowalker; real ROM
  **[unverified]**].
- Effects: images overwritten; walk start replaces the current route/Pokémon data and clears the step log,
  items and peers of the current walk; IdentityData gets our TID/SID/name/versions.
- If our `32` copies +0x00..+0x0F, name and +0x5C..+0x5E **from the walker's own `22` reply**, HGSS keeps
  accepting the walker (it checks only those fields). If not, the user's HGSS rejects it (error 0xB) and the
  Pokémon copy in the save stays stuck until the game's reset flow.
- Restoring images afterwards with raw `02`/`82` writes from a backup is safe (plain EEPROM data).
- Do **not** restore 0x0000–0x027F by raw writes: the walker keeps IdentityData and HealthData in RAM and
  writes them back (health every few seconds/minutes), and partial writes break the reliable checksums. Use
  the protocol (`32` + `38`) to set identity instead.

---

## 8. Recommended pairing sequence for WalkerBox

Preconditions: a full EEPROM backup of the walker (`0C` reads 0x0000–0xFFFF) saved to SD first; the image
blob available (see "data" column).

| Step | Packet / action | Data and where it comes from |
|---|---|---|
| 1 | `FA`→`F8` | — |
| 2 | `20`→`22` | keep reply as `W` (walker identity). If `W.flags & 1`: stop and ask the user (already paired; see section 7) unless the user chose "re-pair keeping owner" |
| 3 | `0C` reads of 0x0280–0x8EFF (0x8C80 bytes, 0x80 per read) | if the walker had been paired before, this is the blob; check 0x0000 = "nintendo" and that all 16 sums at 0x8CB0 are right; if the area is blank (0xFF) we need the blob from a backup file on SD (user's own walker dump or extracted from their own HGSS). The app also has these menu pictures and sounds built in (`romfs/walker/hgss_menus.bin`, taken from a walker; the identity area 0x0000–0x027F is not in it) |
| 4 | `32`, extra 0x01, 0x68 bytes | +0x00=1, +0x04=1, +0x08/+0x0A = 7 (HG) or 8 (SS) — the game the user will use; +0x0C TID/SID and +0x48 name chosen by the user (or copied from `W` for "keep owner"); +0x10 = 40 random bytes (or zeros); +0x60 = 3DS time as BE seconds since 2000-01-01; rest 0 |
| 5 | wait `34` | |
| 6 | raw writes `02`/`82` 0x0280..0x8E7F (279 packets), then 0x8E80..0x8ECF as `0A` (extra 0x8E, byte 0x80, 0x50 bytes) or a full `82` whose 0x8ED0..0x8EFF bytes are the walker's current ones | blob from step 3; only the name image 0x1250–0x138F replaced if we render a new name |
| 7 | raw writes to 0xD700..0xFFBD (0x28BE bytes) | route blob, same as our "send" flow |
| 8 | raw writes to 0xD480..0xD6A3 (0x224 bytes) | team data, +0x08 = `W` +0x10 (walker's own unique ID) |
| 9 | `24`→`26` | |
| 10 | `38`→`38` | walker runs walk start and becomes paired |
| 11 | (optional) `F4` / end | |
| 12 | verify: new connection `20`→`22`: flags = 3, TID/SID/name as sent; `0C` read of 0x8CB0–0x8EFF equals backup | |

Never send `2A`/`2C`/`E0`, `F0`, `FE`, `06` in normal use. Never write below 0x0280.

---

## 9. Open risks

- **R1** Whether the real walker stores the `2A` payload as its unique ID, and whether `2A` really answers with
  0xFF (Dmitry) — matters only if we ever offer "reset". Test T1 on a spare walker: `0C` read 0x0083, `2A`
  with a known payload, `0C` read again.
- **R2** Whether the real walker accepts `32`+`38` on an already-paired walker (picowalker does; game never
  tries).
- **R3** Short raw write (`82` with 0x50 bytes) — the game does it, so the walker accepts it; `0A` is untested
  by us.
- **R4** RTC epoch (2000 vs 2001) — compare walker clock after pairing.
- **R5** Version field (7 vs 8) — pairing as HG may block use with SS and the reverse.
- **R6** Image/route data are Nintendo data: we can only use what the user's own walker or own game gives us.
- **R7** A walker with a wiped EEPROM (0xFF LCD/unique) is not covered; would need `F0` with LCD data from a
  dump — not tested by anyone we know of.
- **R8** The user's walker currently holds a Pokémon (flags 3); any test that sends `32`+`38` or `2A` to it
  ends that walk. Test on a second walker or a picowalker first.

---

## Sources

- pret/pokeheartgold: `asm/overlay_112.s` (functions named above), `src/pokewalker.c`,
  `include/pokewalker.h`, `src/overlay_36.c` — https://github.com/pret/pokeheartgold
- Dmitry Grinberg, Pokewalker — https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker
- mamba2410/pokewalker-wiki (`ir_commands.md`, `eeprom_map.md`, `structs.md`, `behaviour/walker_init.md`,
  `walk_start.md`, `walk_end.md`) — https://github.com/mamba2410/pokewalker-wiki
- mamba2410/picowalker-core (`src/ir/actions.c`, `src/eeprom.c`) — https://github.com/mamba2410/picowalker-core
- mamba2410/pokewalker-ir-linux (`walk_start.log`, `walk_start_2.log`: real HGSS `52` captures; no pairing
  capture exists there) — https://github.com/mamba2410/pokewalker-ir-linux
- mamba2410/reverse-pokewalker (doc "Register PokeWalker", packet 0x32; `dumps/bin/64k-full-rom.bin`, a third
  walker's EEPROM) — https://github.com/mamba2410/reverse-pokewalker
- Our dumps: `hw/test1/eeprom_001.bin`, `hw/test3/eeprom_002.bin`
