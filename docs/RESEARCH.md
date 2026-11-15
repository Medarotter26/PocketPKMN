# Research notes (2026-11-12)

Condensed from two research passes. Marked **[unverified]** where no source confirmed it or sources disagree.
Nothing here has been tested on hardware by this project yet.

Sources:
- Dmitry Grinberg, "Pokewalker hacking": https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker
- reverse-pokewalker (MIT): https://github.com/mamba2410/reverse-pokewalker
- pokewalker-wiki: https://github.com/mamba2410/pokewalker-wiki
- picowalker / picowalker-core: https://github.com/mamba2410/picowalker , https://github.com/mamba2410/picowalker-core
- HGSS IR capture: https://github.com/mamba2410/pokewalker-ir-linux (`walk_start.log`)
- pwalkerHax (GPL-3.0, 3DS homebrew that talks to a real Pokewalker): https://github.com/francesco265/pwalkerHax
- 3dbrew IR pages: https://www.3dbrew.org/wiki/IR_Services (+ `IRU:StartRecvTransfer` etc.)
- PKSM / PKSM-Core (GPL-3.0): https://github.com/FlagBrew/PKSM , https://github.com/FlagBrew/PKSM-Core
- PKHeX (GPL-3.0): https://github.com/kwsch/PKHeX
- Checkpoint: https://github.com/BernardoGiordano/Checkpoint

## 1. Is it possible?

Yes. pwalkerHax already talks to a real Pokewalker from a 3DS (old 3DS reliable; an open issue reports
New 3DS XL being unstable). It drives the IR chip (NXP SC16IS750, I2C bus 3, address 0x9A) directly through
Luma's I/O mapping, not through the `ir:u` service. Nobody has published an `ir:u` + Pokewalker result either way.

## 2. IR link

- IrDA-SIR, 115200 baud, 8N1. Every byte XORed with 0xAA. No length field: a packet ends at an idle gap (~3-4 ms).
- Packet: `cmd, extra, checksum lo, checksum hi, session[4], payload[0..128]`.
- Checksum: zero bytes 2-3; `sum = 2 + sum(byte[even] << 8) + sum(byte[odd])`; fold carries; store little-endian.
  Verified against captured HGSS packets (`fa 01 35 99 57 2e 48 03`, `20 01 73 a0 89 83 f6 ec`).
- Handshake: walker repeats the bare byte 0xFC (0x56 on the wire) about every 100 ms while its Connect screen
  is up. Master sends `FA` with its own 4 random bytes; walker answers `F8` with its own; session = XOR of both.
- `extra`: master sends 0x01, walker 0x02 (picowalker sends 0x01 and HGSS accepted it). For write commands it is
  the address high byte.
- Timing: HGSS answers 2.5-5 ms after the walker's last byte. picowalker waits at most 50 ms for the next
  command and ends a packet after 3.7 ms of silence. The real walker's own timeouts are **[unverified]**.

### 3DS `ir:u` (3dbrew)
- `IRU_SetBitRate(3)` = 115200 (Initialize leaves 48000).
- `StartSendTransfer` is non-blocking; **`WaitSendTransfer` cancels a running send** - wait for the
  send-finished event first.
- `StartRecvTransfer(size, flag=1)` = variable length: receives into shared memory as a ring, and signals the
  receive-finished event each time the line goes idle. `WaitRecvTransfer` cancels and returns the count.
  `GetRecvTransferCount` is command 0x0007 (no libctru wrapper).
- Transfer state is a single field (ready / sending / receiving), so send and receive cannot overlap.

### Commands
| Cmd | Meaning | Reply |
|---|---|---|
| 20 | identity request (walker also flushes health data, writes watts to 0xCE8A) | 22 + 0x68 bytes |
| 32 / 40 / 52 / 60 | master's identity: pairing / return / send / gift | 34 / 42 / 54 / 62 |
| 0C | EEPROM read: addr u16 BE + len u8 (max 128) | 0E + data |
| 02 / 82 | raw 128-byte EEPROM write at `extra<<8` / `+0x80` | 04 |
| 00 / 80 | same, LZ-compressed (HGSS uses these; raw is always accepted) | 04 |
| 0A | EEPROM write, any address/length (not used by the game; **not handled by picowalker**) | 04 |
| 24 | ping | 26 |
| 5A | walk start | 5A |
| 38 | walk start at first pairing | 38 |
| 4E | walk end | 50 |
| 66 | set end message to "Completed" | 68 |
| F4 | disconnect | none |
| C2 / C4 / C6 | event Pokemon / event item / special route (**not handled by picowalker**) | same |
| 10-1C | peer play between two walkers | |

## 3. What a stroll is, on the walker

The walker never holds the real Pokemon. The game keeps it (HGSS: a 136-byte PK4 in the save, general block
0xE5E0) and uploads a 16-byte summary plus pictures. All names and sprites are images, so species numbers above
493 are representable (the firmware has no species table - **[unverified]**, inferred).

PokemonSummary (16 bytes, LE): species u16, held item u16, moves u16[4], level u8,
variant u8 (low 5 bits form, 0x20 female), flags u8 (0x01 has form, 0x02 shiny), pad.

RouteInfo (0xBE bytes at 0x8F00): walking Pokemon summary @0, nickname u16[11] @0x10, friendship @0x26,
route image index @0x27 (0 field, 1 forest, 2 suburbs, 3 urban, 4 mountain, 5 cave, 6 lake, 7 beach),
route name u16[21] @0x28, 3 route Pokemon summaries @0x52, their min steps u16[3] @0x82 and percent u8[3] @0x88,
10 route items u16 @0x8C, their min steps u16[10] @0xA0 and percent u8[10] @0xB4.

Images: 2 bits per pixel, in 8-pixel-tall stripes; each column of a stripe is 2 bytes (high bitplane, then low),
bit 0 = top pixel. Size = width * (height/8) * 2.

Walk-start upload (staged at +0x4800; the walker copies it down on command 5A):

| Final | Staged | Size | Content |
|---|---|---|---|
| 0x8F00 | 0xD700 | 0xBE | RouteInfo |
| 0x8FBE | 0xD7BE | 0xC0 | route image 32x24 |
| 0x907E | 0xD87E | 0x140 | route name image 80x16 |
| 0x91BE | 0xD9BE | 0x180 | walking Pokemon small sprite 32x24, 2 frames |
| 0x933E | 0xDB3E | 0x600 | walking Pokemon large sprite 64x48, 2 frames |
| 0x993E | 0xE13E | 0x140 | walking Pokemon name image 80x16 |
| 0x9A7E | 0xE27E | 0x480 | 3 route Pokemon small sprites |
| 0x9EFE | 0xE6FE | 0x600 | large sprite of route Pokemon C |
| 0xA4FE | 0xECFE | 0x3C0 | 3 route Pokemon name images |
| 0xA8BE | 0xF0BE | 0xF00 | 10 item name images 96x16 |
| 0xB7BE | 0xFFBE | 0x42 | unknown |

Total 0x2900 bytes (82 packets) plus team data 0x224 bytes at 0xD480.

Sequences (master = us):
- **Send:** handshake, 20->22, 52+identity->54, writes 0xD700-0xFFFF, writes 0xD480.., 24->26, 5A->5A, F4.
- **Return:** handshake, 20->22, 40+identity->42, reads, 24->26, 4E->50.
- Read back on return: watts u16 BE @0xCE8A, 3 caught Pokemon summaries @0xCE8C, 3 dowsed items @0xCEBC,
  10 peer-play gift items @0xCEC8, 7 daily step counts @0xCEF0, event log @0xCF0C; total steps come from 22.

Warnings:
- Walk start copies TID, SID and trainer name from the identity we send. To keep a walker usable with its
  original HGSS save, send back the values read with 20->22.
- A walker that already carries a Pokemon from an HGSS save: starting our own walk replaces that stroll, and HGSS
  will refuse the return (it checks the species). Refuse or warn when the "has Pokemon" flag is set.
- A blank walker / blank picowalker has no UI images until the pairing flow uploads 0x0280-0x8EFF; those
  images come from the HGSS ROM.

IdentityData (0x68): unk0 u32 @0, unk1 u32 @4, unk2 u16 @8, unk3 u16 @A, TID @C, SID @E, unique id [0x28] @0x10,
event bitmap [0x10] @0x38, trainer name u16[8] @0x48, event index @0x5A, flags @0x5B (1 paired, 2 has Pokemon,
4 a Pokemon joined), protocol version @0x5C, sub-version @0x5E, last sync u32 BE @0x60 (epoch probably
2000-01-01 **[unverified]**), total steps u32 BE @0x64.

HealthData (0x18, BE, copy at 0x0156): lifetime steps, today's steps, last sync, total days u16, watts u16, ...

### 3a. Extras the stock Pokewalker supports (Dmitry, "Abnormal processes"; read 2026-11-14)

The game never uses these, the real walker's firmware does. picowalker does **not** handle C0-C6.
None tested by this project yet.

- **Special route (C6):** upload a `SpecialRoute` struct to 0xBF00, send C6. It overlays the current route with a
  new name image (80x16), a new area image (32x24, used instead of 0x8FBE), one extra Pokemon (4th) and one extra
  item (11th), each with min steps, percent chance and an optional once-only event number (1-127, 0 = none).
  The extra Pokemon carries `EventPokeExtraData` (OT name, TID/SID, met location, ability, ball) and its own
  2-frame small sprite (0x170 bytes, truncated) and name image. Lasts until walk end. The special Pokemon and item
  go into their own slots, not the 3 caught / 3 dowsed slots.
- **Event Pokemon gift (C2):** `PokemonSummary` to 0xBA44, `EventPokeExtraData` to 0xBA54, small sprite (0x180)
  to 0xBA80, name image (0x140) to 0xBC00, send C2.
- **Event item gift (C4):** 6 zero bytes + item u16 LE to 0xBD40, name image 96x16 (0x180) to 0xBD48, send C4.
- **Stamps:** B8/BA/BC/BE award heart/spade/diamond/club.
- Walk start by 5A clears the byte at 0xB800 = wipes special route/items/Pokemon; 38 does not. So a special
  route must be sent after 5A.
- **06:** write to the walker's RAM (code execution: code at 0xF956-0xFF40, pointer at 0xF7E0). Not needed for
  this project; noted as the way to change firmware behaviour without opening the walker.
- The route image at 0x8FBE is uploaded like every other image, so a custom route can have its own picture;
  `routeImageIdx` is only an index stored in the route data and the event log.

Two points where this page differs from the notes above, to settle on hardware:
- Dmitry's command table says of 32/42/52/60: "From it only RTC is used, if a flag is set", while his
  IdentityData comments say some fields are "written from game packet at walk start". Echoing the walker's own
  identity back (the plan above) is safe under both readings.
- Event log times are "seconds since jan 1 2001" per his struct; the link test currently prints last sync with
  epoch 2000. **[unverified]** - compare the printed date with the real one on the first hardware test.

## 4. picowalker

- RP2350 reimplementation in C (not an emulator); same EEPROM map and protocol; goal is to work with unmodified HGSS.
- Handles: 20, 32/40/52/60, raw and compressed 128-byte writes, 0C, 24, 66, 4E, 38, 5A, F4, peer play.
  **Does not handle** 0A, 06, C0-C6, D0-D6, stamps. So drive it exactly like HGSS does (walk-start path).
- Compressed writes only accepted for exactly 128 decompressed bytes; use raw writes.
- No custom-route feature of its own; routes are whatever the master uploads.
- Colour: yes, but done on the picowalker itself, not over IR. Per the design in picowalker-core issue #9
  (closed; reported working on Waveshare boards, not yet on the picowalker board): the walker hashes the original
  2bpp image it received and looks the hash up in a table of colour sprites stored in its own flash (RGB565 on
  hardware; shiny variant stored right after the normal one). Text and numbers stay greyscale.
  Consequences for us: (a) a sprite only turns colour if the bytes we upload are identical to the stock HGSS
  walker sprite; (b) species or art that HGSS never had (Gen 5-7, custom) stay greyscale unless the picowalker's
  sprite pack is extended with matching entries. **[unverified]** against the shipped code - read from the issue's
  design text only.
- License: drivers BSD-3; core has a custom no-selling license (README and LICENSE disagree).
- PC test targets: `pocketwalker` (real-ROM emulator, IR as a TCP byte stream) and `picowalker-sdl`.
  No 3DS emulator implements `ir:u`, so the IR path itself can only be tested on a console.

## 5. Getting Pokemon out of the games (PKSM's methods)

All of this needs custom firmware (Luma3DS).

| Source | Method |
|---|---|
| Gen 6/7 cart or digital | `ARCHIVE_USER_SAVEDATA` `{mediatype, lowid, highid}`, file `/main`; commit; delete secure value for SD titles |
| Gen 1/2 Virtual Console | same archive, file `/sav.dat` |
| Gen 3 GBA VC inject | FSPXI session, `ARCHIVE_SAVEDATA_AND_CONTENT`, two-slot file with 0x200 headers, CMAC via `FSPXI_CalcSavegameMAC`; title ID entered by the user; game must have been saved once |
| Gen 4/5 DS cart | `pxi:dev` SPI; IR carts (HGSS, BW, B2W2: game code starts with `I`) need a leading `0x00` byte at 1 MHz; save is 0x80000 bytes |
| Flashcarts | not supported by PKSM |

PKSM-Core (GPL-3.0, C++20, no build system of its own - add its sources to ours): Sav classes for every game,
PKX classes PK1-PK9, string tables, personal data (loaded from romfs at run time), block checksums and
memecrypto for Gen 7. PKSM's bank stores each Pokemon natively (generation tag + raw bytes, 0x150 per entry)
and converts only on withdrawal. Downward conversion (to PK4) exists in PKSM-Core but is lossy and partly unfinished;
PKHeX has none. Neither project models the Pokewalker beyond PKHeX's HGSS fields
(walker slot 0xE5E0, steps 0xE704, watts 0xE708, course flags 0xE70C).

PKHeX has the 27 stock courses (6 species each, levels, moves) in `encounter_walker4.pkl` and the Pokewalker
PID/IV rules in `PokewalkerRNG.cs` - reference for building routes and for generating caught Pokemon.

Licensing: PKSM-Core, PKHeX, pwalkerHax and the DS-cart SPI code are all GPL (v3 or v2+). Using any of them makes
this app GPL-3.0.
