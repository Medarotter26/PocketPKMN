#!/usr/bin/env python3
"""Walker SMALL sprites (32x24, 2 frames) for #001-251 from the fan-made G/S/C
"MINIDEX 001-251" minisprite sheet (user, 2026-11-15: "for small sprites can we use
these?"; https://i.imgur.com/VKCdkx5.gif, credits in art_src/minisprites/CREDITS.txt).

The sheet: 509x472 GIF, 2 frames (the 2 animation frames), background (218,229,214),
16x16 sprites at x = 10 + 34*col, y = 37 + 25*row, 15 per row, dex = row*15+col+1.
Conversion: background -> transparent; each Pokemon's colours by brightness rank onto
the walker's 4 shades (as walker_crystal.py); mirrored (walker sprites face right);
both frames share one window. Size: mode "1x" = real size, centred, standing on the
bottom edge; mode "2x" = every pixel doubled, then into 32x24 (rows beyond 24: keep top).
Output: <out>/NNN.small.bin (frame 1 + frame 2, 2 x 0xC0) + preview pages; cut sprites
in <out>/../cut/NNN_1.png / NNN_2.png.
Usage: walker_minisprites.py <sheet.gif> <out_dir> <1x|2x> [dex...]"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws
import importlib.util

BG = (218, 229, 214)


def cut(sheet_path, cut_dir):
    from PIL import Image
    os.makedirs(cut_dir, exist_ok=True)
    im = Image.open(sheet_path)
    frames = []
    for i in range(2):
        im.seek(i)
        frames.append(im.convert("RGB"))
    out = {}
    for n in range(1, 252):
        r, c = divmod(n - 1, 15)
        x, y = 10 + 34 * c, 37 + 25 * r
        pair = []
        for k, f in enumerate(frames):
            t = f.crop((x, y, x + 16, y + 16)).convert("RGBA")
            px = t.load()
            for yy in range(16):
                for xx in range(16):
                    if px[xx, yy][:3] == BG:
                        px[xx, yy] = (0, 0, 0, 0)
            t.save(os.path.join(cut_dir, "%03d_%d.png" % (n, k + 1)))
            pair.append(t)
        out[n] = pair
    return out


def levels_for(pair):
    spec = importlib.util.spec_from_file_location("wc", os.path.join(os.path.dirname(__file__), "walker_crystal.py"))
    return None


def to_small(pair, mode):
    from PIL import ImageOps
    from collections import Counter
    fr = [ImageOps.mirror(f) for f in pair]
    bbox = None
    for f in fr:
        b = f.getchannel("A").getbbox()
        if b:
            bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
    fr = [f.crop(bbox) for f in fr]
    if mode == "2x":
        fr = [f.resize((f.width * 2, f.height * 2), 0) for f in fr]   # NEAREST: pixels doubled
    cnt = Counter()
    for f in fr:
        px = f.load()
        cnt.update(px[x, y][:3] for y in range(f.height) for x in range(f.width) if px[x, y][3] >= 128)
    cols = sorted(cnt, key=lambda c: -ws.lum_of(c))
    lv = {}
    rest = []
    for c in cols:
        l = ws.lum_of(c)
        if l >= 245:
            lv[c] = 0
        elif l <= 20:
            lv[c] = 3
        else:
            rest.append(c)
    if len(rest) == 1:
        lv[rest[0]] = 1 if ws.lum_of(rest[0]) >= 128 else 2
    elif len(rest) == 2:
        lv[rest[0]], lv[rest[1]] = 1, 2
    elif rest:
        total = sum(cnt[c] for c in rest)
        acc = 0
        for c in rest:
            lv[c] = 1 if acc < total / 2 else 2
            acc += cnt[c]
    sw, sh = fr[0].size
    w, h = 32, 24
    x0 = -(w - sw) // 2 if sw <= w else (sw - w) // 2
    y0 = -(h - sh) if sh <= h else 0
    outs = []
    for f in fr:
        px = f.load()
        o = [[0] * w for _ in range(h)]
        for y in range(h):
            for x in range(w):
                sx, sy = x + x0, y + y0
                if 0 <= sx < sw and 0 <= sy < sh and px[sx, sy][3] >= 128:
                    o[y][x] = lv[px[sx, sy][:3]]
        outs.append(o)
    return outs


def main():
    from PIL import Image, ImageDraw
    sheet, out, mode = sys.argv[1], sys.argv[2], sys.argv[3]
    only = [int(a) for a in sys.argv[4:]]
    os.makedirs(out, exist_ok=True)
    sprites = cut(sheet, os.path.join(os.path.dirname(out.rstrip("/")), "cut"))
    rows = []
    for n, pair in sprites.items():
        if only and n not in only:
            continue
        a, b = to_small(pair, mode)
        open(os.path.join(out, "%03d.small.bin" % n), "wb").write(ws.encode(a, 32, 24) + ws.encode(b, 32, 24))
        rows.append((n, pair[0], a, b))
    S, per = 4, 6
    cw, ch = 16 * S + 32 * S * 2 + 40, 24 * S + 22
    for p in range(0, len(rows), 60):
        chunk = rows[p:p + 60]
        sh_ = Image.new("RGB", (cw * per, ch * ((len(chunk) + per - 1) // per)), "white")
        d = ImageDraw.Draw(sh_)
        for i, (n, src, a, b) in enumerate(chunk):
            x, y = (i % per) * cw, (i // per) * ch
            bg = Image.new("RGB", src.size, BG)
            bg.paste(src, mask=src.getchannel("A"))
            sh_.paste(bg.resize((16 * S, 16 * S), 0), (x, y + 16))
            sh_.paste(ws.preview(a, S).convert("RGB"), (x + 16 * S + 8, y + 16))
            sh_.paste(ws.preview(b, S).convert("RGB"), (x + 16 * S + 32 * S + 16, y + 16))
            d.text((x, y + 2), "#%03d  sheet / frame 1 / frame 2" % n, fill="black")
        fn = os.path.join(out, "preview_%s_%03d-%03d.png" % (mode, chunk[0][0], chunk[-1][0]))
        sh_.save(fn)
        print(fn)
    print("converted %d (%s)" % (len(rows), mode))


main()
