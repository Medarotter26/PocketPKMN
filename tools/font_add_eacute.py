#!/usr/bin/env python3
"""font_add_eacute.py <src dir> <out dir> - copies of the user's Console Square Beta .otf files with an
'eacute' (U+00E9) added: the font's own 'e' + its own 'grave' mirrored into an acute and placed above the e
(bottom at the height of the i's dot). The originals are not changed. Needs fontTools (WSL: ~/wbx-fontenv)."""
import os
import sys

from fontTools.pens.boundsPen import BoundsPen
from fontTools.pens.recordingPen import RecordingPen
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.ttLib import TTFont


def add_eacute(src, dst):
    f = TTFont(src)
    gs = f.getGlyphSet()
    if "eacute" in gs:
        f.save(dst)
        return "already has eacute"
    rec = RecordingPen()
    gs["grave"].draw(rec)
    gb, eb, ib = BoundsPen(gs), BoundsPen(gs), BoundsPen(gs)
    gs["grave"].draw(gb)
    gs["e"].draw(eb)
    gs["i"].draw(ib)
    gx0, gy0, gx1, gy1 = gb.bounds
    ex0, _, ex1, _ = eb.bounds
    # bottom of the i's dot = the lowest y of the i's highest contour
    irec = RecordingPen()
    gs["i"].draw(irec)
    contours, cur = [], []
    for op, args in irec.value:
        if op == "closePath":
            contours.append(cur)
            cur = []
        else:
            cur += [p[1] for p in args]
    dot_bottom = max(min(c) for c in contours if c)
    dx = (ex0 + ex1) / 2 - (gx0 + gx1) / 2
    dy = dot_bottom - gy0

    def acute(x, y):  # mirror the grave left-right inside its own box, then move it over the e
        return (gx0 + gx1 - x + dx, y + dy)

    width = f["hmtx"]["e"][0]
    cff = f["CFF "].cff
    top = cff.topDictIndex[0]
    pen = T2CharStringPen(width, gs)
    gs["e"].draw(pen)
    for op, args in rec.value:
        if op == "moveTo":
            pen.moveTo(acute(*args[0]))
        elif op == "lineTo":
            pen.lineTo(acute(*args[0]))
        elif op == "curveTo":
            pen.curveTo(*[acute(*p) for p in args])
        elif op == "closePath":
            pen.closePath()
    cs = pen.getCharString(private=top.Private, globalSubrs=cff.GlobalSubrs)
    chars = top.CharStrings
    chars.charStringsIndex.append(cs)
    chars.charStrings["eacute"] = len(chars.charStringsIndex) - 1
    # one new list for both: the CFF charset and the font's glyph order can be the same object
    order = [g for g in f.getGlyphOrder() if g != "eacute"] + ["eacute"]
    top.charset = order
    f.setGlyphOrder(order)
    f["hmtx"]["eacute"] = (width, f["hmtx"]["e"][1])
    for t in f["cmap"].tables:
        if t.isUnicode():
            t.cmap[0xE9] = "eacute"
    f["maxp"].numGlyphs = len(f.getGlyphOrder())
    f.save(dst)
    return f"added (acute moved {dx:.0f}, {dy:.0f})"


def main():
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for n in sorted(os.listdir(src)):
        if n.endswith(".otf"):
            print(n, add_eacute(os.path.join(src, n), os.path.join(out, n)))


if __name__ == "__main__":
    main()
