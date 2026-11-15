#!/usr/bin/env python3
"""make_icons.py - colour box icons (Gen 7 style) for the box view and Home -> romfs/icons/icons.bin.

Source: PokeSprite (https://github.com/msikma/pokesprite, MIT), folder pokemon-gen7x/regular (68x56 PNGs, one per
species/form, female/ = gender differences) + data/pokemon.json, copied to art_src/icons_src/ (docs/CREDITS_ICONS.md).

Every icon's pixels sit inside the window x 8..58, y 25..55 of the 68x56 canvas (50x30, checked below), so the
window keeps each icon's place (feet on the bottom line, as in the game's box). Each icon is stored cropped to its
own bounding box, as 8-bit indices into its own palette (index 0 = transparent; the icons have only alpha 0/255).

icons.bin (little endian):
  "PKIC" u16 version=1 u16 count u8 win_w=50 u8 win_h=30 u16 0
  count x { u16 species, u8 form, u8 flags (bit0 = female), u32 offset }   sorted by (species, form, flags)
  records: u8 x, y, w, h (in the 50x30 window), u8 n (colours), n x (r, g, b), w*h index bytes

Forms: the game's form number -> PokeSprite form name, for the species below (Gen 7 order; source/ui_icons.c
remaps the few Gen 4/6 numbers that differ). Any other form number -> the species' base icon.

Run (WSL): python3 tools/make_icons.py      -> romfs/icons/icons.bin + art_src/icons_preview.png
"""
import json
import os
import struct
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "art_src", "icons_src")
OUT = os.path.join(ROOT, "romfs", "icons", "icons.bin")
PREVIEW = os.path.join(ROOT, "art_src", "icons_preview.png")
WIN = (8, 25, 58, 55)  # x0, y0, x1, y1 in the 68x56 canvas
MAX_DEX = 807

ALOLA = [19, 20, 26, 27, 28, 37, 38, 50, 51, 52, 53, 74, 75, 76, 88, 89, 103, 105]
LETTERS = [chr(c) for c in range(ord("a"), ord("z") + 1)] + ["exclamation", "question"]
TYPES7 = ["normal", "fighting", "flying", "poison", "ground", "rock", "bug", "ghost", "steel", "fire", "water",
          "grass", "electric", "psychic", "ice", "dragon", "dark", "fairy"]
VIVILLON = ["icy-snow", "polar", "tundra", "continental", "garden", "elegant", "meadow", "modern", "marine",
            "archipelago", "high-plains", "sandstorm", "river", "monsoon", "savanna", "sun", "ocean", "jungle",
            "fancy", "poke-ball"]
COLOURS = ["red", "yellow", "orange", "blue", "white"]
MINIOR = ["red-meteor", "orange-meteor", "yellow-meteor", "green-meteor", "blue-meteor", "indigo-meteor",
          "violet-meteor", "red-gen7", "orange-gen7", "yellow-gen7", "green-gen7", "blue-gen7", "indigo-gen7",
          "violet-gen7"]

# species -> list of PokeSprite form names by form number ("$" = base icon)
FORMS = {
    25: ["$", "original-cap", "hoenn-cap", "sinnoh-cap", "unova-cap", "kalos-cap", "alola-cap", "partner-cap"],
    201: LETTERS,
    386: ["normal", "attack", "defense", "speed"],
    412: ["plant", "sandy", "trash"],
    413: ["plant", "sandy", "trash"],
    422: ["west", "east"],
    423: ["west", "east"],
    479: ["$", "heat", "wash", "frost", "fan", "mow"],
    487: ["altered", "origin"],
    492: ["land", "sky"],
    493: TYPES7,
    550: ["red-striped", "blue-striped"],
    585: ["spring", "summer", "autumn", "winter"],
    586: ["spring", "summer", "autumn", "winter"],
    641: ["incarnate", "therian"],
    642: ["incarnate", "therian"],
    645: ["incarnate", "therian"],
    646: ["$", "white", "black"],
    647: ["ordinary", "resolute"],
    648: ["aria", "pirouette"],
    649: ["$", "douse", "shock", "burn", "chill"],
    666: VIVILLON,
    669: COLOURS,
    670: COLOURS + ["eternal"],
    671: COLOURS,
    676: ["natural", "heart", "star", "diamond", "debutante", "matron", "dandy", "la-reine", "kabuki", "pharaoh"],
    710: ["average", "small", "large", "super"],
    711: ["average", "small", "large", "super"],
    716: ["neutral", "active"],
    718: ["50", "10", "10", "50", "complete"],
    720: ["$", "unbound"],
    741: ["baile", "pom-pom", "pau", "sensu"],
    745: ["midday", "midnight", "dusk"],
    773: TYPES7,
    774: MINIOR,
    800: ["$", "dusk", "dawn", "ultra"],
    801: ["$", "original"],
}
# Pikachu's Gen 6 (Omega Ruby / Alpha Sapphire) cosplay forms: stored as forms 32.. (ui_icons.c maps Gen 6 1..6)
PIKACHU6 = ["rock-star", "belle", "pop-star", "phd", "libre", "cosplay"]


def load_dex():
    with open(os.path.join(SRC, "data", "pokemon.json"), encoding="utf-8") as f:
        d = json.load(f)
    return {int(k): v for k, v in d.items() if int(k) <= MAX_DEX}


def path_for(entry, form, female):
    forms = entry["gen-7"]["forms"]
    if form not in forms:
        raise KeyError("%s has no form %s" % (entry["slug"]["eng"], form))
    alias = forms[form].get("is_alias_of")
    if alias:
        form = alias
    name = entry["slug"]["eng"] + ("" if form == "$" else "-" + form) + ".png"
    sub = "female" if female else ""
    return os.path.join(SRC, "pokemon-gen7x", "regular", sub, name)


def encode(path):
    im = Image.open(path).convert("RGBA")
    assert im.size == (68, 56), path
    b = im.getbbox()
    assert b and b[0] >= WIN[0] and b[1] >= WIN[1] and b[2] <= WIN[2] and b[3] <= WIN[3], (path, b)
    crop = im.crop(b)
    w, h = crop.size
    pal, idx = [], bytearray()
    for r, g, bl, a in crop.getdata():
        if a == 0:
            idx.append(0)
            continue
        assert a == 255, path
        c = (r, g, bl)
        if c not in pal:
            pal.append(c)
        idx.append(pal.index(c) + 1)
    assert len(pal) <= 255, path
    rec = bytes([b[0] - WIN[0], b[1] - WIN[1], w, h, len(pal)]) + b"".join(bytes(c) for c in pal) + bytes(idx)
    return rec


def main():
    dex = load_dex()
    entries = []  # (species, form, flags, path)
    for sp in range(1, MAX_DEX + 1):
        e = dex[sp]
        entries.append((sp, 0, 0, path_for(e, "$", False)))
        fem = os.path.join(SRC, "pokemon-gen7x", "regular", "female", e["slug"]["eng"] + ".png")
        if os.path.exists(fem):
            entries.append((sp, 0, 1, fem))
        names = FORMS.get(sp, [])
        if sp in ALOLA:
            names = ["$", "alola"]
        for n, name in enumerate(names):
            if n == 0 and name == "$":
                continue
            entries.append((sp, n, 0, path_for(e, name, False)))
        if sp == 25:
            for n, name in enumerate(PIKACHU6):
                entries.append((sp, 32 + n, 0, path_for(e, name, False)))
            for n, name in enumerate(FORMS[25]):  # Pikachu's caps have female files too; keep them
                if n:
                    p = os.path.join(SRC, "pokemon-gen7x", "regular", "female", "pikachu-%s.png" % name)
                    if os.path.exists(p):
                        entries.append((sp, n, 1, p))
    # a form 0 that is not "$" replaces the base entry
    seen = {}
    for sp, form, fl, p in entries:
        seen[(sp, form, fl)] = p
    keys = sorted(seen)
    recs, offs, cache = bytearray(), {}, {}
    head = 12 + 8 * len(keys)
    for k in keys:
        p = seen[k]
        if p not in cache:
            cache[p] = head + len(recs)
            recs += encode(p)
        offs[k] = cache[p]
    out = bytearray(b"PKIC" + struct.pack("<HHBBH", 1, len(keys), WIN[2] - WIN[0], WIN[3] - WIN[1], 0))
    for k in keys:
        out += struct.pack("<HBBI", k[0], k[1], k[2], offs[k])
    out += recs
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(out)
    print("icons.bin: %d entries, %d distinct pictures, %d bytes" % (len(keys), len(cache), len(out)))

    # preview: every base icon, 30 per row (as decoded from the file)
    base = [k for k in keys if k[1] == 0 and k[2] == 0]
    cols = 30
    sheet = Image.new("RGBA", (cols * 50, (len(base) + cols - 1) // cols * 30), (230, 240, 222, 255))
    for i, k in enumerate(base):
        o = offs[k]
        x, y, w, h, n = out[o:o + 5]
        pal = [tuple(out[o + 5 + 3 * j:o + 8 + 3 * j]) for j in range(n)]
        px = out[o + 5 + 3 * n:o + 5 + 3 * n + w * h]
        im = Image.new("RGBA", (w, h))
        im.putdata([(0, 0, 0, 0) if v == 0 else pal[v - 1] + (255,) for v in px])
        sheet.alpha_composite(im, ((i % cols) * 50 + x, (i // cols) * 30 + y))
    sheet.save(PREVIEW)
    print("preview:", PREVIEW)


if __name__ == "__main__":
    sys.exit(main())
