#!/usr/bin/env python3
"""Fit the brightness thresholds of walker_sprites' "walker" mode to the stock
Pokewalker art: for every HGSS / walker pair that fits uncropped, compare how
many light-grey, dark-grey and black pixels each has (no alignment needed), and
grid-search the thresholds with the smallest total difference.
Usage: walker_fit.py <walker_ref dir>"""
import glob, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def counts(rows):
    c = [0, 0, 0, 0]
    for r in rows:
        for v in r:
            c[v] += 1
    return c[1:]


def main():
    from PIL import Image
    ref = sys.argv[1]
    pairs = []
    for hg in sorted(glob.glob(os.path.join(ref, "Spr_4h_*.png"))):
        num = os.path.basename(hg)[7:10]
        pw = [p for p in glob.glob(os.path.join(ref, "Spr_4pw_%s*.png" % num))]
        if not pw:
            continue
        im = ws.first_frame(hg)
        bb = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if not bb or bb[2] - bb[0] > 64 or bb[3] - bb[1] > 48:
            continue                 # cropped on the walker: counts would not compare
        st = Image.open(pw[0]).convert("L")
        lv = {255: 0, 168: 1, 80: 2, 0: 3}
        stock = [[lv.get(st.getpixel((x, y)), 0) for x in range(64)] for y in range(48)]
        px = im.crop(bb).load()
        lums = [0.299 * px[x, y][0] + 0.587 * px[x, y][1] + 0.114 * px[x, y][2]
                for y in range(bb[3] - bb[1]) for x in range(bb[2] - bb[0]) if px[x, y][3] >= 128]
        pairs.append((num, lums, counts(stock)))
    print(len(pairs), "uncropped pairs:", [p[0] for p in pairs])
    best = None
    for a in range(150, 250, 5):
        for b in range(90, a, 5):
            for c in range(30, b, 5):
                err = 0
                for num, lums, sc in pairs:
                    mc = [0, 0, 0]
                    for l in lums:
                        if l >= a:
                            continue
                        mc[0 if l >= b else 1 if l >= c else 2] += 1
                    err += sum(abs(x - y) for x, y in zip(mc, sc))
                if best is None or err < best[0]:
                    best = (err, a, b, c)
    print("best thresholds white>=%d light>=%d dark>=%d (total count error %d)" % (best[1], best[2], best[3], best[0]))


main()
