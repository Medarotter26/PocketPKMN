#!/usr/bin/env python3
"""Two-frame Gen 5 walker files (user, 2026-11-15: "Make walker files" after choosing a
second key frame for 124 Gen 5 Pokemon / forms, art_src/gen5keys/keys/<key>_1.png, _2.png).

Rule: everything the user already reviewed stays byte for byte - the small sprite, large
frame 1, its crop and colours (art_src/sprites5to7/out/NNN.bin). Only LARGE FRAME 2 is new:
  - converted with to_walker_frames([f1, f2], ref_first=True): the same window and colour
    rules as frame 1 (decided from frame 1 alone), frame 2's moved parts kept;
  - if the current file's frame 1 is not what today's rules give (reviewed with an older
    rule version, or a hand edit), frame 2 = the current frame 1 wherever the converted
    frames 1 and 2 agree, and the converted frame 2 only where the pose changed ("patched").
Forms / genders without their own file (550B, 585A, 641T, 646W, 647R, 521_f ...): the
base file whose frame 1 matches is used; the others get a full new file (small sprite =
the base Pokemon's), listed as "new".

  walker_gen5_two.py <keys_dir> <base_out> <out>  -> <out>/<key>.bin (0x780) + report.txt + sheets"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws
from PIL import Image, ImageDraw

keys_dir, base_out, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
kf = json.load(open(os.path.join(keys_dir, "keyframes.json")))
report, rows = [], []


def same(a, b):
    return sum(1 for y in range(48) for x in range(64) if a[y][x] == b[y][x]) / (64 * 48)


def shift(a, dx, dy):
    """Picture moved dx right / dy down (empty = white)."""
    return [[a[y - dy][x - dx] if 0 <= y - dy < 48 and 0 <= x - dx < 64 else 0 for x in range(64)] for y in range(48)]


F1, F2 = {}, {}
HEAD = ws.head_point
# walker_colours.txt lines marked "# base-only" are for the base form only (Keldeo, 2026-11-15):
# forms of that Pokemon (647R ...) do not get them.
COLOURS = ws.colour_overrides
BASE_ONLY = set()
for _line in open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_colours.txt")):
    if "base-only" in _line:
        _q = _line.split("#")[0].split()
        if len(_q) == 3:
            BASE_ONLY.add((int(_q[0]), tuple(int(_q[1][i:i + 2], 16) for i in (0, 2, 4))))


def form_colours(num):
    return {c: s for c, s in COLOURS(num).items() if (num, c) not in BASE_ONLY}
# Forms shaped unlike their base Pokemon: own face points "key x y" (mirrored, trimmed
# frame 1, as walker_heads.txt) in tools/walker_gen5_form_heads.txt.
FORM_HEADS = {}
_fh = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_gen5_form_heads.txt")
if os.path.exists(_fh):
    for line in open(_fh):
        q = line.split("#")[0].split()
        if len(q) == 3:
            FORM_HEADS[q[0]] = (int(q[1]), int(q[2]))

# Pokemon with hand settings for frame 1 (paint / erase / fill boxes in walker_colours.txt): in frame 2
# those boxes would hit whatever moved into them (user, 2026-11-16: "sigigiligh has a plain square
# blocking in the lower left corner" - Sigilyph's erase box cut its wing). Frame 2 is worked out
# without them and keeps frame 1's pixels only where the pose did not change.
# Only the flagged one: for Servine (erase boxes = cut-off tail bits, right in frame 2 too) and Pignite
# (paint) the old way stays (user's rule: change only what was flagged).
HAND = {561}
_HAND_FNS = (ws.paints, ws.erases, ws.fills)


def no_hand(fn, *a, **kw):
    ws.paints, ws.erases, ws.fills = (lambda n, w: []), (lambda n, w: []), (lambda n: [])
    try:
        return fn(*a, **kw)
    finally:
        ws.paints, ws.erases, ws.fills = _HAND_FNS


# Frame 2 moved sideways / up-down before converting, "key dx dy" in the walker picture (dx > 0 =
# right): tools/walker_gen5_frame2_shift.txt (user: "Vanillite moves too far right for its second
# frame animation").
F2SHIFT = {}
_fs = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_gen5_frame2_shift.txt")
if os.path.exists(_fs):
    for line in open(_fs):
        q = line.split("#")[0].split()
        if len(q) == 3:
            F2SHIFT[q[0]] = (int(q[1]), int(q[2]))


def moved(im, dx, dy):
    """Source frame moved so the WALKER picture moves dx right / dy down (the source is mirrored)."""
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    out.paste(im, (-dx, dy))
    return out


by_num = {}
for k in sorted(kf):
    by_num.setdefault(int(k[:3]), []).append(k)

for num, ks in sorted(by_num.items()):
    base_p = os.path.join(base_out, "%03d.bin" % num)
    base = open(base_p, "rb").read() if os.path.exists(base_p) else None
    base1 = ws.decode(base[0x180:0x480], 64, 48) if base else None
    conv = {}
    for k in ks:
        f1 = Image.open(os.path.join(keys_dir, k + "_1.png")).convert("RGBA")
        f2 = Image.open(os.path.join(keys_dir, k + "_2.png")).convert("RGBA")
        if k in F2SHIFT:
            f2 = moved(f2, *F2SHIFT[k])
        F1[k], F2[k] = f1, f2
        ws.head_point = (lambda p: (lambda n: p))(FORM_HEADS[k]) if k in FORM_HEADS else HEAD
        ws.colour_overrides = COLOURS if k == "%03d" % num else form_colours
        conv[k] = ws.to_walker_frames([f1, f2], 64, 48, num, ref_first=True)
    ws.head_point = HEAD
    ws.colour_overrides = COLOURS
    # which key is the reviewed base file? (the one whose frame 1 matches best)
    # the plain number owns its file (645, not 645T); genders (521_f / 521_m): the best match
    owner = ("%03d" % num if "%03d" % num in ks else max(ks, key=lambda k: same(conv[k][0], base1))) if base else None
    for k in ks:
        c1, c2 = conv[k]
        if k == owner:
            if c1 == base1 and num in HAND:
                n1, n2 = no_hand(ws.to_walker_frames, [F1[k], F2[k]], 64, 48, num, ref_first=True)
                f2s = [[base1[y][x] if n1[y][x] == n2[y][x] else n2[y][x] for x in range(64)] for y in range(48)]
                how = "exact, hand settings kept only where the pose did not change"
            elif c1 == base1:
                f2s, how = c2, "exact"
            else:
                # the reviewed frame 1 may sit in another window (older crop): line today's
                # frames up with it first, then take frame 2 only where the pose changed
                dx, dy = max(((dx, dy) for dx in range(-24, 25) for dy in range(-24, 25)),
                             key=lambda s: same(shift(c1, *s), base1))
                # convert again IN the reviewed window (no empty edge strips), then take
                # frame 2 only where the pose changed
                s1, s2 = no_hand(ws.to_walker_frames, [F1[k], F2[k]], 64, 48, num, ref_first=True, window_shift=(dx, dy))
                f2s = [[base1[y][x] if s1[y][x] == s2[y][x] else s2[y][x] for x in range(64)] for y in range(48)]
                m = same(s1, base1)
                how = "patched (frame 1 %.0f%% like today's rules after shift %d,%d)%s" % (
                    100 * m, dx, dy, "  <-- CHECK" if m < 0.85 else "")
            data = base[:0x480] + ws.encode(f2s, 64, 48)
            big1 = base1
        else:
            # a form / gender without its own reviewed file: all new, small sprite of the base
            small = base[:0x180] if base else bytes(0x180)
            data = small + ws.encode(c1, 64, 48) + ws.encode(c2, 64, 48)
            f2s, big1 = c2, c1
            how = "new (no reviewed file for this form; small sprite = #%03d's)" % num if base else "new (no base file)"
        assert len(data) == 0x780
        if base and k == owner:
            assert data[:0x480] == base[:0x480], k
        open(os.path.join(out, k + ".bin"), "wb").write(data)
        report.append("%s frame2=f%d: %s" % (k, kf[k]["key2"], how))
        rows.append((k, big1, f2s, ws.decode(data[0:0xC0], 32, 24)))

open(os.path.join(out, "report.txt"), "w").write("\n".join(report) + "\n")
print("\n".join(r for r in report if "exact" not in r))
print("files:", len(rows), " exact:", sum("exact" in r for r in report))
S, per_row, per_page = 2, 4, 48
cw, ch = 64 * S * 2 + 32 * S + 30, 48 * S + 18
for p in range(0, len(rows), per_page):
    chunk = rows[p:p + per_page]
    sh = Image.new("RGB", (per_row * cw, ((len(chunk) + per_row - 1) // per_row) * ch), (200, 220, 255))
    d = ImageDraw.Draw(sh)
    for i, (k, a, b, s) in enumerate(chunk):
        x, y = (i % per_row) * cw + 4, (i // per_row) * ch + 14
        d.text((x, y - 12), "#%s  large 1 | large 2 | small" % k, fill="black")
        for im in (a, b):
            sh.paste(ws.preview(im, S).convert("RGB"), (x, y)); x += 64 * S + 6
        sh.paste(ws.preview(s, S).convert("RGB"), (x, y))
    sh.save(os.path.join(out, "preview_%d.png" % (p // per_page + 1)))
