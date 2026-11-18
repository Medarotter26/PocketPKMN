# PocketPKMN credits

PocketPKMN (formerly WalkerBox) is a fan-made homebrew app for the Nintendo 3DS. It is not made, endorsed or
supported by Nintendo, The Pokémon Company, Game Freak or Creatures. Pokémon and Pokéwalker are trademarks of
their owners.

This file is the source for the in-app credits screen (Settings -> Credits, `source/ui_credits.c`, v0.46);
keep the two in step.

## Licence

PocketPKMN is free software under the **GNU General Public License, version 3** (see `LICENSE`). You may copy,
change and share it under that licence; it comes with no warranty. The source code must be available to anyone
who receives the app.

## Code

- **PKHeX.Core** by Kurt (kwsch) and contributors (GPL-3.0) - its legality analysis and encounter data choose the
  legal origins of walk catches and check every catch PocketPKMN can make (PC tools only: `tools/pkhex_check_*.cs`;
  not part of the app). The species names in other languages (`romfs/i18n/*/species*.txt`, v0.64) were written
  out from PKHeX.Core. Pokémon data in the course tables also comes from PokeAPI (pokeapi.co).
- **PKSM-Core** by FlagBrew and contributors (GPL-3.0) - reading and writing Pokémon game saves, species tables
  and text. Kept in `lib/PKSM-Core` (upstream commit in `lib/PKSM-Core/UPSTREAM_COMMIT.txt`); it also carries
  memecrypto and pcg-cpp under their own licences.
- **libctru, citro3d, citro2d** and the **devkitARM** toolchain by devkitPro and contributors (zlib licence).
- **Luma3DS** csvc stub (zlib licence), used for `svcControlService` (`source/csvc.s`).

## Walker sprites (shipped inside the app)

- **Gen 1-4 (#1-493):** the Pokéwalker sprites from Pokémon HeartGold / SoulSilver (Game Freak / Nintendo),
  taken from the user's own game.
- **Gen 1/2 set (#1-251, Unown A-Z):** large sprites based on Pokémon Crystal's sprites (Game Freak / Nintendo),
  converted for the walker screen. Their small pictures are the HeartGold / SoulSilver ones (v0.45, user: "ditch our
  custom gen 2 mini sprites"); the fan-made GSC minisprites (Chamber, Solo993, Blue Emerald, Lake, Neslug,
  Pikachu25) are no longer shown.
- **Gen 5-7 (#494-807), converted to the walker's 4-shade screen:**
  - Black/White sprites (Game Freak / Nintendo), via the Bulbagarden Archives.
  - **Smogon Sprite Project** (Gen 6 / Gen 7 BW-style sprites, the 18 Alolan forms, and (v0.90) the Gen 6/7 forms
    and the female Pyroar, with their menu icons from the Bulbagarden Archives) - spriters: AMVictory, Arkeis, aXl,
    Basic Vanillite, Branflakes325, Bynine, Corson, Espeon Scientist, fishbowlsoul90, Galifia, GeoisEvil, G.E.Z.,
    HealnDeal, Larryturbo, Layell, Legitimate Username, leParagon, Mega-Pokebattlerz, MyMarshlands, N-Kin, Noscium,
    paintseagull, princessofmusic, Quanyails, RedRooster, Siiilver, Sleet, TheAetherPlayer, The Cynical Poet,
    TrainerSplash, Wobblebuns, Wyverii, Zermonious, Zerudez (the Small Pumpkaboo / Gourgeist sprites have no
    spriter listed on the sheet). Per-sprite credits: [docs/credits_smogon.csv](credits_smogon.csv).
- **Walker menu pictures and sounds (built in for "Set up a walker"):** Pokémon HeartGold / SoulSilver (Game Freak /
  Nintendo), taken from the user's own Pokéwalker.
- **Background music "Pocket Stroll"** (`romfs/sound/music.bin`, v0.73, rebuilt in v0.79): composed for PocketPKMN
  and synthesised from code (`tools/compose_song2.py`, earlier `tools/compose_music.py`; no samples or recordings),
  part of PocketPKMN under its licence (GPL-3.0).
  Button clicks and transfer sounds: PocketPKMN's own (`tools/make_sfx.py`).
- **"?" picture for route Pokémon not seen yet** (`gfx/ui_unknown.png`, v0.50): a Pokémon game graphic
  (© Nintendo / Creatures Inc. / GAME FREAK inc.).

## Box icons

- **PokeSprite** by Michiel Sikma and contributors (MIT licence for code and data), Gen 6/7 menu sprites
  (© Nintendo / Creatures Inc. / GAME FREAK inc.). Details: `docs/CREDITS_ICONS.md`.

## Other art

- App icon, HOME Menu banner, banner jingle, UI pieces, route pictures and the placeholder sprite: drawn for
  PocketPKMN (`tools/cia_art.py`, `tools/ui_atlas.py`, `tools/route_art.py`, `tools/gen_art.py`).
- Banner text font: **Inter** by Rasmus Andersson (SIL Open Font License 1.1).
- **Connect screen picture** (v0.46): rendered in Blender for PocketPKMN from two 3D models:
  - "Pokemon Heart Gold Soul Silver Pokewalker X512" by **Callum Lax**
    (https://sketchfab.com/3d-models/pokemon-heart-gold-soul-silver-pokewalker-x512-3ceece9eb4c3405ba28948529c24aea4),
    licensed under **CC BY 4.0** (https://creativecommons.org/licenses/by/4.0/); changes: flattened textures, its
    screen replaced, posed and rendered with line art.
  - "Nintendo 3DS" by **Derek** (3D Warehouse,
    https://3dwarehouse.sketchup.com/model/fc5f72380c92667d78b5af3aa27f5807/Nintendo-3DS), used under the
    3D Warehouse General Model License; only a rendered picture of it is shipped, not the model.
