# ORAS courses - plan (2026-11-17)

Owner: "lets do oras courses". Nothing is built yet - this is what would change, for the owner to approve first.

## Today

Omega Ruby (title 000400000011C400) and Alpha Sapphire (000400000011C500) saves already load (Move Pokemon, walk
sends), but `routes_file_for` (source/routes.c) gives them **no course file**, so they have no courses. X/Y
(`xy.txt`), Sun/Moon/USUM (`sm.txt`), D/P/Pt and GSC have theirs.

## Source

The owner's workbook `Downloads\walker courses\ORAS Pokewalker Courses.xlsx` (read only, as for X/Y and Gen 7):
18 courses x 6 Pokemon (A filler / B middle / C prizes), item ladders (Items sheet), unlocks (Unlocks sheet), Hidden
Ability and origin game per Pokemon, rules (exclusives, no legendaries / lore items).

| Unlock | Courses |
|---|---|
| Start | Field, Forest |
| Badge 1-8 | Dim Tunnel, Beachside, Generator Yard, Busy City, Sunbaked Ruins, Underwater + Cliffside (both badge 6), Misty Mountain, Frozen Shoal |
| Hall of Fame | Beginner's Path |
| Hall of Fame + total watts | Scavenger's Trail 5,000, Far Shores 7,500, Trader's Hill 10,000, Fossil Dig Site 15,000, Champion's Road 20,000, Starfall Crater 30,000 |

## Steps (the X/Y and Sun/Moon way)

1. **`tools/routes_oras.py`** (reusing routes_xy / routes_sm helpers) -> **`romfs/routes/oras.txt`**, `game or as`.
   Names resolved against the app's lists; moves = the last 4 ORAS level-up moves (PokeAPI data in art_src/pokeapi);
   form, origin game and HA per Pokemon from the workbook; version exclusives walkable in both games with the
   origin of the game they belong to (workbook "Exclusives" goal). The workbook is never edited.
2. **Legality:** `tools/pkhex_check` extended to OR/AS -> `romfs/routes/catch_oras.bin` + `docs/CATCHES_ORAS.md`;
   every row must pass PKHeX (as X/Y 100 %, Sun/Moon 432/432). HA as the workbook says (ORAS DexNav / hordes /
   breeding; Friend Safari ones only at Lv 30+ with an X/Y origin; transfer-only -> regular ability).
3. **App** (needs the owner's word - it adds new catch delivery):
   - routes.c: OR -> `oras`/`or`, AS -> `oras`/`as`.
   - storage.cpp: catch templates for OR/AS (`catch_oras.bin`, versions 0/1); `hallOfFameOf` for ORAS (the ORAS
     Hall of Fame offset from PKHeX SAV6AO, checked on the owner's Alpha Sapphire saves in hw/gen6); badges from
     PKSM-Core's `badges()` (checked on the same saves).
   - Catches are built as PK6, as for X/Y (walk_catch.cpp), with the ORAS met locations.
4. **Pictures:** stand-ins first (the closest X/Y or HGSS course picture, as Sun/Moon did); drawn ORAS pictures later.
5. **Tests:** `tests/check_routes_oras.py` (as check_routes_xy), test_routes.c for the two title IDs, the PKHeX report;
   all test scripts. Rebuild; ask before the SD card.

## Questions (the workbook's own "OPEN" list, plus pictures)

1. Treecko is listed twice in Beginner's Path - intended, or should one be another Pokemon?
2. Generator Yard is a split of your Busy City list - keep it as its own course?
3. Radar / dowsing watt costs: nothing to decide - the walker itself charges them, as on a real walker.
4. Item courses can be farmed daily - make the top rungs rare finds, or keep them like the X/Y ladders?
5. Linoone is in three courses, Plusle and Minun in two each - fine?
6. Course pictures: stand-ins now, drawn ones later?
