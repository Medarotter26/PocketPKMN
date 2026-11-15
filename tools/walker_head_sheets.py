#!/usr/bin/env python3
"""Sheets for marking head positions of big Pokemon: each tile is the sprite as the
converter sees it (first frame, mirrored, cropped to the Pokemon) at 3x with a grid
every 10 source pixels, labelled. The head centre read off a tile goes into
tools/walker_heads.txt as "dex x y" (source pixels in that tile).
Usage: walker_head_sheets.py <cache> <out_prefix> <dex...>"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def main():
    from PIL import Image, ImageDraw, ImageOps
    cache, prefix = sys.argv[1], sys.argv[2]
    nums = [int(n) for n in sys.argv[3:]]
    S, per_row, per_sheet = 3, 4, 16
    for sh_i in range(0, len(nums), per_sheet):
        chunk = nums[sh_i:sh_i + per_sheet]
        tw, th = 100 * S + 30, 100 * S + 40
        rows = (len(chunk) + per_row - 1) // per_row
        sheet = Image.new("RGB", (tw * per_row, th * rows), "white")
        d = ImageDraw.Draw(sheet)
        for i, n in enumerate(chunk):
            im = ImageOps.mirror(ws.first_frame(os.path.join(cache, "large", "%03d.png" % n)))
            b = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
            im = im.crop(b)
            ox, oy = (i % per_row) * tw + 24, (i // per_row) * th + 24
            bg = Image.new("RGB", im.size, (235, 240, 235))
            bg.paste(im, mask=im.getchannel("A"))
            sheet.paste(bg.resize((im.width * S, im.height * S), Image.NEAREST), (ox, oy))
            for g in range(0, max(im.width, im.height) + 1, 10):
                if g <= im.width:
                    d.line([(ox + g * S, oy), (ox + g * S, oy + im.height * S)], fill=(255, 0, 0) if g % 50 == 0 else (255, 160, 160))
                    d.text((ox + g * S - 4, oy - 12), str(g), fill="red")
                if g <= im.height:
                    d.line([(ox, oy + g * S), (ox + im.width * S, oy + g * S)], fill=(255, 0, 0) if g % 50 == 0 else (255, 160, 160))
                    d.text((ox - 20, oy + g * S - 5), str(g), fill="red")
            d.text((ox, oy + im.height * S + 4), "#%d  %dx%d" % (n, im.width, im.height), fill="black")
        out = "%s_%02d.png" % (prefix, sh_i // per_sheet + 1)
        sheet.save(out)
        print(out)


main()
