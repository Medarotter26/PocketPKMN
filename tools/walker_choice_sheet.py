#!/usr/bin/env python3
"""One sheet to choose from: per species
   source | LARGE from the full sprite (walker style) | LARGE = menu icon x2 | SMALL = menu icon
Usage: walker_choice_sheet.py <full_cache> <icon_cache> <out.png>"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
from walker_sprites import first_frame, to_shades, preview


def main():
    from PIL import Image, ImageDraw
    full, icons, out = sys.argv[1:4]
    nums = sorted(int(f[:3]) for f in os.listdir(full) if f.endswith(".png"))
    S, gap = 3, 14
    cw = [100, 64 * S, 64 * S, 32 * S]
    W = sum(cw) + gap * 5
    H = 30 + len(nums) * (48 * S + gap)
    sheet = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(sheet)
    heads = ["source", "LARGE: full sprite", "LARGE: menu icon x2", "SMALL: menu icon"]
    x = gap
    for i, h in enumerate(heads):
        d.text((x, 8), h, fill="black")
        x += cw[i] + gap
    for r, n in enumerate(nums):
        y = 30 + r * (48 * S + gap)
        src = first_frame(os.path.join(full, "%03d.png" % n))
        t = src.copy()
        t.thumbnail((96, 96))
        bg = Image.new("RGB", t.size, "white")
        bg.paste(t, mask=t.getchannel("A"))
        sheet.paste(bg, (gap, y))
        d.text((gap, y + 100), "#%d" % n, fill="black")
        sheet.paste(preview(to_shades(src, 64, 48, "walker"), S).convert("RGB"), (gap * 2 + cw[0], y))
        ip = os.path.join(icons, "%03d.png" % n)
        if os.path.exists(ip):
            small = to_shades(first_frame(ip), 32, 24, "icon")
            big = [[v for v in row for _ in (0, 1)] for row in small for _ in (0, 1)]
            sheet.paste(preview(big, S).convert("RGB"), (gap * 3 + cw[0] + cw[1], y))
            sheet.paste(preview(small, S).convert("RGB"), (gap * 4 + cw[0] + cw[1] + cw[2], y))
    sheet.save(out)
    print(out)


main()
