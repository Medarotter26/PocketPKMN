# Rename WalkerBox -> PocketPKMN: migration plan (PROPOSED 2026-11-15; rename approved by the user)

## What moves
| Now | After |
|---|---|
| `sdmc:/3ds/walkerbox.3dsx` | `sdmc:/3ds/pocketpkmn.3dsx` |
| `sdmc:/3ds/walkerbox/` (trainer.txt, ledger.txt, bank.wbk/.bak/.tmp, link_log.txt, eeprom_NNN.bin, game_backups/, versions.txt, sprites/) | `sdmc:/3ds/pocketpkmn/` (same files, unchanged) |
| app title / header "WalkerBox", HOME menu name | "PocketPKMN" |

Unchanged on purpose (internal formats; changing them buys nothing and risks data): bank magic `WBXBANK1`, origin
record `WBO1`, the ledger's existing lines, eeprom file names, the C names (wbx_*), the repo folder.

## How
1. One header `include/app_paths.h` with `APP_DIR "sdmc:/3ds/pocketpkmn"`; every path (main.c, storage.cpp,
   wbx_trainer.h, wbx_sprites.h) uses it. Today five files spell the path out.
2. Migration at start-up, before the trainer is loaded, never during IR:
   - only `walkerbox/` exists -> one directory RENAME (`rename()` on the same SD card: the folder entry changes,
     no file is copied or deleted), then a ledger line `moved /3ds/walkerbox -> /3ds/pocketpkmn`, then a
     screen saying so;
   - rename fails -> error screen, the app stops (never runs with the data split in two places);
   - both folders exist -> use `pocketpkmn/`, touch nothing, warn on Home that the old folder is still there;
   - neither -> first start as today.
3. Old app: if `walkerbox.3dsx` were started after the move, it would make a new empty trainer + bank. So when
   copying to the SD card (on your OK) the old `walkerbox.3dsx` is removed in the same step (a copy kept on the PC).
4. All screen text "WalkerBox" -> "PocketPKMN" (header, help texts, messages). Makefile TARGET pocketpkmn,
   APP_TITLE PocketPKMN.
5. Tests: PC test of the migration function on temp folders (the four cases above); then Azahar with its own
   virtual SD holding a `walkerbox/` folder; then your console.
