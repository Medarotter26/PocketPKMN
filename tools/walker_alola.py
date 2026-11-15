#!/usr/bin/env python3
"""Walker sprites for the 18 Alolan forms (user, 2026-11-15: "Can you do the alolan
form sprites from here" + the Smogon Gen 7 Sprite Project sheet, its "Alolan Forms"
rows). Same sources and rules as the other Gen 7 Pokemon:
  large: the sheet's "Front" column (one picture -> both large frames);
  small: the Sun/Moon menu icon from Bulbagarden, Alolan form = "NNNAMS6.png".

  walker_alola.py fetch <dir>           -> <dir>/sheet.csv, large/NNN.png, small/NNN.png, credits.csv
  walker_alola.py convert <dir> <out>   -> <out>/NNN_alola.bin (0x780: small f1, f2, large f1, f2)
                                           + preview.png
Face points for big sprites: tools/walker_alola_heads.txt ("dex x y" in the mirrored,
trimmed picture, like walker_heads.txt); window shifts: walker_alola_crops.txt
("dex dx dy" / "dex small dx dy"). Kept apart from the Gen 1/2 files with the same
numbers."""
import csv, os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws

SHEET = "https://docs.google.com/spreadsheets/d/1FMcHbSKEWZc7v2Ur4cyJjT_NhO0gqXyU9kDhsOQhlBQ/export?format=csv&gid=0"
HERE = os.path.dirname(os.path.abspath(__file__))


def rows(d):
    path = os.path.join(d, "sheet.csv")
    if not os.path.exists(path):
        open(path, "wb").write(ws.get(SHEET))
    out, on = {}, False
    for r in csv.reader(open(path, encoding="utf-8")):
        if r and r[0].strip() == "Alolan Forms":
            on = True
            continue
        if on and r and not r[0].strip().isdigit():
            break                      # "Other Forms" ends the section
        if on and len(r) > 2 and r[2].startswith("http"):
            out[int(r[0])] = {"name": r[1], "front": r[2], "spriter": r[6] if len(r) > 6 else ""}
    return out


def fetch(d):
    for sub in ("large", "small"):
        os.makedirs(os.path.join(d, sub), exist_ok=True)
    cr = os.path.join(d, "credits.csv")
    for num, r in rows(d).items():
        got = []
        for sub, urls in (("large", [r["front"]]), ("small", [ws.bulba_url("%03dAMS6.png" % num)])):
            out = os.path.join(d, sub, "%03d.png" % num)
            if os.path.exists(out):
                continue
            url = ws.fetch_one(urls, out)
            got.append("%s %s" % (sub, "ok" if url else "MISSING"))
            if url:
                with open(cr, "a", encoding="utf-8") as c:
                    c.write("%d alola,%s,%s,%s\n" % (num, sub, url, (r["spriter"] if sub == "large" else "Game Freak").replace(",", ";")))
        print("%03d %s: %s" % (num, r["name"], ", ".join(got) or "cached"))


def table(name, n):
    p = os.path.join(HERE, name)
    out = {}
    if os.path.exists(p):
        for line in open(p):
            q = line.split("#")[0].split()
            if len(q) == n:
                out.setdefault(int(q[0]), tuple(int(v) for v in q[1:]))
            elif len(q) == n + 1 and q[1] == "small":
                out.setdefault((int(q[0]), "small"), tuple(int(v) for v in q[2:]))
    return out


def convert(d, out):
    from PIL import Image, ImageDraw
    heads = table("walker_alola_heads.txt", 3)
    crops = table("walker_alola_crops.txt", 3)
    # The Alolan forms share dex numbers with Gen 1/2 and must not pick up Gen 5-7
    # per-dex settings: use only the Alolan tables.
    ws.head_point = lambda num: heads.get(num)
    ws.crop_override = lambda num, w=64: crops.get(num if w == 64 else (num, "small"), (0, 0))
    ws.review_mode = lambda num: None
    colours = {}
    p = os.path.join(HERE, "walker_alola_colours.txt")
    if os.path.exists(p):
        for line in open(p):
            q = line.split("#")[0].split()
            if len(q) == 3:
                h = q[1]
                colours.setdefault(int(q[0]), {})[(int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))] = int(q[2])
    ws.colour_overrides = lambda num: colours.get(num, {})
    ws.small_overrides = lambda num: {}
    for f in ("paints", "erases"):
        setattr(ws, f, lambda num, w: [])
    ws.fills = lambda num: []
    os.makedirs(out, exist_ok=True)
    done = []
    for num in sorted(rows(d)):
        src = ws.first_frame(os.path.join(d, "large", "%03d.png" % num))
        big = ws.to_walker_frames([src, src], 64, 48, num)
        ic = ws.all_frames(os.path.join(d, "small", "%03d.png" % num))
        if len(ic) == 1:
            ic = ic * 2
        small = ws.icon_frames(ic[:2], num)
        data = ws.encode(small[0], 32, 24) + ws.encode(small[1], 32, 24) + ws.encode(big[0], 64, 48) + ws.encode(big[1], 64, 48)
        assert len(data) == 0x780
        open(os.path.join(out, "%03d_alola.bin" % num), "wb").write(data)
        done.append((num, src, big, small))
    S, gap = 3, 10
    W = gap * 3 + 96 * 2 + 64 * S + 32 * S * 2 + gap * 3
    sheet = Image.new("RGB", (W, 22 + len(done) * (48 * S + gap)), "white")
    dr = ImageDraw.Draw(sheet)
    dr.text((gap, 5), "source (Smogon)     LARGE (both frames the same)      SMALL frame 1 / frame 2", fill="black")
    for i, (num, src, big, small) in enumerate(done):
        y = 22 + i * (48 * S + gap)
        bg = Image.new("RGB", src.size, "white")
        bg.paste(src, mask=src.getchannel("A"))
        sheet.paste(bg.resize((96 * 2 * src.width // max(src.size), 96 * 2 * src.height // max(src.size)), 0), (gap, y - 20 if i else y))
        dr.text((gap, y + 48 * S - 12), "#%d Alola" % num, fill="black")
        x = gap * 2 + 192
        sheet.paste(ws.preview(big[0], S).convert("RGB"), (x, y)); x += 64 * S + gap
        for f in small:
            sheet.paste(ws.preview(f, S).convert("RGB"), (x, y)); x += 32 * S + gap
    sheet.save(os.path.join(out, "preview.png"))
    print("converted %d" % len(done))


if __name__ == "__main__":
    if sys.argv[1] == "fetch":
        fetch(sys.argv[2])
    else:
        convert(sys.argv[2], sys.argv[3])
