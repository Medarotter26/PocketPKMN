#!/usr/bin/env python3
"""Second key frame for Gen 5 walker sprites (user, 2026-11-15: "The ones in red either
don't move enough from their 1st pose or the pose chosen for frame 2 looks bad. Try to
find better options" - the red DOTS on their review sheet; plus "Keldeo Resolute Black
and White Kyurem too and the Therian forms" from Bulbagarden's B2W2 category).

Source: Bulbagarden's animated Black/White sprites "Spr 5b NNN.png" (APNG; forms and
genders "Spr 5b 521 m.png", "Spr 5b 585A.png"; the B2W2-only forms "Spr 5b2 641T.png").
Frame 1 = the animation's first frame (the standing pose). Frame 2 options: frames that
move clearly away from frame 1 but are NOT squashed / stretched (the outline box stays
near frame 1's size, no blended new colours), spread so that the options differ.

  walker_gen5_keys.py fetch <dir> <key...>     -> <dir>/anim/<key>.png  (1 request/s)
  walker_gen5_keys.py pick <dir> <key...>      -> <dir>/options.json + <dir>/options_N.png sheets
  walker_gen5_keys.py save <dir> <key>=<frame> ... -> <dir>/keys/<key>_1.png, <key>_2.png, keyframes.json
Keys: "494", "521_m", "550B", "585A", "641T" ..."""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws


def file_name(key):
    if key[-1] in "TR" and key[:3] in ("641", "642", "645", "647") or key in ("646B", "646W"):
        return "Spr 5b2 %s.png" % key
    if "_" in key:
        n, g = key.split("_")
        return "Spr 5b %s %s.png" % (n, g)
    return "Spr 5b %s.png" % key


def fetch(d, keys):
    os.makedirs(os.path.join(d, "anim"), exist_ok=True)
    for k in keys:
        out = os.path.join(d, "anim", k + ".png")
        if os.path.exists(out):
            continue
        url = ws.fetch_one([ws.bulba_url(file_name(k))], out)
        print(k, file_name(k), "ok" if url else "MISSING")


def frames_of(d, k):
    from PIL import Image
    ws.first_frame(os.path.join(d, "anim", k + ".png"))      # PIL limits
    im = Image.open(os.path.join(d, "anim", k + ".png"))
    out = []
    for i in range(getattr(im, "n_frames", 1)):
        im.seek(i)
        out.append(im.convert("RGBA"))
    return out


def stats(f0, f):
    """change: share of the Pokemon's pixels that differ from frame 1; size: outline-box
    change (squash / stretch); newc: share of pixels in colours frame 1 does not have."""
    a0, a = f0.load(), f.load()
    w, h = f0.size
    b0 = f0.getchannel("A").getbbox() or (0, 0, 1, 1)
    b = f.getchannel("A").getbbox() or (0, 0, 1, 1)
    pal = set()
    union = diff = body = newc = 0
    for y in range(h):
        for x in range(w):
            if a0[x, y][3] >= 128:
                pal.add(a0[x, y][:3])
    for y in range(h):
        for x in range(w):
            p, q = a0[x, y], a[x, y]
            o0, o1 = p[3] >= 128, q[3] >= 128
            if o0 or o1:
                union += 1
                if o0 != o1 or sum(abs(p[i] - q[i]) for i in range(3)) > 30:
                    diff += 1
            if o1:
                body += 1
                if q[:3] not in pal:
                    newc += 1
    w0, h0 = b0[2] - b0[0], b0[3] - b0[1]
    w1, h1 = b[2] - b[0], b[3] - b[1]
    size = abs(w1 - w0) / max(w0, 1) + abs(h1 - h0) / max(h0, 1)
    return diff / max(union, 1), size, newc / max(body, 1)


def arr(f):
    import numpy as np
    a = np.asarray(f, dtype=np.int16)
    return a[..., :3], a[..., 3] >= 128


def pose_change(f0, f, shift=6):
    """Share of the Pokemon's pixels that differ from f0 AFTER lining the two up (the best
    shift within +-shift px): moving parts count, the whole sprite bobbing does not."""
    import numpy as np
    c0, m0 = arr(f0)
    c1, m1 = arr(f)
    h, w = m0.shape
    best = 1.0
    for dy in range(-shift, shift + 1, 2 if shift > 3 else 1):
        for dx in range(-shift, shift + 1, 2 if shift > 3 else 1):
            m = np.zeros_like(m1); c = np.zeros_like(c1)
            ys, yd = (slice(0, h - dy), slice(dy, h)) if dy >= 0 else (slice(-dy, h), slice(0, h + dy))
            xs, xd = (slice(0, w - dx), slice(dx, w)) if dx >= 0 else (slice(-dx, w), slice(0, w + dx))
            m[yd, xd] = m1[ys, xs]; c[yd, xd] = c1[ys, xs]
            union = (m0 | m).sum()
            if not union:
                continue
            diff = ((m0 != m) | (m0 & m & (np.abs(c0 - c).sum(-1) > 30))).sum()
            best = min(best, diff / union)
    return best


def hist(f):
    """Share of the Pokemon's pixels per colour (rounded to 32 levels)."""
    from collections import Counter
    px = f.load()
    c = Counter(tuple(v // 32 for v in px[x, y][:3]) for y in range(f.height) for x in range(f.width) if px[x, y][3] >= 128)
    t = sum(c.values()) or 1
    return {k: v / t for k, v in c.items()}


def hist_dist(a, b):
    return sum(abs(a.get(k, 0) - b.get(k, 0)) for k in set(a) | set(b)) / 2


def pick(d, keys):
    from PIL import Image, ImageDraw
    res = {}
    for k in keys:
        fr = frames_of(d, k)
        n = len(fr)
        cand = []
        h0 = hist(fr[0])
        for i in range(1, n):
            ch, sz, nc = stats(fr[0], fr[i])
            pc = pose_change(fr[0], fr[i])
            hd = hist_dist(h0, hist(fr[i]))
            if hd > 0.35:
                continue    # a colour flash (Zebstrika's lightning, glows): not a pose
            # good = the pose changes clearly, no squash / stretch, no blended colours
            cand.append((pc - 2.0 * max(0.0, sz - 0.06) - 3.0 * nc - 2.0 * max(0.0, hd - 0.15), i, pc, sz, nc))
        cand.sort(reverse=True)
        chosen = []
        gap = max(3, n // 10)
        # frames the user has already been shown and passed over (earlier rounds): not again,
        # nor their near neighbours
        seen = [o["frame"] for o in SEEN.get(k, [])]
        for sc, i, pc, sz, nc in cand:
            if any(abs(i - s) < max(2, gap // 2) for s in seen):
                continue
            if all(abs(i - j["frame"]) >= gap and pose_change(fr[j["frame"]], fr[i], 3) >= 0.10 for j in chosen):
                chosen.append({"frame": i, "pose": round(pc, 3), "size": round(sz, 3), "newcol": round(nc, 3)})
            if len(chosen) == 3:
                break
        res[k] = {"frames": n, "options": chosen}
        print(k, n, [(o["frame"], o["pose"], o["size"]) for o in chosen])
    json.dump(res, open(os.path.join(d, "options.json"), "w"), indent=1)
    # sheets: per row frame 1 | A | B | C, colour, 2x, 4 Pokemon per row
    S, per_row, rows_pp = 1, 4, 10
    cell = 96 * 4 + 50
    keys = list(res)
    for p in range(0, len(keys), per_row * rows_pp):
        chunk = keys[p:p + per_row * rows_pp]
        nrow = (len(chunk) + per_row - 1) // per_row
        sh = Image.new("RGB", (per_row * cell, nrow * 118), "white")
        dr = ImageDraw.Draw(sh)
        for j, k in enumerate(chunk):
            fr = frames_of(d, k)
            x, y = (j % per_row) * cell + 6, (j // per_row) * 118
            dr.rectangle((x - 6, y, x - 6 + cell - 2, y + 116), outline=(180, 180, 180))
            dr.text((x, y + 2), "#%s (%d frames)" % (k, res[k]["frames"]), fill="black")
            for t, (lab, i) in enumerate([("1", 0)] + [("ABC"[n], o["frame"]) for n, o in enumerate(res[k]["options"])]):
                f = fr[i]
                bg = Image.new("RGB", f.size, (236, 236, 236))
                bg.paste(f, mask=f.getchannel("A"))
                if max(bg.size) > 96:     # big ones: whole pixels only (nearest), no blur
                    s = 96 / max(bg.size)
                    bg = bg.resize((round(bg.width * s), round(bg.height * s)), Image.NEAREST)
                sh.paste(bg, (x + t * 98 + (96 - bg.width) // 2, y + 16 + (96 - bg.height)))
                dr.text((x + t * 98 + 2, y + 16), lab if i == 0 else "%s f%d" % (lab, i), fill=(200, 0, 0) if i else "black")
        fn = os.path.join(d, "options_%d.png" % (p // (per_row * rows_pp) + 1))
        sh.save(fn)
        print("sheet", fn)


def save(d, pairs):
    os.makedirs(os.path.join(d, "keys"), exist_ok=True)
    jp = os.path.join(d, "keys", "keyframes.json")
    kf = json.load(open(jp)) if os.path.exists(jp) else {}
    for pr in pairs:
        k, i = pr.split("=")
        fr = frames_of(d, k)
        fr[0].save(os.path.join(d, "keys", k + "_1.png"))
        fr[int(i)].save(os.path.join(d, "keys", k + "_2.png"))
        ch, sz, nc = stats(fr[0], fr[int(i)])
        kf[k] = {"frames": len(fr), "key1": 0, "key2": int(i), "change": round(ch, 4), "size": round(sz, 4)}
    json.dump(kf, open(jp, "w"), indent=1, sort_keys=True)


SEEN = {}

if __name__ == "__main__":
    cmd, d, rest = sys.argv[1], sys.argv[2], sys.argv[3:]
    # WBX_SEEN=<options.json of an earlier round>: its options are not offered again
    if os.environ.get("WBX_SEEN"):
        for p in os.environ["WBX_SEEN"].split(":"):
            for k, v in json.load(open(p)).items():
                SEEN.setdefault(k, []).extend(v["options"])
    {"fetch": fetch, "pick": pick, "save": save}[cmd](d, rest)
