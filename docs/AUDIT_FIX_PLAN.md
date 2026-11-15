# Audit fix plan (v0.50) - for the owner's OK before any code is written

Source: AUDIT_REPORT.md (2026-11-15). These change storage / save-writing / walk flows, so per the brief they are
shown first. Nothing below is written yet. Each item: what changes, what the user sees, what is NOT changed.

## B1. Stale public copy
Re-stage `~/pocketpkmn-repo` as the very last step (after every fix), then byte-compare every staged file with the
project. No code change.

## B2. A Pokémon stuck "on a walk" for good
- **New way out.** End walk with a walker that has no Pokémon, or a different walk than the app's ledger expects,
  while the app's slot is marked walking. The app then asks (red "Please check" box, L+R + A):
  "PEPPERONI's walk was lost (the walker no longer has it). Take PEPPERONI back without walk gains?"
  Yes: the walking mark is cleared, a ledger line `walk-abandoned bank=B/S reason=...` is written, and the
  Pokémon is a normal slot Pokémon again (put back into its game as usual). No: nothing changes.
- **Ledger check at walk start.** `ledger()` will return whether the line reached the card. If the walk-start
  line fails, the slot is NOT marked and the user is told (case c).
- Not changed: the normal End walk path, the gains, the walker writes.

## S1. Refused game write leaves a copy in both places
`commitGame` will say "refused before writing" or "write failed" (today both are just `false`).
- **Refused before writing** (Gen 3 no room / parity, Gen 6 checksum check): the PocketPKMN copy is taken out
  again (checked on the card) and the user sees "Nothing was moved".
- **Write failed:** "in both places" stays as it is, because the game may really have been written.

## S2. Power loss in the middle of a PocketPKMN box save
- **At start-up.** If `bank.wbk` is missing but a complete `bank.tmp` or `bank.bak` is there, the app uses it
  (tmp first, then bak). It shows "Your PocketPKMN box was recovered from <file>" and logs it.
- **While a recovery file waits.** The app never writes a new, empty box over those files.
- Not changed: the save order (tmp → rename).

## S3. Put back says "done" when the slot could not be cleared
- The app first tries the box save again (3 tries).
- **If it still fails:** that slot is blocked for the rest of this run, and the message says so plainly.
- **New check before any send or put back:** if the slot's Pokémon (its fingerprint) is already in its origin game,
  the slot copy is removed instead of being used. This also fixes copies left behind in both places by any older
  failure.

## S4. "Restore game save" can lose or double a Pokémon
Before restoring, the app reads the ledger for Pokémon moved in or out of that game after the backup was made.
- **If there are any:** it lists them (up to 4 names, then "+N more") under "Please check" and needs L+R + A.
- **A Pokémon that would be lost:** the ones put into the game after the backup. Restore is refused unless the user
  first moves them back into PocketPKMN.

## S5. SD card writes while the walker is still linked
- **Ledger lines.** Ledger lines made during a link are kept in memory and written right after the link closes.
  This is the same as Send already does.
- **Link test.** The ledger read for "is this walk ours?" moves to after the link closes.
- **Backup.** Walker tools → Backup saves its file after the link closes. The backup data is already in memory; it
  just reaches the card a moment later.
- **One kept exception.** The exception the auditor called intended stays, and is written down in docs: the
  "prefer i2c::IR" log checkpoints.

## S6. Lost reply to "start walk"
- **Before the start-walk command,** the app writes a `walk-pending` ledger line with the same data as
  `walk-start`.
- **At the next connection,** if the walker has that walk, the app finishes the job: it writes `walk-start` and marks
  the slot, and End walk then works with gains.
- **If the walker does not have it,** the pending line is dropped.
- The message after a failed send changes from "the walker is unchanged" to "the walker may have started the
  walk; connect again to check".

## S7. Cartridge vs eShop copy of the same game
The origin record gets the media (card / SD). Return prefers the same media.
- **Only the other copy is present:** the app asks before using it.
- **Old records without media:** they act as today.

## S8. Tests that need your own files
`run_storage.sh`, `run_walk.sh`, `run_image.sh` and `run_sprites.sh` print "skipped: needs your own saves /
walker backups in hw/" and pass when those files are missing. The README lists which tests need them. Nothing
changes when the files are there.

## S9. Docs contradiction
docs/PAIRING.md:245 is reworded. The HGSS menu pictures and sounds ARE shipped (your choice), taken from your own
walker; the identity area is not included. The README keeps saying the graphics are not under the GPL.

## S10. Credits pointing at files not in the repo
- **Credit files.** `art_src/sprites5to7/credits.csv` is copied to `docs/credits_smogon.csv`, and
  `art_src/icons_src/license.md` to `docs/LICENSE_pokesprite.md`. The credits then point there.
- **3D models.** The Sketchfab link for Callum Lax's model (CC BY 4.0) is added. A note on the licence the 3D
  Warehouse model by Derek was used under is added. **I need from you:** the Sketchfab page link if you have it.

## Also in v0.50 (small, from the minor list, only if you say so)
- **M16.** The HOME Menu shows the publisher as "walker app". This would become "Medarotter26" (or a name you
  choose).
- **M18.** A `.gitignore` in the project folder too, so the ROM, `hw/` and saves can never be uploaded by mistake.

## Order and checks
1. Code: B2, S1, S2, S3, S6, S7.
2. S4 and S5 (main.c only).
3. Tests and docs: S8, S9, S10.
4. Each storage change gets a PC test in tests/ (bank recovery, refused-write rollback, pending walk, media
   match). All existing tests must still pass. Gen 6 round trip: 18 walks.
5. Build v0.50. You test on the console: Link test, send, end walk, and one lost-walk case (start a walk, then
   reset the walker from HeartGold/SoulSilver or just End walk with a walker that has no Pokémon).
6. Re-stage (B1), then upload.
