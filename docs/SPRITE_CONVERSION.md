# Walker sprites for Gen 5–7 Pokémon: what was learned (2026-11-14)

Everything found while converting the Gen 5–7 species to Pokéwalker sprites, including what failed and the user's
rules. Tools: `tools/walker_sprites.py` (+ helpers in `tools/`), art and results in `art_src/sprites5to7/`
(on the user's PC only; Nintendo / Smogon art is never put in the app or the repo).

## 1. The walker's format (hard limits)

- 2 bits per pixel, 4 shades: 0 white, 1 light grey, 2 dark grey, 3 black. Stored in 8-pixel-high stripes, per
  column 2 bytes (high bitplane, low bitplane), bit 0 = top pixel (`include/pw_image.h`). The stock files on
  Bulbagarden use the greys 255 / 168 / 80 / 0.
- Walking Pokémon: **small 32×24**, 2 frames, EEPROM 0x91BE, 0x180 bytes; **large 64×48**, 2 frames, 0x933E,
  0x600 bytes. The large sprite follows the small one directly, so a 32×32 small sprite does not fit (192 bytes per
  small frame = 32 × 24 × 2 bits). Route Pokémon small sprites are the same size (0x9A7E, 3 × 0x180).
- One converted Pokémon = 0x780 bytes: small frame 1, small frame 2, large frame 1, large frame 2.

## 2. Sources (the user's choices)

| Picture | Gen 5 (#494–649) | Gen 6/7 (#650–807) |
|---|---|---|
| Large 64×48 | first frame of the Black/White sprite, Bulbagarden "Spr 5b NNN.png" (gendered: "… m.png") | Smogon Gen 6 / Gen 7 Sprite Project sheets, "Front" column (BW-style fan art, spriter credits kept in `credits.csv`) |
| Small 32×24 | Gen V menu sprites, the animated ones "AniNNNMS.png" (2 frames), else "NNNMS3.png" | Sun/Moon/USUM menu sprites "NNNMS6.png" (1 frame, used for both) |

- Meltan / Melmetal (#808/809) are left out ("they aren't in gen 7"). Gen 4/5/7 *save* support comes last.
- Bulbagarden file URL = `/media/upload/<md5[0]>/<md5[0:2]>/<File_name>` (MediaWiki). A missing name returns an
  HTML page, not a 404: check the PNG signature. Fetch at 1 request per second.
- **Some Smogon Gen 7 links are sheets**, not single sprites: 96×96 tiles, front | back, and a shiny row
  (192×96 / 192×192: Pikipek, Toucannon, Grubbin, Ribombee, Bewear, Comfey, Sandygast, Palossand, Togedemaru,
  Mareanie, Wishiwashi …). Use the top-left tile.
- Some Smogon PNGs carry huge EXIF/zTXt chunks; PIL refuses them unless `PngImagePlugin.MAX_TEXT_CHUNK` is raised.

## 3. What the stock walker art does (reference: Bulbagarden "Pokéwalker Pokémon sprites", "Spr 4pw NNN.png")

- **Not scaled**: the large sprite is the DS front sprite at about 1:1. Big Pokémon run off the edges; the crop
  keeps the **face** (the user: "they should be cropped so that the face is mostly visible", stock Venusaur).
- **Mirrored**: stock walker sprites face right; DS sprites face left.
- **Small sprites are icon-like** (menu icons), not cut-down large sprites.
- Black outlines; light colours white, mid colours light grey, dark colours dark grey; black mostly for lines.
- The stock sprites were **redrawn by hand**, not converted: a pixel-aligned fit against 100+ HGSS/walker pairs
  failed (only 2 aligned). There is no exact rule to copy; shading is judged by eye. (`walker_calib_aligned.py`)
- A count-only fit on 9 uncropped pairs gave brightness cut-offs 220 / 150 / 95 (luma) — the "original" rules.

## 4. What worked

- **Full size, crop by hand-marked face**: every large sprite bigger than 64×48 (208 of 314) has its face
  position in `tools/walker_heads.txt` (read off grid sheets, `walker_head_sheets.py`); the window is centred on
  it. BW/Smogon sprites are drawn bigger than HGSS ones, so big Pokémon need this; automatic guesses ("top right",
  "most detail") kept missing the face.
- **Silhouette edge always black.**
- **Same-brightness colours must be told apart**: hue differences vanish in luma (Zoroark's red mane ≈ grey body,
  Scolipede's purple rings ≈ red body, eyes of another hue). Perceived colour difference (CIE Lab ΔE) catches
  these; between areas and between neighbouring pixels (eyes, markings).
- **Detail rule**: neighbouring pixels with a clear brightness step but the same grey → the darker one a shade
  darker (Chespin's smile).
- **Stray cut-off bits**: pieces separated from the body that touch the picture edge and are small → erase (user,
  Malamar).
- **Dark Pokémon**: body colours should usually stop at dark grey and black be kept for lines; but very dark,
  near-black palettes (Zekrom) need their colours spread by hand, and the user preferred the *lighter* spread
  (light body, dark shading, black deepest) because the dark one hid the face details.
- **Small warm accents should stand out light** on a darker body (from the user's own Hawlucha / Litleo edits:
  only the orange markings → white, only the red tuft → light grey; the rest unchanged).

## 5. What failed (do not repeat)

- **Shrinking** big sprites to fit — rejected: "why are we resizing??? We just had bad crops to fix".
- Per-sprite percentile shading (each sprite graded on its own) — washed out, outlines lost.
- Gamma-lifting dark Pokémon — merged outlines into the body (Zoroark), turned Roggenrola white.
- A cropped piece of a full sprite as the small picture — only a fragment.
- Rectangle "paint" to imitate hand-drawn diagonals — too coarse; per-colour rules + the user's choice instead.
- Lightening a whole Pokémon when the user meant one part ("litleo is too dark" really meant the red tuft).
- **Global rule changes** — they changed sprites the user had not flagged and made some worse.

## 6. The user's working rules (binding)

- **Only change what the user flagged.** Unflagged Pokémon stay byte-identical to the reviewed version
  (`out_v1`, via `WBX_FROZEN`). Check after every run (byte compare) and report the list that changed.
- Fix exactly the problem named; when the user shows their own edit, measure it and match it.
- Review rounds: the user sends screenshots + notes, "wait" until "that's all", then all at once.

## 7. How the tool is organised now

- `walker_sprites.py fetch <cache> all` → `<cache>/large/`, `<cache>/small/`, `credits.csv`.
- `walker_sprites.py convert <cache> <out>` — ALWAYS with
  `WBX_FROZEN=<cache>/out_v1 WBX_EDITS=<cache>/edits`. Writes `NNN.bin` + `preview.png` + pages of 40.
- `walker_review.txt`: per Pokémon mode — not listed = reviewed version byte for byte; `crop` = face crop +
  stray-bit erase, original colours; `colour` = newer colour rules (black for lines, ΔE rules, 205/140/80);
  `touch` = reviewed version + only the listed tweaks.
- `walker_colours.txt`: `dex RRGGBB shade` (a source colour → a grey), `dex fill x y shade` (fill an enclosed gap),
  `dex erase x0 y0 x1 y1`, `dex paint x0 y0 x1 y1 shade` (lighter pixels in the box → that shade).
- `walker_crops.txt`: `dex dx dy` (window shift, large) / `dex small dx dy`.
- `walker_heads.txt`: face positions for big Pokémon.
- Hand edits: `export <out> <edits> [dex…]` → `NNN_large.png`, `NNN_small1/2.png` (palette PNG, the 4 greys);
  a file present in `edits/` is used as is. Active now: Accelgor (617) and Scatterbug (664) large, Pyroar (668)
  all three.
- Checks: `walker_review_sheet.py` (before/after), helper scripts for colour maps and byte compares.
- Versions kept for comparison: `out_v1` (reviewed, the freeze base) … `out_v14`.

## 8. Gen 1–2 from Crystal (2026-11-15)

User: "can you create walker sprites for these crystal sprites? using both frames of their animation too?" —
source `Downloads/walker sprites/crystal` (read only): `NNN_1.png` / `NNN_2.png`, two keyframes of Crystal's
animated front sprites (56×56), #001–251 + Unown 201B–Z (276 sets), with `keyframes.json` and a review sheet.
Tool `tools/walker_crystal.py <in> <out>` → `art_src/crystal/out/NNN.large.bin` (large frame 1 + frame 2,
2 × 0x300) + preview pages.
- Crystal sprites are Game Boy Color art with 4 colours (white, light, dark, black) — 535 of 552 frames exactly.
  They map by brightness rank straight onto the walker's 4 shades; this is how the game itself shows them on a
  Game Boy (light colour = the brighter one: Squirtle's shell light / body dark, Charizard's body light / wings dark).
- 17 frames have 3–18 colours (blends, fire): the palette comes from the Pokémon's 4-colour frame; extra colours
  take the shade of the nearest palette colour, so both frames keep the same greys.
- Mirrored, full size, both frames in one shared window (no jump); fits → on the bottom edge; taller than 48
  (152 frames, max 56) → top kept, up to 8 rows of feet lost.
- No small sprites in that folder (Gen 1–2 have no per-species menu icons in Crystal).
- **Crops** (user: "now do the crops"): 80 sprites are taller than 48 rows (by 1–8); 66 have the face near the
  top (top kept); 14 move down to show the face, `tools/walker_crystal_crops.txt` ("dex first-row").
- **Small sprites**: the fan-made G/S/C "MINIDEX 001–251" minisprite sheet (user's choice;
  https://i.imgur.com/VKCdkx5.gif, credits in `art_src/minisprites/CREDITS.txt`: Chamber, Solo993, Blue
  Emerald, Lake, Neslug, Pikachu25). 509×472 GIF, its 2 frames = the 2 animation frames; 16×16 sprites at
  x = 10 + 34·col, y = 37 + 25·row, 15 per row; background (218,229,214) → transparent.
  `tools/walker_minisprites.py <gif> <out> 1x` → `art_src/minisprites/out/NNN.small.bin`. **Real size** (user's
  pick; doubled was blocky and lost the feet), centred on the bottom edge, mirrored, brightness-rank greys.
- **Complete files**: `tools/walker_gen12_combine.py` → `art_src/gen12/out/NNN.bin` (small + Crystal large,
  0x780 bytes like the Gen 5–7 files); Unown letters share the one Unown minisprite.

## 8c. Alolan forms (2026-11-15)

User: "Can you do the alolan form sprites from here" + the Smogon Gen 7 Sprite Project sheet (the same one as for
Gen 7), its "Alolan Forms" section: 18 forms (Rattata, Raticate, Raichu, Sandshrew, Sandslash, Vulpix,
Ninetales, Diglett, Dugtrio, Meowth, Persian, Geodude, Graveler, Golem, Grimer, Muk, Exeggutor, Marowak).
Tool `tools/walker_alola.py fetch <dir>` / `convert <dir> <out>` → `art_src/alola/{large,small}/`, `credits.csv`,
`art_src/alola/out/NNN_alola.bin` (0x780 like the others) + `preview.png`.
- Large: the sheet's "Front" (one picture → both large frames), the original v1 colour rules, mirrored, full size.
- Small: the Sun/Moon menu icon, Bulbagarden "NNNAMS6.png" (the Alolan form's name; "NNNMS6.png" is the
  Kanto one), `icon_frames`.
- 9 are bigger than 64×48; face points in `tools/walker_alola_heads.txt` (grid sheet `art_src/alola/heads_grid.png`).
- The Alolan forms share dex numbers with Gen 1/2, so the tool uses ONLY its own tables
  (`walker_alola_heads.txt`, `walker_alola_crops.txt`) and none of the per-dex Gen 5–7 settings.
- Review 1: Rattata ("looks a bit off": its shadow went black and merged with the outlines, eye lost) → per-colour
  lines in `tools/walker_alola_colours.txt`, lighter spread as for Zekrom; Golem ("should move down a little") →
  face point y 40 → 36. User: "both look better". In the app since v0.58: `romfs/sprites/forms/NNN_f1.bin` (`tools/sync_form_sprites.sh`).

## 9. Open for later

- Gen 1–4 sprites for users without HGSS (source not decided).
- Small sprites: 45 Sun/Moon icons are taller than 24 px and lose 1–6 rows of feet (`small_fit.csv/png`); the
  user has not chosen how to handle the 14 worst (move the crop / leave).
