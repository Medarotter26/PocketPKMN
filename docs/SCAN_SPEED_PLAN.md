# Faster "Looking for your games" — plan and risks (2026-11-17)

From the GitHub issue: "the wait times are too long, especially when it's in 'looking for games' … when you open
the app, after receiving/sending a Pokémon, and then again after the whole process."

This is a read-only analysis. **No code has been changed.** Each part needs the owner's word before it is built.

## 1. Measured times

These come from the owner's own console log (Platinum test, scratchpad `console_pt/link_log.txt`): 6 Pokémon games
and 63 installed titles.

| Part of one full scan | Time |
|---|---|
| Pokémon data + PocketPKMN box loaded, list of installed titles | 0.6 s |
| Opening the save of **every** installed title (57 of the 63 are not Pokémon games) | 4.0 s |
| Reading the whole DS cartridge save (512 KiB over the card bus) | 1.5 s |
| Searching the whole SD card for DS `.sav` files | 1.2 s |
| Game icons for Home | 0.6 s |
| **One full scan** | **≈ 9 s** |

How many full scans each action runs today:

| What the player does | Full scans | Wait |
|---|---|---|
| Open the app (Home) | 1 | 9 s |
| Send to walker: game list (`ui_pick.c:427`) | 1 more | 9 s |
| Back on Home after the send (`main.c:3593`) | 1 more | 9 s |
| End walk: catches, items, put back (`storage_deliver_catches`, `storage_deliver_items`, `storage_return_to_game`) | up to 3 | up to 27 s |
| Back on Home after End walk | 1 more | 9 s |
| Move Pokémon (`storage_browse`) and Home after it | 2 | 18 s |

So one send costs ≈ 27 s of waiting, and one walk end up to ≈ 36 s. **None of this happens while the walker is
connected.** The link is closed (`main.c:2423`) before the walk's results are delivered (`main.c:2562`).

## 2. Why the app rescans today, and what can really change

Every write today works on a save that was read moments before. That is the safety the rescans give, and any change
must keep it.

While PocketPKMN is open, a save can change in only one way: **a game card is swapped** (a 3DS cartridge for Gen 6/7,
or a DS cartridge for Gen 4).
- No other app or game can run while PocketPKMN is open. Starting one closes PocketPKMN.
- The SD card cannot be taken out while it runs.
- Virtual Console, GBA inject and TWiLight Menu `.sav` saves therefore cannot change behind the app's back.

A card swap can happen at any time: at the HOME Menu while the app is suspended, or by pulling the card while the app
is on screen. Checking whether the slot changed is cheap (about 0.01 s: is a card in? which kind? which game code?).
It **cannot** spot one card swapped for **another copy of the same game** (two Platinum cards, or two Moon cards):
the game code and title ID are the same.

### Found while doing this analysis (a gap today, not caused by any plan here)

- **DS cards are protected.** Before a write, `dsWrite` reads the card again and checks a fingerprint of the whole
  save (`storage.cpp:235-245`).
- **3DS saves are not.** `writeUserSave` (`storage.cpp:1145`) writes without checking that the save on the card is
  still the one that was read. This covers Gen 6/7 cartridges and also the SD titles.
- **How it could go wrong today:** in Send to walker, the player opens a Moon cartridge's boxes, presses HOME, swaps
  in a *second* Moon cartridge, comes back and sends a Pokémon. The second cartridge's save would be replaced by the
  first one's, minus that Pokémon. This is very rare, but it is a whole-save loss.
- **Caching would make the window bigger,** so the fix below (guard G1) comes first in every option.

## 3. The parts, and what could go wrong with each

### G1 (safety, needed before A; worth doing on its own): check the card just before every write

**DONE (built into v0.88; owner: "Yes, start with G1"). Console-checked 2026-11-17: Moon swapped for X between opening the box and moving -> refused, nothing written.** How it was built:
- `stillOnCard(f, why)` (storage.cpp) runs at the top of `commitGame` (covers deposits, withdrawals and catches), in
  Restore (after its questions, which is where the real wait is) and before each item write.
- On a mismatch:
  - a move counts as a **refused** write. The existing messages apply: "The game save could not be changed (its
    safety check refused). Nothing was moved." / "It stays in PocketPKMN." The save is read again, so the screen then
    shows the card that is really in the slot;
  - Restore shows "Not restored: … (card swapped?). Nothing was written.";
  - the debug log gets "write REFUSED before writing: … (card swapped?)".
- The comparison copy `f.onCard` is taken at the scan and in `reloadSave`, *before* PKSM-Core builds its save object
  on the same buffer. `writeAndKeep` updates it after each Gen 1/2/6/7 write, before `beginEditing`.
- `reloadSave` now takes over a new GBA container only together with a save read from it. Before this, a failed
  parse left a new container next to the old save object, which would let the check pass.
- DS game cards keep their own check inside `dsWrite`. DS `.sav` files on the SD card get the new check.
- Instead of a model, T1 became `tests/run_g1.sh`. It reads storage.cpp and fails if any game-save write
  (writeUserSave, writeAndKeep, gbavc::write, gbavc::restore) has no `stillOnCard` call before it in the same
  function: 8 writes found, all checked. Removing the check from `commitGame` gives 5 failures, and from Restore 2,
  so it catches a missing check.

The original plan for G1:

- **What it does:** just before any game-save write, the app reads that save from the card again and compares it
  byte for byte with the copy the edit started from. If they differ, nothing is written. The player sees "The save
  changed since it was read (card swapped?). Nothing was moved." and the games are scanned again.
- **Applies to:** every save kind: Game Boy VC, GBA inject (the whole container file), 3DS SD and cartridge
  `/main`, DS card and DS `.sav` file. The DS card check that exists today stays as it is.
- **Cost:** one extra read of one save per move: about 0.05–0.5 s, or 1.5 s for a DS card (which is already read
  twice per write today).
- **What could go wrong:**
  - *A false "changed".* The comparison uses the bytes the edit started from (the `before` copy that `commitGame`
    already gets), not the edited copy. Nothing else writes these saves while the app runs, so a difference really
    means another save. The worst case is a refused move with nothing written, which is the safe direction.
  - *GBA container slots.* The 3DS writes GBA saves into alternating container slots, so the compare must be on the
    whole container file (`f.gbaFile`), which `reloadSave` already keeps current after every GBA write.

### A: keep the games between Home and "Send to walker" (saves 9 s per send)

- **What changes:** Home's start-up scan stays in memory. Send to walker uses it straight away instead of scanning
  again.
- **When the kept games are thrown away** (a full scan runs instead):
  - at app start, and on "Rescan games";
  - when the card slot changed (in or out, kind, or game code);
  - after any suspend (HOME Menu) or sleep (lid). The app already gets these events; `sound.c:113` uses them.
    This also covers a swap to a second copy of the same game, as long as the swap went through the HOME Menu;
  - after Move Pokémon (it has its own scan and can change versions by hand);
  - when any reload after a write failed. The memory copy may then differ from the card, so it must never be used
    again.
- **After a successful write,** that one save is read from the card again (`reloadSave`, which already exists and
  already runs after every failed write and every Gen 3/4 write). Its badge and Hall of Fame figures are worked out
  again.

What could go wrong, and how each one is prevented:

| # | Failure | How it is prevented |
|---|---|---|
| A1 | A write uses a stale copy (card swapped) | G1 refuses it; nothing is written |
| A2 | **Finding 6 settle against a stale copy.** `settleMove` deletes the PocketPKMN copy when the counts say the game has it. On a copy that is not what is on the card (e.g. a withdraw edited in memory but not written), it could **delete the only copy = a loss** | Rule: `resolveMoves` runs **only on saves just read from the card** (full scan, or the one-game read of C), never on kept games. Test T3 checks this rule |
| A3 | GBA reading holds a system session (`PxiFS0`, `gbavc.cpp:157`) for as long as the games are kept | The kept data and the session are split: the session is closed when the pick screens close, as today (`gbavc::exit`). The GBA write opens it again by itself (`gbavc.cpp:257`) |
| A4 | Backups change. Today the first move after every scan makes a new `game_backups` file. With kept games, `backedUp` would stay set, so fewer backups | When the kept games are reused, `backedUp` is reset, and the backup is made from the bytes G1 just read (= the card now), not from `f.original` (`reloadSave` does not refresh `f.original`). Backups then stay as today: one before the first move of each Send to walker |
| A5 | Badges / Hall of Fame (course unlocks) out of date | The app's own writes never change badges or the Hall of Fame. They are worked out again anyway after each reload |
| A6 | Memory: about 3 MB kept (USUM 445 KB, DS 512 KB, GBA 263 KB ×3 copies, Game Boy 32 KB ×3…) | This is what the pick screens hold today. Free memory is measured on the console before and after; if it is tight, release before the walker link and accept one rescan after a send |
| A7 | Moves that were cut off are settled less often | A cut-off means the power went, so the next start does a full scan. A *failed* withdraw keeps its mark until the next fresh read of that game, which C does on the next Home or End walk |

### C: read only the walked Pokémon's game at End walk (saves up to ~25 s per walk end)

- **What changes:**
  - `storage_deliver_catches`, `storage_deliver_items` and `storage_return_to_game` each still read their save fresh,
    as today, but only for the walked Pokémon's own title (`entryOriginTid`).
  - That title is still looked for on both the SD card and the game card, so the "SD copy or card copy" choice
    (`storage.cpp:2802-2822`) works as today.
  - For a DS title, only the DS part runs (the card plus the SD `.sav` search, because a TWiLight save's path is only
    stored as a hash). For a 3DS title, the DS part is skipped.
- **Why this is the lowest risk:**
  - It is the same reading code with one filter added, and every write still uses a save read moments before.
  - `resolveMoves` on a filtered list settles only marks for that game. The others wait, marked, exactly as they do
    today when their save is not in the console. The finding-6 test already covers this "save away" case.
- **What could go wrong:**

| # | Failure | How it is prevented |
|---|---|---|
| C1 | The filter misses the game (e.g. a DS title ID made from a path hash) | The filter compares title IDs exactly as `entrySave` does today; for DS it keeps the whole DS search. If the game is not found, the result is today's message "their game is not in this 3DS now", and the results wait in the pending-walk file |
| C2 | A cut-off move of *another* game is not settled at End walk | It is settled at the next full scan (start-up, Rescan games), as today when that game's card is out |
| C3 | The Home screen after End walk shows old data | Home's list (names, icons, badges) does not change when the app moves Pokémon. With C, Home reads only the walked game again (to settle any mark on the walker slot and refresh it), not everything |

**Expected:** each End walk step goes from 9 s to about 0.7–1 s for a Game Boy, GBA or 3DS game (title list plus
one save), or about 2–3 s for a DS game (the card plus the SD search). Home after a send or End walk: about 1 s
instead of 9 s.

### B: remember which titles are not Pokémon games (saves ~2–3 s at start-up) — **not recommended**

The idea: keep a file of title IDs whose save was "not a Pokémon save" and skip them next time. It can **hide a real
game**:
- **A game that has never been saved.** A newly installed Pokémon VC game or GBA inject has an empty save. It reads
  as "not a Pokémon save" (the log shows several "GBA save not usable: size 00000000"). After the player saves in the
  game, PocketPKMN would keep skipping it.
- **A reused title ID.** An inject can be rebuilt with another game under the same title ID. Injects of the same ROM
  size even have the same install size, so a size check does not help.

Making B safe means reading each save anyway, which is the slow part. The saving is also only at start-up. **I
recommend not doing B.** If start-up time still matters after A and C, the first step is to measure where the 4 s
goes, title by title (a timing line in the debug log), before choosing anything.

### Not changed

- The SD `.sav` search (1.2 s) and the DS card read (1.5 s) stay at every full scan. They are how new TWiLight saves
  and swapped cards are found.
- Move Pokémon keeps its own full scan.

## 4. Expected result (6 games, as on the owner's console)

| | Today | G1 + C | G1 + C + A |
|---|---|---|---|
| Open the app | 9 s | 9 s | 9 s |
| Send to walker list | 9 s | 9 s | ≈ 0 s |
| Home after a send | 9 s | ≈ 1 s | ≈ 1 s |
| End walk (catches + items + put back) | ≈ 27 s | ≈ 2–3 s (DS ≈ 6–9 s) | same |
| Home after End walk | 9 s | ≈ 1 s | ≈ 1 s |
| Each move (G1 check) | — | +0.05–0.5 s (DS card: +1.5 s) | same |

## 5. Tests

- **T1 (PC model, like `tests/test_moves.c`):** a write never happens when the card's save differs from the copy
  the edit started from. Cases: swapped card, same-game swap, GBA container slot flipped.
- **T2 (PC model):** after every action (send, End walk, Move Pokémon, failed write, failed reload, suspend, card
  in/out), the kept games are either equal to the card or thrown away. They are never used when they might differ.
- **T3 (PC model):** `resolveMoves` / settle is only ever called on saves just read. The finding-6 model (195 runs)
  is run again with the settle fed a stale copy, to show that rule is what prevents the loss. That variant must fail
  and the real rule must pass.
- **All existing tests** (`tests/run_storage.sh` and the other 12 scripts) must pass.
- **Console checks:**
  1. A normal send and End walk with catches and items, on a Game Boy game, a GBA inject, a 3DS game and a DS card.
     Times read from the debug log.
  2. Open Send to walker, press HOME, take the DS card out, come back: the list is scanned again and the game is gone.
  3. The same with a card put back in.
  4. Only if two copies of one game card are at hand: swap them through the HOME Menu between Home and the send. A
     rescan happens. Then swap them *without* HOME (pull the card on screen): G1 refuses the write and nothing is
     moved.
  5. The debug log still shows every scan and every refusal.

## 6. Recommendation

Do this in three separate versions, each console-checked before the next:

1. **G1 alone.** It closes the same-game card swap gap that exists today, and nothing gets faster or riskier.
2. **C**, plus Home reading only the walked game after a send or End walk. This is most of the waiting the issue
   describes: about 27–36 s down to a few seconds per walk end. Every write still uses a freshly read save.
3. **A**, only after 1 and 2 are proven on the console. It saves another 9 s per send, but it is the only part where
   saves are kept in memory, so it carries the rules A1–A7.

Not B.
