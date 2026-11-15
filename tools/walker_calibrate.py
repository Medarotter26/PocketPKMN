#!/usr/bin/env python3
"""Calibration: run the HGSS front sprites through walker_sprites' "walker" mode and
put each result next to the stock Pokewalker sprite of the same species.
Usage: walker_calibrate.py <walker_ref dir> <out.png>   (WBX_THRESH=a,b,c to try thresholds)"""
import glob, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from walker_sprites import first_frame, to_shades, preview


def main():
    from PIL import Image
    ref, out = sys.argv[1], sys.argv[2]
    pairs = []
    for hg in sorted(glob.glob(os.path.join(ref, "Spr_4h_*.png"))):
        if not open(hg, "rb").read(4).startswith(b"\x89PNG"):
            continue
        num = os.path.basename(hg)[7:10]
        pw = [p for p in glob.glob(os.path.join(ref, "Spr_4pw_%s*.png" % num))]
        good = None
        for p in pw:
            try:
                Image.open(p)
                good = p
            except Exception:
                pass
        if good:
            pairs.append((hg, good))
    S = 3
    sheet = Image.new("L", (64 * S * 2 + 20, 48 * S * len(pairs) + 10 * len(pairs)), 255)
    for i, (hg, pw) in enumerate(pairs):
        mine = preview(to_shades(first_frame(hg), 64, 48, "walker"), S)
        stock = Image.open(pw).convert("L")
        stock = stock.resize((stock.width * S, stock.height * S), Image.NEAREST)
        y = i * (48 * S + 10)
        sheet.paste(mine, (0, y))
        sheet.paste(stock, (64 * S + 20, y))
    sheet.save(out)
    print(out, len(pairs), "pairs")


main()
