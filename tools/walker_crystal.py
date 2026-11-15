#!/usr/bin/env python3
"""Walker LARGE sprites (64x48, 2 frames) from Crystal's animated front sprites
(user, 2026-11-15: "can you create walker sprites for these crystal sprites? using
both frames of their animation too?"). Input: a folder of NNN_1.png / NNN_2.png
(56x56, two keyframes per Pokemon, Unown as 201B..201Z), e.g.
Downloads/walker sprites/crystal (read only).

Conversion:
- Crystal sprites are Game Boy Color art: 4 colours per sprite = white, light,
  dark, black. Each colour maps by its brightness rank straight to the walker's 4
  shades (white 0, the lighter colour 1, the darker colour 2, black 3). Frames with
  more colours (a few) are grouped into those 4 levels by brightness.
- Mirrored (walker sprites face right), full size, no scaling.
- Both frames share one window, so the animation does not jump: centred; a
  Pokemon that fits stands on the bottom edge; a taller one (up to 56 rows) keeps
  the top (the face).
Output: <out>/NNN.large.bin = large frame 1 + large frame 2 (2 x 0x300 bytes, walker
2bpp, include/pw_image.h), and preview pages.
Usage: walker_crystal.py <in_dir> <out_dir>"""
import os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def palette_levels(frames):
    """Colour -> shade 0..3 for one Pokemon (both frames together). The palette is
    taken from a frame with the normal 4 colours; colours only found in an odd frame
    (17 frames have 3-18: blends, fire effects) get the shade of the nearest palette
    colour, so both frames keep the game's greys."""
    from collections import Counter
    per = []
    for f in frames:
        px = f.load()
        per.append(Counter(px[x, y][:3] for y in range(f.height) for x in range(f.width) if px[x, y][3] >= 128))
    normal = [c for c in per if len(c) == 4]
    if normal and any(len(c) != 4 for c in per):
        base = palette_levels_from(normal[0])
        out = dict(base)
        for c in per:
            for col in c:
                if col not in out:
                    near = min(base, key=lambda b: sum((p - q) ** 2 for p, q in zip(b, col)))
                    out[col] = base[near]
        return out
    cnt = Counter()
    for c in per:
        cnt.update(c)
    return palette_levels_from(cnt)


def palette_levels_from(cnt):
    cols = sorted(cnt, key=lambda c: -ws.lum_of(c))          # light -> dark
    if len(cols) <= 4:
        # GBC: white, light, dark, black. Map by rank, but a colour that is not
        # white keeps out of 0 and a colour that is not black keeps out of 3.
        levels = {}
        rest = [c for c in cols if ws.lum_of(c) < 245 and ws.lum_of(c) > 20]
        for c in cols:
            if ws.lum_of(c) >= 245:
                levels[c] = 0
            elif ws.lum_of(c) <= 20:
                levels[c] = 3
        if len(rest) == 1:
            levels[rest[0]] = 1 if ws.lum_of(rest[0]) >= 128 else 2
        elif len(rest) >= 2:
            levels[rest[0]] = 1
            for c in rest[1:]:
                levels[c] = 2
        return levels
    # More colours: white / black as above, the rest split into light / dark at the
    # brightness that halves their pixels.
    levels = {}
    rest = []
    for c in cols:
        l = ws.lum_of(c)
        if l >= 245:
            levels[c] = 0
        elif l <= 20:
            levels[c] = 3
        else:
            rest.append(c)
    total = sum(cnt[c] for c in rest)
    acc = 0
    for c in rest:                      # light -> dark
        levels[c] = 1 if acc < total / 2 else 2
        acc += cnt[c]
    return levels


def crop_rows():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_crystal_crops.txt")
    out = {}
    if os.path.exists(path):
        for line in open(path):
            p = line.split("#")[0].split()
            if len(p) == 2:
                out[p[0]] = int(p[1])
    return out


def convert_one(frames, name=None):
    from PIL import ImageOps
    frames = [ImageOps.mirror(f.convert("RGBA")) for f in frames]
    bbox = None
    for f in frames:
        b = f.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if b:
            bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
    frames = [f.crop(bbox) for f in frames]
    sw, sh = frames[0].size
    w, h = 64, 48
    x0 = -(w - sw) // 2 if sw <= w else (sw - w) // 2
    y0 = -(h - sh) if sh <= h else 0
    if sh > h and name in crop_rows():
        y0 = min(max(crop_rows()[name], 0), sh - h)   # face in view (walker_crystal_crops.txt)
    lv = palette_levels(frames)
    outs = []
    for f in frames:
        px = f.load()
        out = [[0] * w for _ in range(h)]
        for y in range(h):
            for x in range(w):
                sx, sy = x + x0, y + y0
                if 0 <= sx < sw and 0 <= sy < sh and px[sx, sy][3] >= 128:
                    out[y][x] = lv[px[sx, sy][:3]]
        outs.append(out)
    return outs


def main():
    from PIL import Image, ImageDraw
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    names = sorted({m.group(1) for f in os.listdir(src) for m in [re.match(r"(\d{3}[A-Z]?)_1\.png$", f)] if m})
    rows = []
    for name in names:
        p1, p2 = os.path.join(src, name + "_1.png"), os.path.join(src, name + "_2.png")
        if not os.path.exists(p2):
            p2 = p1
        frames = [Image.open(p1), Image.open(p2)]
        a, b = convert_one(frames, name)
        open(os.path.join(out, name + ".large.bin"), "wb").write(ws.encode(a, 64, 48) + ws.encode(b, 64, 48))
        rows.append((name, frames[0].convert("RGBA"), a, b))
    print("converted %d Pokemon / forms to %s" % (len(rows), out))
    S, gap, per_page = 3, 10, 40
    W = 60 + 56 * 2 + 64 * S * 2 + gap * 5
    for p in range(0, len(rows), per_page):
        chunk = rows[p:p + per_page]
        sheet = Image.new("RGB", (W, 20 + len(chunk) * (48 * S + gap)), "white")
        d = ImageDraw.Draw(sheet)
        d.text((gap, 4), "source frame 1          walker frame 1 / frame 2", fill="black")
        for i, (name, src1, a, b) in enumerate(chunk):
            y = 20 + i * (48 * S + gap)
            bg = Image.new("RGB", src1.size, "white")
            bg.paste(src1, mask=src1.getchannel("A"))
            sheet.paste(bg.resize((112, 112), Image.NEAREST), (gap, y))
            d.text((gap, y + 116), "#" + name, fill="black")
            sheet.paste(ws.preview(a, S).convert("RGB"), (gap * 3 + 112, y))
            sheet.paste(ws.preview(b, S).convert("RGB"), (gap * 4 + 112 + 64 * S, y))
        fn = os.path.join(out, "preview_%s-%s.png" % (chunk[0][0], chunk[-1][0]))
        sheet.save(fn)
        print("page:", fn)


main()
