#!/usr/bin/env python3
"""Before / after sheet for flagged species:
   source | large BEFORE | large AFTER | small BEFORE | small AFTER
Usage: walker_review_sheet.py <cache> <old_out> <new_out> <out.png> <dex...>"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def main():
    from PIL import Image, ImageDraw
    cache, old, new, outp = sys.argv[1:5]
    nums = [int(n) for n in sys.argv[5:]]
    S, gap = 3, 12
    cols = [100, 64 * S, 64 * S, 32 * S, 32 * S]
    W = sum(cols) + gap * (len(cols) + 1)
    H = 24 + len(nums) * (48 * S + gap)
    sheet = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(sheet)
    x = gap
    for c, t in zip(cols, ["source", "LARGE before", "LARGE after", "small before", "small after"]):
        d.text((x, 6), t, fill="black")
        x += c + gap
    for i, n in enumerate(nums):
        y = 24 + i * (48 * S + gap)
        src = ws.first_frame(os.path.join(cache, "large", "%03d.png" % n))
        t = src.copy()
        t.thumbnail((96, 96))
        bg = Image.new("RGB", t.size, "white")
        bg.paste(t, mask=t.getchannel("A"))
        sheet.paste(bg, (gap, y))
        d.text((gap, y + 100), "#%d" % n, fill="black")
        x = gap * 2 + cols[0]
        for folder in (old, new):
            p = os.path.join(folder, "%03d.bin" % n)
            if os.path.exists(p):
                b = open(p, "rb").read()
                sheet.paste(ws.preview(ws.decode(b[0x180:0x480], 64, 48), S).convert("RGB"), (x, y))
            x += 64 * S + gap
        for folder in (old, new):
            p = os.path.join(folder, "%03d.bin" % n)
            if os.path.exists(p):
                b = open(p, "rb").read()
                sheet.paste(ws.preview(ws.decode(b[0:0xC0], 32, 24), S).convert("RGB"), (x, y))
            x += 32 * S + gap
    sheet.save(outp)
    print(outp)


main()
