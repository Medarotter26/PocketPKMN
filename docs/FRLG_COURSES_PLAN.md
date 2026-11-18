# FireRed / LeafGreen courses - plan (2026-11-17)

Owner: "After thats done can you add the frlg courses?". Nothing is built yet - this is what would change.

## Why this is bigger than ORAS

FRLG would be the **first Gen 3 game with courses**. Today a Gen 3 game (GBA Virtual Console inject) can be opened
and moved in and out of, but it has **no courses, so its Pokemon cannot go on a walk** ("has no walker routes yet"),
and the app has **no Gen 3 catches and no Gen 3 items**. Each of those is new save-writing code (owner's approval).

## Source

The owner's `Downloads\walker courses\FRLG Pokewalker Courses.xlsx` (read only): 19 courses x 6 Pokemon,
item ladders, origin game (walker's own game / FireRed / LeafGreen exclusives - 12 pairs), no Hidden Abilities
(Gen 3 has none).

| Unlock | Courses |
|---|---|
| Start | Field, Shopping District, Forest |
| Badge 1-8 | Rocky Foothills; Riverside, Clear Lake; Underground Subway; Residential Zone, Hobby Town; Beach; Freight Yard; Volcano Peak, Safari Grounds; Mountains |
| Hall of Fame | Starter's Lane |
| Hall of Fame + total watts | Island Shores 5,000, Twilight Grove 7,500, Trader's Crossing 10,000, Champion's Bazaar 20,000 |

## Steps

**Phase 1 - courses and walking**
1. `tools/routes_frlg.py` -> `romfs/routes/frlg.txt` (`game fr lg`), as ORAS: moves = the last 4 FRLG level-up
   moves; origin per Pokemon; items. Pictures: stand-ins (closest existing course picture). "Champion's Bazaar" is
   81 px (limit 80) -> "Elite Bazaar", as X/Y and Gen 7 did (question 3).
2. Which game: a GBA inject's title ID says nothing about the game, so the course file is chosen from the save
   itself (PKSM-Core tells FireRed / LeafGreen apart) - a small change to `routes_file_for` and its callers.
3. Unlocks: badges and Hall of Fame for FRLG, located with PKHeX on a FRLG save (as ORAS's were).
4. Walking a Gen 3 Pokemon: send to the walker and return to the game (Gen 3 save write, the way moves already
   write it), checked with PKHeX that the returned Pokemon stays legal.

**Phase 2 - catches** (new Gen 3 catch builder)
5. PKHeX templates for FR / LG (`catch_frlg.bin`, report `docs/CATCHES_FRLG.md`), every one legal.
6. In Gen 3 a Pokemon's PID and IVs come from the game's random-number method, and PKHeX checks that. The app would
   make them the same way from a random seed (as the game does), so they pass. Shiny catches would need a long
   search (question 1).
7. Exclusives of the other game come as a received Pokemon (Gen 3 has no "handler", so nothing else changes).

**Phase 3 - items** (new): walk items into the Gen 3 bag (Gen 3 has its own item numbers and bag layout), checked
with PKHeX.

**Tests:** course file checks, catches (PKHeX, with a FRLG save loaded), items into a FRLG save, all test scripts.

## Questions

1. Gen 3 catches never shiny (simplest, always legal) - or search for a shiny one when the walker says shiny?
2. Workbook OPEN point: Houndoom, Magcargo, Ursaring - if PKHeX finds no legal FireRed / LeafGreen origin for one,
   give it a Ruby / Sapphire / Emerald origin (received in a trade)?
3. "Champion's Bazaar" -> "Elite Bazaar" (too wide for the walker), as on X/Y and Gen 7?
4. **A FireRed or LeafGreen save is needed** to find the badge / Hall of Fame bytes and to test catches and items
   with the save loaded (hw/ has only Emerald). Do you have one on the 3DS (GBA Virtual Console)? The app's own
   backup of it is enough.
5. Build all three phases together (recommended)? With phase 1 alone, a walk's catches and items for a FRLG game
   could not be made: today the app finishes them without delivering ("can never be made", main.c), so they would
   be lost - phase 1 alone would need that changed to keep them waiting.
