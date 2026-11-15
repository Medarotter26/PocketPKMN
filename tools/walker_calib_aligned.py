#!/usr/bin/env python3
"""Pixel-aligned calibration against the stock Pokewalker art.
For every HGSS front sprite / stock walker sprite pair in <walker_ref>:
  - the HGSS sprite (first frame, mirrored, cropped) is slid over the 64x48 stock
    sprite until its silhouette edge lands on the most stock black pixels;
  - for every interior pixel (not on the silhouette edge) the source colour and the
    stock shade are recorded.
Then: how often does a brightness rule reproduce the stock shade? Tries luma and
CIE L*, fits the 3 cut-offs, and prints the confusion table.
Usage: walker_calib_aligned.py <walker_ref dir>"""
import glob, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def lstar(rgb):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(float(v)) for v in rgb[:3])
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    f = y ** (1 / 3) if y > 216 / 24389 else (24389 / 27 * y + 16) / 116
    return 116 * f - 16          # 0..100


def load_pairs(ref):
    from PIL import Image, ImageOps
    lv = {255: 0, 168: 1, 80: 2, 0: 3}
    pairs = []
    for hg in sorted(glob.glob(os.path.join(ref, "Spr_4h_*.png"))):
        num = os.path.basename(hg)[7:10]
        pws = sorted(glob.glob(os.path.join(ref, "Spr_4pw_%s*.png" % num)))
        if not pws:
            continue
        try:
            st = Image.open(pws[0]).convert("L")
            src = ImageOps.mirror(ws.first_frame(hg))
        except Exception:
            continue
        if st.size != (64, 48):
            continue
        b = src.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if not b:
            continue
        src = src.crop(b)
        stock = [[lv.get(st.getpixel((x, y)), 0) for x in range(64)] for y in range(48)]
        pairs.append((num, src, stock))
    return pairs


def align(src, stock):
    sw, sh = src.size
    px = src.load()
    op = lambda x, y: 0 <= x < sw and 0 <= y < sh and px[x, y][3] >= 128
    edge = [(x, y) for y in range(sh) for x in range(sw) if op(x, y) and
            not all(op(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    best = (-1, 0, 0)
    for oy in range(-sh + 8, 48 - 8):
        for ox in range(-sw + 8, 64 - 8):
            s = 0
            n = 0
            for x, y in edge:
                X, Y = x + ox, y + oy
                if 0 <= X < 64 and 0 <= Y < 48:
                    n += 1
                    s += stock[Y][X] == 3
            if n > len(edge) * 0.5 and s > best[0]:
                best = (s, ox, oy)
    return best, len(edge), op


def main():
    pairs = load_pairs(sys.argv[1])
    samples = []   # (luma, L*, rgb, stock shade, num)
    good = 0
    for num, src, stock in pairs:
        (s, ox, oy), ne, op = align(src, stock)
        frac = s / max(1, ne)
        if frac < 0.45:
            print("#%s: alignment weak (%.0f%% of the edge on black) - skipped" % (num, 100 * frac))
            continue
        good += 1
        px = src.load()
        sw, sh = src.size
        for y in range(sh):
            for x in range(sw):
                if not op(x, y):
                    continue
                if not all(op(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    continue
                X, Y = x + ox, y + oy
                if 0 <= X < 64 and 0 <= Y < 48:
                    p = px[x, y]
                    samples.append((ws.lum_of(p), lstar(p), p[:3], stock[Y][X], num))
    print("%d pairs aligned, %d interior pixels" % (good, len(samples)))

    def acc(key, t):
        ok = 0
        for s in samples:
            v = s[key]
            sh = 0 if v >= t[0] else 1 if v >= t[1] else 2 if v >= t[2] else 3
            ok += sh == s[3]
        return ok / len(samples)

    print("current luma 220/150/95: %.1f%% agree" % (100 * acc(0, (220, 150, 95))))
    for key, name, rng in ((0, "luma", range(0, 256, 5)), (1, "L*", range(0, 101, 2))):
        best = None
        vals = list(rng)
        for a in vals:
            for b in vals:
                if b >= a:
                    break
                for c in vals:
                    if c >= b:
                        break
                    r = acc(key, (a, b, c))
                    if best is None or r > best[0]:
                        best = (r, a, b, c)
        print("best %s cut-offs white>=%d light>=%d dark>=%d: %.1f%% agree" % (name, best[1], best[2], best[3], 100 * best[0]))
        t = best[1:]
        conf = [[0] * 4 for _ in range(4)]
        for s in samples:
            v = s[key]
            sh = 0 if v >= t[0] else 1 if v >= t[1] else 2 if v >= t[2] else 3
            conf[s[3]][sh] += 1
        print("  rows = stock shade (white, light, dark, black), cols = this rule:")
        for i, row in enumerate(conf):
            print("   ", i, row)


main()
