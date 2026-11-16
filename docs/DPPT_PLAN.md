# Diamond / Pearl / Platinum - plan (prepared 2026-11-15, not started)

User: "Im ready to prepare implementing DPPt" + "I'd like us to ensure that saves from twilight menu can be read and
show up different from cartridges". Source of truth for the courses: `walker courses/DPt Pokewalker Courses.xlsx`
(read only; 17 courses, 102 Pokemon, 170 items; no Hidden Abilities in Gen 4).

## 1. Courses (same pipeline as Gen 2 / X/Y / Gen 7)

- `tools/routes_dppt.py` -> `romfs/routes/dppt.txt`, versions `d` / `p` / `pt`; unlocks `badges=N`, `hof`,
  `hof watts=N` (the app already understands them); group A always 0 steps; `origin=` from the sheet's
  "Origin game to set" (walker's own game / Diamond / Pearl / "Diamond or Platinum" / "Pearl or Platinum").
- Course names must fit the walker (80 px): "Champion's Bazaar" is 81 px -> "Elite Bazaar" as in the other games;
  the others to be measured.
- Pictures: shared names reuse theirs (Starter's Lane, Trader's Crossing, Elite Bazaar; Abandoned Factory could use
  X/Y's Rusted Works); the rest need new pictures (places only, no creatures).

## 2. Legal catches

- PKHeX templates for D, P and Pt (`tools/pkhex_check_Templates.cs` with PK4), regular abilities (Gen 4 has no HA),
  the sheet's origin game; dual-slot encounters (a GBA game in slot 2) count as normal wild origins in PKHeX.
- Trade evolutions (Magmortar, Steelix, Electivire, Trader's Crossing) get a traded-once history as in the other
  games.
- Sheet check: see section 6.

## 3. Reading and writing the saves - two kinds, shown apart

- **Game card (DS cartridge in the 3DS slot):** read / write the save chip over SPI (PKSM's DS-card code, GPL like
  the rest). Label: "Game card".
- **TWiLight Menu++ (DS game files on the SD card):** the save is a plain `.sav` file (512 KiB for D/P/Pt).
  User: "I don't know how TWiLight does it, it could be different for every user? PKSM just finds it and reads it.
  See what they do". PKSM's source (FlagBrew/PKSM, `3ds/source/titles/loader.cpp`, `Configuration.cpp`,
  `ExtraSavesScreen.cpp`): it auto-scans only `/3ds/Checkpoint/saves` (+ its own backups if "show backups" is on),
  matching by file name (`POKEMON D.sav` ...); every other save - TWiLight's included - is added BY HAND in its
  "Extra Saves" list, where the user also picks the game. Nothing in PKSM knows TWiLight's folders.
  PocketPKMN does better, independent of anyone's layout: the game scan walks the SD card for `.sav` files of a DS
  Pokemon save's size (512 KiB; allow emulator / dumper bytes appended), skipping `/Nintendo 3DS`, `/3ds/Checkpoint`,
  `/3ds/PKSM/backups`, PocketPKMN's own `game_backups`, and lets PKSM-Core identify the game from the contents. Such
  a save is listed as "SD card (TWiLight Menu)" with its file name - apart from a game card, even for the same game.
  Writing = that `.sav` file (keep any appended bytes; temp file + rename; backup first). The game files (`.nds`)
  are never opened. The scan runs with the games scan (never during a walker link).
  User's SD card (2026-11-15): TWiLight Menu++ installed (`/_nds/TWiLightMenu`) but no DS game saves on the card -
  the DS games are cartridges; PKSM's own cartridge backups are in `/3ds/PKSM/backups` (Platinum `CPUE`, Pearl JP,
  SoulSilver, White). Test the SD path with a COPY of a Platinum backup in a test folder (same format as TWiLight's).
- Both: a backup before the first change, as for every game.

## 4. Changing a Gen 4 save only the way the game does

- D/P/Pt saves hold two copies of the game (two halves); each has a "general" and a "storage" (boxes) part with a
  footer (save counter, size, CRC). The game writes the other half each time it saves.
- As for Moon: a before / after pair made BY THE GAME (move one Pokemon to another box slot, save), from both a
  game card and a TWiLight save if possible -> the rule (which half, which counters / checksums change) and a PC test
  that reproduces the game's own result byte for byte.

## 5. Progress, items, the rest

- Badges: PKSM-Core. Hall of Fame: to be found (PKHeX), checked on the user's save.
- Items: the bag (D/P/Pt have no item PC), like X/Y and Gen 7.
- Watts per save, sounds, sprites (Gen 4 = the HGSS walker sprites already in the app): nothing new.

## 6. Decisions (user, 2026-11-15)

- "Lakefront is fine" (the sheet's open point: keep the name).
- Games on hand: **Diamond and Platinum, as cartridges** (no TWiLight saves). Before / after pairs: Checkpoint
  already backs up the user's DS cartridges (folders such as `CPUE POKEMON PL` in `/3ds/Checkpoint/saves`).
- "HGSS will have new courses just for this app" - a later step (its own workbook), after D/P/Pt.

## 7. Sheet legality check (done 2026-11-15)

`tools/pkhex_check_Gen7Check.cs` (now also D / P / Pt with PK4; env `G7VERS=D,P,Pt`), every row in every game its
origin column allows, regular ability: **102 of 102 have a legal origin** - dual-slot Pokemon, version exclusives
and trade evolutions included. (Froslass only after the checker made female-only species female: a female Snorunt
from Route 216, Lv 33-34, + Dawn Stone.) Gen 4 note: PKHeX's Gen 4 generator never gives up looking for a hidden
ability, so the checker skips HA for Gen 4. Sheet items: no Plates, Mega Stones or Z-Crystals; no special Pokemon.
