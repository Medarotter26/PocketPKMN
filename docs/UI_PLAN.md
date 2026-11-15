# GPU UI plan (v0.32 onward) - PROPOSED 2026-11-15, waiting for the user's OK

Spec = tools/ui_mockups.py (colours, sizes, positions; mockups are 2x, the app draws at 1x).
Nothing here changes pw_proto, ir_raw, storage.cpp behaviour, walk_poke, walk_compose or gamematch.

## 1. Screens (mockup -> app)

| Screen | Mockup | Shown when | Data from |
|---|---|---|---|
| Home | mock_1_home | start, after every flow | bank walker slot (storage_walk_*), ledger's last walk-start line (date -> "Walking since 11/18/26", watts-banked -> "Watts kept", ev), trainer.txt, game list + SMDH icons |
| Game select | mock_2_game_select | Send to walker | game list (scan), SMDH icon + title name, OT/ID, media, generation |
| Not yet compatible | mock_2b_not_compatible | A on a game without routes | gameHasRoutes() (today: every game has Test Path) |
| Game box | mock_3a_game_box | after a game is chosen | the game's boxes (read only until A + confirm) |
| Walking slot | mock_3b_walking_slot | box view while a walk runs / Pokemon waits in the slot | bank slot + ledger |
| Send | mock_4_send | after the pick, before part 2 | storage_walk_prepare, walk_ev (L route / R EV - today X) |
| Connect | mock_5_connect | every IR flow (link test, backup, send part 1/2, end walk, reset, setup, restore) | static picture + progress + one status line |
| Message / confirm popups | style of 2b | questions (ask), results, errors | the flow's text |
| Walker tools | (list in home's style) | home tile | today's menu items: link test, backup, write test, restore, walk start test, reset, set up, bus watch, Storage (old, until restore is covered) |
| Sprites | (list + big LCD) | home tile | SD sprite pack (as do_sprites now) |
| Settings | (list) | home tile | trainer, burst guard on/off, game backups, debug view |

## 2. Drawing layer

- citro2d + citro3d (both in the devkitARM image: libcitro2d.a, tex3ds, mkbcfnt checked).
- New files only: `source/ui_draw.c` (primitives, 1:1 with ui_mockups.py: background, panel, band, info_box,
  button, key_badge (colourless), footer, status_bar, gauge, ev_bars, walker_lcd + LCD grid, popup),
  `source/ui_screens.c` (one draw function per screen, state struct in, no I/O inside),
  `source/ui_text.c` (font + measuring), `source/ui_icons.c` (SMDH icons).
- Background (gradient + glow + lattice) is drawn ONCE at start into a texture per screen size, then one quad per frame.
- Walker sprites (2-bit, from the SD pack) are turned into small textures when a screen opens (never per frame).
  Big sprite animates only if its two frames differ (byte compare at load), every 500 ms.
- Font switch: `UI_FONT_CONSOLE_SQUARE` in `include/ui_config.h`, 0 = 3DS system font (shared memory, no file),
  1 = Console Square Beta as romfs bcfnt (made with mkbcfnt). **Stays 0 until you OK the licence.**
- HOME icons: each title's SMDH (large 48x48 icon, already GPU tile order) read once at the game scan, kept in
  RAM. Cartridge title read the same way. No Nintendo art in the repo; the icons are read from the console.

## 3. Input and the IR-time limit

How the link runs today (checked in main.c / pw_proto.c / ir_raw.c): one thread; each exchange busy-polls the
bus with the burst guard; between exchanges main.c calls `cancel_cb` (hidScanInput, B = cancel) and, every
0x800 bytes, prints a % and swaps the console buffer. Reconnects print a prompt and wait inside pw_connect.

Plan:
1. Before `ir_raw_open`, the connect screen is fully prepared: picture, texts parsed, textures uploaded. No SD
   or romfs access after that until `session_close` (the log stays in RAM as now; dbg flush happens at close).
2. A flag `ui_ir_busy` is set from session open to close. While it is set:
   - frames are drawn ONLY from the existing between-packet points (the % spots in full_read / the write loops,
     reconnect prompts) - never from a timer and never inside an exchange;
   - at most one frame per 250 ms;
   - frame start is non-blocking (`C3D_FRAME_NONBLOCK`): if the GPU is still busy the update is skipped, the
     CPU never waits for vblank or the GPU;
   - the frame = the cached screen snapshot (one quad) + progress bar + one status line: a few quads, well under
     a millisecond of CPU.
3. B to cancel stays exactly as now (cancel_cb). No other keys are read during IR.
4. Outside IR, normal 60 fps loop with `C3D_FRAME_SYNCDRAW`.
5. Test: link log must show guard waits/misses and retries no worse than v0.31's console test (same flows).

## 4. Text from the flows (the "layer over the flows" part)

main.c (247 printf) and storage.cpp (97 printf, consoleSelect/consoleClear) print to the two PrintConsoles.
Step 1 keeps ALL of that unchanged: the two consoles are kept but point at RAM buffers instead of the screens
(consoleInit, then frameBuffer swapped to our own buffer). So every flow still works word for word.
- Unconverted screens show that buffer inside the new frame ("text view"); converted screens ignore it.
- The text view stays as the hidden DEBUG VIEW at the end (Settings -> Debug view, or hold L + SELECT anywhere
  outside IR). dbg.c / link_log.txt untouched.
- Screens are converted one at a time by replacing a flow's printf/ask/wait_key calls with ui calls - the
  flow's order of steps, checks and writes stays the same.

**Restructure needing your OK (storage.cpp):** `storage_pick_for_walk` and `pickInGame` draw their own console
screens (game list, box list, questions). For GPU screens they must hand data out instead. Proposal: new
functions next to the old ones, old ones left in place for Storage (old):
- `storage_games(list)` = the same scanVcSaves result (name, title id, OT/ID, gen, media, has routes);
- `storage_game_box(game, box, 30 entries)` = read-only display data (species, form, level, gender, egg,
  nickname, nature, ability, EVs, friendship, EXP to next level, OT/ID);
- `storage_pick_commit(game, box, slot, msg)` = exactly pickInGame's A branch: empty/egg checks ->
  readyToMove -> depositOne into the walker slot (same order, same backup on first entry to a game).
- slot-waiting state (A send again / X put back) moves to the home screen; it calls the same functions.
tests/run_storage.sh must still pass; I would add a PC test that the commit path writes the same bytes as the
old pickInGame for the same pick.

## 5. Build order (each: build, PC tests, then SD only on your OK, then your console test)

- **v0.32** citro2d start-up, drawing layer, system font, text view, **Home** with real data (walker slot,
  Walking since, games row with SMDH icons, End walk / Send main button, Walker tools / Sprites / Settings tiles,
  footer). Every flow still runs, its text in the text view. IR rule from section 3 in place.
- **v0.33** Connect screen for every IR flow (no render: a plain panel with the walker LCD + instructions;
  the render waits for your OK), message/confirm popups (ask/wait_key) in the 2b style.
- **v0.34** Game select + Not yet compatible popup (needs the storage split above).
- **v0.35** Game box (3a) + walking slot (3b) with the Pokemon details on top.
- **v0.36** Send screen (route + EV) and the End walk result screen.
- **v0.37** Walker tools, Sprites, Settings as real lists; text view becomes debug only.
- Later, each only on your word: Console Square font, connect-scene render (+ credits screen), PocketPKMN
  rename (SD migration plan first).

## 6. Questions

1. Box icons (3a/3b grid): the mockups use colour icons from art_src (your PC only). The app has only the SD
   walker pack. (a) use the walker small sprite (32x24, 2-bit) in the grid - nothing new, or (b) add a colour
   icon pack to the SD pack (needs a source you allow + credits).
2. OK for the storage.cpp split in section 4?
3. Button mapping on the send screen: mockup says L Route / R EV; today X sets the EV. Use the mockup's?
4. Debug view key: hold L + SELECT (SELECT alone stays the burst-guard toggle on Home, as now)?
