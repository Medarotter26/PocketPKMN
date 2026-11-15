#!/usr/bin/env python3
"""Which small sprites (menu icons) do not fit the walker's 32x24 small sprite?
For every <cache>/small/NNN.png: the Pokemon's size over all its frames (empty
rows/columns of the icon ignored) and how many rows / columns would be cut.
Writes <out>.csv and a picture of every one that is cut (red line = the cut).
Usage: walker_small_fit.py <cache> <out_prefix>"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def main():
    from PIL import Image, ImageDraw, ImageOps
    cache, out = sys.argv[1], sys.argv[2]
    rows, cut = [], []
    for f in sorted(os.listdir(os.path.join(cache, "small"))):
        if not f.endswith(".png"):
            continue
        num = int(f[:3])
        frames = ws.all_frames(os.path.join(cache, "small", f))[:2]
        bbox = None
        for fr in frames:
            b = fr.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
            if b:
                bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
        if not bbox:
            continue
        w, h = bbox[2] - bbox[0], bbox[3] - bbox[1]
        lost_rows, lost_cols = max(0, h - 24), max(0, w - 32)
        rows.append((num, frames[0].size, w, h, lost_rows, lost_cols))
        if lost_rows or lost_cols:
            cut.append((num, frames[0], bbox, lost_rows, lost_cols))
    with open(out + ".csv", "w") as c:
        c.write("dex,icon_size,pokemon_w,pokemon_h,rows_cut,cols_cut\n")
        for r in rows:
            c.write("%d,%dx%d,%d,%d,%d,%d\n" % (r[0], r[1][0], r[1][1], r[2], r[3], r[4], r[5]))
    print("%d icons, %d fit, %d need cutting" % (len(rows), len(rows) - len(cut), len(cut)))
    print("rows cut: " + ", ".join("#%d %d" % (n, lr) for n, _, _, lr, _ in cut if lr))
    print("cols cut: " + ", ".join("#%d %d" % (n, lc) for n, _, _, _, lc in cut if lc))
    if cut:
        S, per = 6, 8
        cw, ch = 44 * S, 52 * S
        sheet = Image.new("RGB", (cw * per, ch * ((len(cut) + per - 1) // per)), "white")
        d = ImageDraw.Draw(sheet)
        for i, (num, fr, bbox, lr, lc) in enumerate(cut):
            x, y = (i % per) * cw, (i // per) * ch
            im = ImageOps.mirror(fr)
            bg = Image.new("RGB", im.size, (225, 235, 225))
            bg.paste(im, mask=im.getchannel("A"))
            sheet.paste(bg.resize((im.width * S, im.height * S), Image.NEAREST), (x, y))
            # the 32x24 window the converter keeps: top of the Pokemon, centred / facing side
            mb = (im.width - bbox[2], bbox[1], im.width - bbox[0], bbox[3])
            bw = mb[2] - mb[0]
            x0 = mb[0] + (-(32 - bw) // 2 if bw <= 32 else bw - 32)
            d.rectangle([x + x0 * S, y + mb[1] * S, x + (x0 + 32) * S - 1, y + (mb[1] + 24) * S - 1], outline="red", width=2)
            d.text((x + 4, y + im.height * S + 4), "#%d  -%d rows -%d cols" % (num, lr, lc), fill="black")
        sheet.save(out + ".png")
        print(out + ".png")


main()
