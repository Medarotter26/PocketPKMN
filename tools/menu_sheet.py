#!/usr/bin/env python3
"""menu_sheet.py - contact sheet of the whole WalkerBox menu art set.

  python3 tools/menu_sheet.py [out.png] [--scale N]

Default output: hw/dump/menu_art_sheet.png at 3x. Each slot is drawn at
the scale on a blue-grey board with its address and name under it
(labels use the WalkerBox font). White slots show as a hatched box.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pwimg  # noqa: E402

BG = (196, 206, 222)
BORDER = (90, 110, 150)
LABEL = (20, 30, 60)


def main():
    args = sys.argv[1:]
    scale = 3
    if "--scale" in args:
        i = args.index("--scale")
        scale = int(args[i + 1])
        del args[i:i + 2]
    out = args[0] if args else os.path.join(pwimg.ROOT, "hw", "dump", "menu_art_sheet.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)

    sheet_w = 1400
    pad = 10
    label_h = 2 * 16 + 2   # two label lines at 1x (font is 16 tall), drawn at 1x
    cells = []
    for sl in pwimg.SLOTS:
        sh, data, fit = pwimg.render_slot(sl)
        if sl["kind"] == "mask":
            # show the mask as an 8x8 picture: cleared pixels dark
            sh = bytearray(64)
            for x in range(8):
                for y in range(8):
                    if not (data[x] >> y) & 1:
                        sh[y * 8 + x] = 2
            w, h = 8, 8
        else:
            w, h = sl["w"], sl["h"]
        cells.append((sl, sh, w, h))

    # flow layout
    x = y = pad
    row_h = 0
    pos = []
    for sl, sh, w, h in cells:
        lw1 = pwimg.text_width("%04X" % sl["addr"])
        lw2 = pwimg.text_width(sl["name"])
        cw = max(w * scale + 2, lw1 + 2, lw2 + 2)
        ch = h * scale + 2 + label_h
        if x + cw > sheet_w - pad:
            x = pad
            y += row_h + pad
            row_h = 0
        pos.append((x, y, cw))
        x += cw + pad
        row_h = max(row_h, ch)
    sheet_h = y + row_h + pad

    px = bytearray(bytes(BG) * (sheet_w * sheet_h))

    def put(xx, yy, rgb):
        if 0 <= xx < sheet_w and 0 <= yy < sheet_h:
            o = (yy * sheet_w + xx) * 3
            px[o:o + 3] = bytes(rgb)

    def label(text, lx, ly):
        tmp = bytearray(sheet_w * 16)
        pwimg.draw_text_line(tmp, sheet_w, 16, text, 0, 0)
        for yy in range(16):
            for xx in range(min(sheet_w, pwimg.text_width(text) + 1)):
                if tmp[yy * sheet_w + xx]:
                    put(lx + xx, ly + yy, LABEL)

    for (sl, sh, w, h), (cx, cy, cw) in zip(cells, pos):
        iw, ih = w * scale, h * scale
        for xx in range(iw + 2):
            put(cx + xx, cy, BORDER)
            put(cx + xx, cy + ih + 1, BORDER)
        for yy in range(ih + 2):
            put(cx, cy + yy, BORDER)
            put(cx + iw + 1, cy + yy, BORDER)
        for yy in range(ih):
            for xx in range(iw):
                if sh is None:
                    g = 235 if ((xx + yy) // 4) % 2 else 255
                    rgb = (g, g, g)
                else:
                    v = pwimg.GREY[sh[(yy // scale) * w + (xx // scale)]]
                    rgb = (v, v, v)
                put(cx + 1 + xx, cy + 1 + yy, rgb)
        label("%04X" % sl["addr"], cx, cy + ih + 2)
        label(sl["name"], cx, cy + ih + 2 + 16)

    rows = [px[r * sheet_w * 3:(r + 1) * sheet_w * 3] for r in range(sheet_h)]
    pwimg.write_png(out, sheet_w, sheet_h, rows)
    print("wrote %s (%dx%d, %d slots, scale %d)" % (out, sheet_w, sheet_h, len(cells), scale))


if __name__ == "__main__":
    main()
