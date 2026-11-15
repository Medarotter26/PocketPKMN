#!/usr/bin/env python3
"""Where do the small sprites lose detail? For every Pokemon: pairs of neighbouring
icon pixels that clearly differ (brightness step >= 30 or colour difference >= 22)
but got the SAME grey ("lost"), in the current small sprite (out/) and with the new
icon conversion (icon_frames). Prints a table and writes a sheet of the candidates
(current loses a lot, new keeps clearly more): icon | now | new.
Usage: walker_small_detail.py <cache> <out_prefix> [min_lost] [min_gain]"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def lost_pairs(frame_rgba, shades, x0, y0):
    px = frame_rgba.load()
    sw, sh = frame_rgba.size
    op = lambda x, y: 0 <= x < sw and 0 <= y < sh and px[x, y][3] >= 128
    n = 0
    for y in range(24):
        for x in range(32):
            sx, sy = x + x0, y + y0
            if not op(sx, sy):
                continue
            for ex, ey in ((1, 0), (0, 1)):
                if x + ex >= 32 or y + ey >= 24 or not op(sx + ex, sy + ey):
                    continue
                a, b = px[sx, sy][:3], px[sx + ex, sy + ey][:3]
                if (abs(ws.lum_of(a) - ws.lum_of(b)) >= 30 or ws.delta_e(a, b) >= 22) and \
                        shades[y][x] == shades[y + ey][x + ex]:
                    n += 1
    return n


def window(frames, num):
    from PIL import ImageOps
    fr = [ImageOps.mirror(f) for f in frames]
    bbox = None
    for f in fr:
        b = f.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if b:
            bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
    fr = [f.crop(bbox) for f in fr]
    sw, sh = fr[0].size
    x0 = -(32 - sw) // 2 if sw <= 32 else sw - 32
    y0 = -(24 - sh) if sh <= 24 else 0
    dx, dy = ws.crop_override(num, 32)
    x0 = min(max(x0 + dx, 0), sw - 32) if sw > 32 else x0 + dx
    y0 = min(max(y0 + dy, 0), sh - 24) if sh > 24 else y0 + dy
    return fr[0], x0, y0


def main():
    from PIL import Image, ImageDraw
    cache, prefix = sys.argv[1], sys.argv[2]
    min_lost = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    min_gain = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
    rows, cands = [], []
    for f in sorted(os.listdir(os.path.join(cache, "small"))):
        num = int(f[:3])
        binp = os.path.join(cache, "out", "%03d.bin" % num)
        if not os.path.exists(binp):
            continue
        frames = ws.all_frames(os.path.join(cache, "small", f))[:2]
        cur = ws.decode(open(binp, "rb").read()[0:0xC0], 32, 24)
        new = ws.icon_frames(frames, num)[0]
        fr0, x0, y0 = window(frames, num)
        lc, ln = lost_pairs(fr0, cur, x0, y0), lost_pairs(fr0, new, x0, y0)
        rows.append((num, lc, ln))
        if lc >= min_lost and ln <= lc * (1 - min_gain):
            cands.append((num, frames, fr0, x0, y0, cur, new, lc, ln))
    with open(prefix + ".csv", "w") as c:
        c.write("dex,lost_now,lost_new\n")
        for r in rows:
            c.write("%d,%d,%d\n" % r)
    print("%d small sprites; %d candidates (now loses >= %d details, new keeps >= %d%% of them)" % (
        len(rows), len(cands), min_lost, int(min_gain * 100)))
    print("candidates:", " ".join(str(c[0]) for c in cands))
    S, per = 6, 4
    cw, ch = 32 * S * 3 + 40, 24 * S + 22
    for page in range(0, len(cands), 24):
        chunk = cands[page:page + 24]
        sheet = Image.new("RGB", (cw * per, ch * ((len(chunk) + per - 1) // per)), "white")
        d = ImageDraw.Draw(sheet)
        for i, (num, frames, fr0, x0, y0, cur, new, lc, ln) in enumerate(chunk):
            x, y = (i % per) * cw, (i // per) * ch
            canvas = Image.new("RGBA", (32, 24), (230, 240, 230, 255))
            canvas.alpha_composite(fr0, (-x0, -y0)) if x0 <= 0 and y0 <= 0 else None
            if x0 > 0 or y0 > 0:
                canvas.alpha_composite(fr0.crop((max(x0, 0), max(y0, 0), max(x0, 0) + 32, max(y0, 0) + 24)),
                                       (max(-x0, 0), max(-y0, 0)))
            sheet.paste(canvas.convert("RGB").resize((32 * S, 24 * S), Image.NEAREST), (x, y + 16))
            sheet.paste(ws.preview(cur, S).convert("RGB"), (x + 32 * S + 8, y + 16))
            sheet.paste(ws.preview(new, S).convert("RGB"), (x + 64 * S + 16, y + 16))
            d.text((x, y + 2), "#%d  icon / now (lost %d) / new (lost %d)" % (num, lc, ln), fill="black")
        name = "%s_%02d.png" % (prefix, page // 24 + 1)
        sheet.save(name)
        print(name)


main()
