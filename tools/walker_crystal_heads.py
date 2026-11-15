#!/usr/bin/env python3
"""Grid sheets for marking faces of the Crystal sprites taller than 48 rows: each tile
is the Pokemon as walker_crystal.py sees it (both frames' shared box, mirrored, frame
1 shown) at 4x with a grid every 8 px. Prints the list of tall ones.
Usage: walker_crystal_heads.py <in_dir> <out_prefix>"""
import os, re, sys
from PIL import Image, ImageDraw, ImageOps


def shared(src, name):
    fs = []
    for k in ("1", "2"):
        p = os.path.join(src, "%s_%s.png" % (name, k))
        if os.path.exists(p):
            fs.append(ImageOps.mirror(Image.open(p).convert("RGBA")))
    bbox = None
    for f in fs:
        b = f.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
    return fs[0].crop(bbox)


def main():
    src, prefix = sys.argv[1], sys.argv[2]
    names = sorted({m.group(1) for f in os.listdir(src) for m in [re.match(r"(\d{3}[A-Z]?)_1\.png$", f)] if m})
    tall = []
    for n in names:
        im = shared(src, n)
        if im.height > 48:
            tall.append((n, im))
    print(len(tall), "tall:", " ".join("%s(%d)" % (n, im.height) for n, im in tall))
    S, per_row, per_sheet = 4, 5, 25
    tw, th = 60 * S + 30, 60 * S + 30
    for s in range(0, len(tall), per_sheet):
        chunk = tall[s:s + per_sheet]
        rows = (len(chunk) + per_row - 1) // per_row
        sheet = Image.new("RGB", (tw * per_row, th * rows), "white")
        d = ImageDraw.Draw(sheet)
        for i, (n, im) in enumerate(chunk):
            ox, oy = (i % per_row) * tw + 22, (i // per_row) * th + 14
            bg = Image.new("RGB", im.size, (235, 240, 235))
            bg.paste(im, mask=im.getchannel("A"))
            sheet.paste(bg.resize((im.width * S, im.height * S), Image.NEAREST), (ox, oy))
            for g in range(0, 57, 8):
                if g <= im.width:
                    d.line([(ox + g * S, oy), (ox + g * S, oy + im.height * S)], fill=(255, 150, 150))
                    d.text((ox + g * S - 3, oy - 12), str(g), fill="red")
                if g <= im.height:
                    d.line([(ox, oy + g * S), (ox + im.width * S, oy + g * S)], fill=(255, 150, 150))
                    d.text((ox - 18, oy + g * S - 5), str(g), fill="red")
            d.text((ox, oy + im.height * S + 2), "#%s %dx%d" % (n, im.width, im.height), fill="black")
        fn = "%s_%02d.png" % (prefix, s // per_sheet + 1)
        sheet.save(fn)
        print(fn)


main()
