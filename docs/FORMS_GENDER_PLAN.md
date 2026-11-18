# Walker sprites: forms and female versions - plan (2026-11-17)

Owner: "work out how to get gender differences and forms to display ... in the base pokewalker deoxys shows its
different forms ... and so does giratina". READ-ONLY study: no app code changed. Nothing is built until the owner
says so, and not while another session is editing the same files (see "Files" at the end).

## 1. What HGSS itself has (read from the owner's SoulSilver ROM)

The walker sprites come from two archives in the ROM. `tools/walker_assets.py` today takes only the first 493 of
each (one per species). Both archives hold more:

| archive | members | used today | extra |
|---|---|---|---|
| a/2/4/8, small 32x24 (index = species) | 540 | 1-493 | 494-539 = **46** |
| a/2/5/6, large 64x48 (index = species - 1) | 664 | 0-492 | 493-663 = **171** |

Every extra was matched by picture against the 493 base sprites (fewest different pixels), and the ones with a
large difference were checked by eye side by side (all confirmed). Sheets: scratchpad `hgss_small_extra.png`,
`hgss_large_extra.png`, `hgss_uncertain_pairs.png` (Nintendo art - never in the repo or a release).

### Small extras (494-539) - forms only, NO female versions

| index | what |
|---|---|
| 494 | Egg |
| 495 | Manaphy Egg |
| 496-498 | Deoxys Attack, Defense, Speed (forms 1-3) |
| 499-526 | Unown A-Z, !, ? (forms 0-27; 499 = A, same as #201) |
| 527-528 | Burmy Sandy, Trash (forms 1-2) |
| 529-530 | Wormadam Sandy, Trash (forms 1-2) |
| 531 | Shellos East (form 1) |
| 532 | Gastrodon East (form 1) |
| 533 | Giratina Origin (form 1) |
| 534 | Shaymin Sky (form 1) |
| 535-539 | Rotom Heat, Wash, Frost, Fan, Mow (forms 1-5) |

### Large extras (493-663)

| index | what |
|---|---|
| 493-585 | **93 female versions** (list below) |
| 586-589 | Deoxys forms 0-3 (Normal, Attack, Defense, Speed) |
| 590-617 | Unown forms 0-27 (A-Z, !, ?) |
| 618-621 | Castform forms 0-3 (battle only - never in a box) |
| 622-624 | Burmy forms 0-2 |
| 625-627 | Wormadam forms 0-2 |
| 628-629 | Shellos West, East |
| 630-631 | Gastrodon West, East |
| 632-633 | Cherrim Overcast, Sunshine (battle only - never in a box) |
| 634-651 | Arceus forms 0-17 (Gen 4 order, includes the "???" type) |
| 652-653 | Shaymin Land, Sky |
| 654-659 | Rotom forms 0-5 |
| 660-661 | Giratina Altered, Origin |
| 662-663 | Pichu, Spiky-eared Pichu (form 1) |

Female versions (large only), by national dex number, in archive order 493-585:
3, 12, 19, 20, 25, 26, 41, 42, 44, 45, 64, 65, 84, 85, 97, 111, 112, 118, 119, 123, 129, 130, 154, 165, 166, 178,
185, 186, 190, 194, 195, 198, 202, 203, 207, 208, 212, 214, 215, 217, 221, 224, 229, 232, 255, 256, 257, 267, 269,
272, 274, 275, 307, 308, 315, 316, 317, 322, 323, 332, 350, 369, 461, 418, 407, 465, 473, 396, 419, 464, 397, 398,
453, 454, 399, 400, 449, 450, 456, 457, 415, 403, 404, 405, 459, 460, 417, 401, 402, 443, 444, 445, 424.
Four of them (Venusaur #3, Torchic #255, Buizel #418, Floatzel #419) are pixel-identical to the male picture at
walker size; they ship anyway (the game has them) - no visible change.

So: **Deoxys and Giratina forms are there (the owner is right)**, as are Rotom, Shaymin, Burmy/Wormadam,
Shellos/Gastrodon, Unown, Arceus and Spiky-eared Pichu. Female versions exist **only as large pictures**; the small
picture of a female is the normal one (that is what HGSS has).

## 2. What the app does today

* Large + small walker pictures come from `wbx_sprite_load_for(dex, form, gen)` (source/wbx_sprites.c):
  Gen 1/2 games -> Gen 1/2 set; Gen 3 games -> Gen 3 set (v0.88); Gen 7 games with form > 0 -> romfs/sprites/forms
  (Alolan, Gen 5 forms); everything else -> the pack (one picture per species). **No gender at all.**
* Callers pass the form only for some generations: walk_poke.cpp (walk + Home) gen 2/3/7; ui_pick.c (Send to walker
  big picture) gen 2/3; routes.c / ui_send.c pass the route file's form (D/P/Pt and S/M files only).
* So a Gen 4 Giratina-Origin or Rotom-Wash shows the base picture today, also from a Gen 6/7 game (the forms folder
  has no Gen 4 species), and every female shows the male picture.
* The summary sent to the walker already carries form and female (walk_poke.cpp: variant = form | 0x20 female) - only
  the PICTURES are missing. The menu icons (ui_icons.c) already handle form and female separately; not part of this.

## 3. Proposed rules (for the owner to approve)

Only Pokemon from **Gen 4-7 games** with dex **1-493** change. Gen 1/2 and Gen 3 games keep their own sets exactly as
now (Gen 1-3 have no female versions; owner on Gen 3: "we are NOT adjusting the gen 3 sprite set"). #494-807 keep
today's pictures (separate question 5 below).

For a Pokemon from a Gen 4-7 game, dex 1-493, in this order:
1. Gen 7 and an Alolan/forms-folder file for its form -> as today (unchanged, checked first).
2. Its form has an HGSS form picture -> that large picture; the small one is HGSS's small form picture if there is
   one (Deoxys, Unown, Burmy, Wormadam, Shellos, Gastrodon, Giratina, Shaymin, Rotom), else the normal small one.
3. Else, female and HGSS has a female picture -> that large picture + the normal small one.
4. Else the pack, as today.

A missing or short file always falls back to the next step (never a placeholder where today there is a picture).

### Form numbers - where they differ between games (must be mapped, not passed through)

* **Arceus**: Gen 4 has 18 forms with "???" at 9 (Normal, Fighting, Flying, Poison, Ground, Rock, Bug, Ghost, Steel,
  ???, Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark). Gen 5 dropped "???" (17 forms); Gen 6/7 added Fairy
  as form 17. So a Gen 6/7 Arceus form f maps to HGSS form f < 9 ? f : f + 1, and Fairy (17) has no HGSS picture ->
  the normal Arceus. To be checked against PKSM-Core's / PKHeX's form lists before building. At walker size the 18
  Arceus pictures differ only in the plate colour shading; a wrong mapping would be hard to see, so the test must
  check it.
* **Pikachu** forms in Gen 6/7 (Cosplay, caps) have no HGSS picture -> normal Pikachu. **Pichu** form 1 (Spiky-eared)
  exists only in Gen 4 saves.
* **Castform / Cherrim**: a box always stores form 0, so their HGSS form pictures are never needed - not shipped.
* Deoxys 0-3, Giratina 0-1, Shaymin 0-1, Rotom 0-5, Burmy/Wormadam 0-2, Shellos/Gastrodon 0-1, Unown 0-27: the same
  numbers in Gen 4-7 saves.

## 4. How it would be built

* `tools/walker_assets.py`: a new command (`forms`) that reads the same ROM and writes the extras as complete
  0x780-byte files into two NEW romfs folders, e.g. `romfs/sprites/hgss_forms/NNN_fF.bin` (small + large of the
  form) and `romfs/sprites/hgss_female/NNN.bin` (normal small + female large). The species/form/female of each
  archive index comes from the tables in section 1 (written into the tool, with this matching run kept as a check).
  `romfs/sprites/pack/`, `gen12/`, `gen3/`, `forms/` untouched (byte-compared before/after, as for v0.88).
  About 35 form files + 93 female files, about 240 KiB.
* `source/wbx_sprites.c` / `include/wbx_sprites.h`: a new `wbx_sprite_load_for_ex(dex, form, female, gen, out)`
  with the rules above; the existing `wbx_sprite_load_for` stays and calls it with female = 0, so callers that do
  not change keep exactly today's result.
* Callers: walk_poke.cpp (the walking Pokemon + Home) and ui_pick.c (the big picture on Send to walker) pass form
  for every generation and the female flag. routes.c / ui_send.c: unchanged (route Pokemon have no gender in our
  route files; see question 4).
* Tests (tests/test_sprite_gen.c): every form file and every female file used for its dex/form/gen; Gen 1/2/3 games
  and #494+ byte-identical to today for every dex, form 0-31 and both genders; Arceus mapping per generation; a
  missing file falls back; the existing checks unchanged. All 13 scripts.

## 5. Questions for the owner

1. Female versions for Gen 4-7 games' Pokemon, as HGSS does - yes?
2. Forms as listed (Deoxys, Giratina, Rotom, Shaymin, Burmy, Wormadam, Shellos, Gastrodon, Unown, Arceus,
   Spiky-eared Pichu) for Gen 4-7 games - yes?
3. Gen 3 games' Deoxys stays Normal (owner earlier: "do not try Gen 3 forms") - confirm it stays that way.
4. Route Pokemon (the ones you can catch on a course): our course files have no gender, so they always show (and
   are sent) as the normal picture. Leave that for now? (Whether HGSS's own courses fix a gender per Pokemon is not
   checked yet.)
5. #494-807 (our own Gen 5-7 conversions): a few female files already exist (521, 592, 593 - Unfezant, Frillish, Jellicent) but
   are not shipped; and Gen 5 forms in the forms folder are used only for Gen 7 games, not Gen 6 ones. A separate
   step later?

## 6. Files - and the other session

Would change: tools/walker_assets.py, new romfs/sprites/hgss_forms/ + hgss_female/, include/wbx_sprites.h,
source/wbx_sprites.c, source/walk_poke.cpp, source/ui_pick.c, tests/test_sprite_gen.c (+ run_sprite_gen.sh), RESUME.md.
The games-scan session is editing source/storage.cpp now and its later steps (C, A) would touch ui_pick.c and main.c.
Building in the same folder while it works already mixed its unreviewed code into v0.88 once - so this waits until
that session has finished and reported, and then runs alone.
