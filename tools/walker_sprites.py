#!/usr/bin/env python3
"""Walker sprites for species HGSS never had (user, 2026-11-14):
  Gen 5  -> the FIRST FRAME of the Black/White sprite (Bulbagarden Archives,
            Category:Black_and_White_sprites, files "Spr 5b NNN.png")
  Gen 6/7 -> the "Front" sprite of Smogon's Gen6 / Gen7 Sprite Project sheets
            (BW-style fan sprites; spriter credits kept in credits.csv)
Each becomes the walker's two pictures: small 32x24 and large 64x48, 2 frames,
4 shades (walker 2bpp, include/pw_image.h).

Usage:
  walker_sprites.py fetch  <cache_dir> <species...|all>   download sources (1 request / s)
  walker_sprites.py convert <cache_dir> <out_dir> [mode]  make .bin + preview PNGs
  walker_sprites.py ref <eeprom.bin> <out.png>            decode the walker's own sprites
Sources stay in <cache_dir> on the user's PC; they are not part of the app.
"""
import csv, hashlib, io, os, sys, time, urllib.request

UA = {"User-Agent": "Mozilla/5.0 (WalkerBox sprite tool)"}
MENU = os.environ.get("WBX_MENU") == "1"   # Gen 6/7 from the SM/USUM menu sprites instead of Smogon
SHEETS = {
    6: "https://docs.google.com/spreadsheets/d/1Gn0UORn-unvcbUeQhQdEBz0ADNcH49BZZqQ1dpXm9eo/export?format=csv&gid=0",
    7: "https://docs.google.com/spreadsheets/d/1FMcHbSKEWZc7v2Ur4cyJjT_NhO0gqXyU9kDhsOQhlBQ/export?format=csv",
}


def get(url):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=30) as r:
        return r.read()


def bulba_url(name):
    # MediaWiki file path: /media/upload/<md5[0]>/<md5[0:2]>/<File_name>
    fn = name.replace(" ", "_")
    h = hashlib.md5(fn.encode()).hexdigest()
    return "https://archives.bulbagarden.net/media/upload/%s/%s/%s" % (h[0], h[:2], fn)


def sheet_rows(gen, cache):
    path = os.path.join(cache, "sheet_gen%d.csv" % gen)
    if not os.path.exists(path):
        open(path, "wb").write(get(SHEETS[gen]))
    rows = {}
    for r in csv.reader(open(path, encoding="utf-8")):
        if len(r) > 6 and r[0].strip().isdigit() and r[2].startswith("http"):
            num = int(r[0])
            rows.setdefault(num, {"name": r[1], "front": r[2], "spriter": r[6]})  # first row = base form
    return rows


def sources(num, cache):
    """Candidate URLs (first that exists wins) for the LARGE and the SMALL picture.
    User, 2026-11-14:
      large Gen 5: first frame of the Black/White sprite ("Spr 5b NNN.png", gendered
                   ones "... m.png");
      large Gen 6/7: Smogon sprite project "Front" (or the SM/USUM menu icon if WBX_MENU=1);
      small Gen 5: "for gen 5 pokemon, use these menu sprites" - Generation V menu
                   sprites, the animated ones ("AniNNNMS.png", 2 frames) else "NNNMS3.png";
      small Gen 6/7: "for gens 6/7 can we use their menu sprites?" - SM/USUM "NNNMS6.png"."""
    credit = "Game Freak"
    if 494 <= num <= 649:
        large = [bulba_url("Spr 5b %03d.png" % num), bulba_url("Spr 5b %03d m.png" % num)]
        small = [bulba_url("Ani%03dMS.png" % num), bulba_url("%03dMS3.png" % num)]
    else:
        small = [bulba_url("%03dMS6.png" % num)]
        if MENU:
            large = small
        else:
            row = sheet_rows(6 if num <= 721 else 7, cache).get(num)
            large = [row["front"]] if row else []
            credit = row["spriter"] if row else ""
    return large, small, credit


def fetch_one(urls, out):
    for url in urls:
        try:
            data = get(url)
        except Exception:
            data = b""
        time.sleep(1.0)
        if data.startswith(b"\x89PNG"):
            open(out, "wb").write(data)
            return url
    return None


def fetch(cache, species):
    for sub in ("large", "small"):
        os.makedirs(os.path.join(cache, sub), exist_ok=True)
    credits = os.path.join(cache, "credits.csv")
    for num in species:
        large, small, credit = sources(num, cache)
        got = []
        for sub, urls in (("large", large), ("small", small)):
            out = os.path.join(cache, sub, "%03d.png" % num)
            if os.path.exists(out):
                continue
            url = fetch_one(urls, out)
            got.append("%s %s" % (sub, "ok" if url else "MISSING"))
            if url:
                with open(credits, "a", encoding="utf-8") as c:
                    c.write("%d,%s,%s,%s\n" % (num, sub, url, credit.replace(",", ";")))
        if got:
            print("%03d: %s" % (num, ", ".join(got)))


# ---- conversion --------------------------------------------------------------

def first_frame(path):
    from PIL import Image, ImageFile, PngImagePlugin
    # Some fan sprites carry large text chunks (EXIF in zTXt); PIL refuses those by default.
    PngImagePlugin.MAX_TEXT_CHUNK = 64 << 20
    PngImagePlugin.MAX_TEXT_MEMORY = 256 << 20
    ImageFile.LOAD_TRUNCATED_IMAGES = True
    im = Image.open(path)
    im.seek(0)                      # APNG: the first frame
    im = im.convert("RGBA")
    # Some Smogon Gen 7 links are sheets of 96x96 tiles: front | back, and a shiny
    # row below (Pikipek, Toucannon, Grubbin, Bewear ... 192x96 / 192x192). The
    # front, normal colours, is the top-left tile.
    if im.width >= 192 and im.width % 96 == 0 and im.height % 96 == 0:
        im = im.crop((0, 0, 96, 96))
    return im


# Brightness -> walker shade, as the stock walker art does it (compared with the
# HGSS sprites, art_src/walker_ref): light colours white, mid light grey, orange /
# blue / teal dark grey, the darkest and the outlines black. Stock files use the
# greys 255 / 168 / 80 / 0.
# Fitted to 9 uncropped HGSS / stock walker pairs (tools/walker_fit.py, 2026-11-14).
THRESH = [float(v) for v in os.environ.get("WBX_THRESH", "205,140,80").split(",")]


def walker_shade(lum, t=None):
    t = t or THRESH
    return 0 if lum >= t[0] else 1 if lum >= t[1] else 2 if lum >= t[2] else 3


# Per-species crop corrections for big Pokemon, "dex dx dy" (pixels the 64x48 window
# moves right / down from the default), in tools/walker_crops.txt.
def crop_override(num, w=64):
    """"dex dx dy" (large picture) or "dex small dx dy" (small one): the window moves
    dx right / dy down, i.e. the Pokemon moves left / up in its picture."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_crops.txt")
    if num is None or not os.path.exists(path):
        return 0, 0
    for line in open(path):
        p = line.split("#")[0].split()
        if w == 64 and len(p) == 3 and int(p[0]) == num:
            return int(p[1]), int(p[2])
        if w == 32 and len(p) == 4 and int(p[0]) == num and p[1] == "small":
            return int(p[2]), int(p[3])
    return 0, 0


def paints(num, w):
    """"dex paint x0 y0 x1 y1 shade" (large picture): inside the rectangle, every
    pixel of the Pokemon LIGHTER than shade becomes that shade (Pignite: the white
    specks and the light band at the bottom of its arms -> dark grey)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_colours.txt")
    out = []
    if num is None or w != 64 or not os.path.exists(path):
        return out
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 7 and int(p[0]) == num and p[1] == "paint":
            out.append(tuple(int(v) for v in p[2:]))
    return out


def erases(num, w):
    """"dex erase x0 y0 x1 y1" (large) / "dex erase-small ...": a rectangle of the
    picture made empty (user: Serperior "erase the tail", stray pixels)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_colours.txt")
    out = []
    if num is None or not os.path.exists(path):
        return out
    key = "erase" if w == 64 else "erase-small"
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 6 and int(p[0]) == num and p[1] == key:
            out.append(tuple(int(v) for v in p[2:]))
    return out


def colour_overrides(num):
    """{(r, g, b): shade} for one Pokemon from tools/walker_colours.txt
    ("dex RRGGBB shade", shade 0 white .. 3 black): fixed by hand where the rules
    do not get it right (user review 2026-11-14)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_colours.txt")
    out = {}
    if num is None or not os.path.exists(path):
        return out
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 3 and int(p[0]) == num:
            h = p[1]
            out[(int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))] = int(p[2])
    return out


def small_overrides(num):
    """{(r, g, b): shade} for the SMALL sprite (menu icon) from walker_colours.txt
    lines "dex small RRGGBB shade". An icon with such lines gets no automatic detail
    rule (on the icons' fine shading it made speckle)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_colours.txt")
    out = {}
    if num is None or not os.path.exists(path):
        return out
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 4 and int(p[0]) == num and p[1] == "small":
            h = p[2]
            out[(int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))] = int(p[3])
    return out


def fills(num):
    """[(x, y, shade)] from tools/walker_colours.txt lines "dex fill x y shade":
    an enclosed gap of background in the LARGE picture (64x48 coordinates) filled
    with a shade (Patrat: the gap beside the belly)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_colours.txt")
    out = []
    if num is None or not os.path.exists(path):
        return out
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 5 and int(p[0]) == num and p[1] == "fill":
            out.append((int(p[2]), int(p[3]), int(p[4])))
    return out


def apply_fills(out, body, w, h, num):
    for fx, fy, shade in fills(num):
        if not (0 <= fx < w and 0 <= fy < h) or body[fy][fx]:
            continue
        seen, stack, area, leak = set(), [(fx, fy)], [], False
        while stack and not leak:
            x, y = stack.pop()
            if (x, y) in seen:
                continue
            seen.add((x, y))
            if x in (0, w - 1) or y in (0, h - 1) or len(area) > 80:
                leak = True     # not an enclosed gap: leave it
                break
            area.append((x, y))
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if not body[ny][nx] and (nx, ny) not in seen:
                    stack.append((nx, ny))
        if not leak:
            for x, y in area:
                out[y][x] = shade


def head_point(num):
    """(x, y) of the face from tools/walker_heads.txt, or None."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_heads.txt")
    if num is None or not os.path.exists(path):
        return None
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 3 and int(p[0]) == num:
            return int(p[1]), int(p[2])
    return None


def lum_of(p):
    return 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2]


_LAB = {}


def lab_of(p):
    """CIE L*a*b* (D65) of an sRGB colour; perceived colour difference = distance."""
    key = tuple(p[:3])
    if key in _LAB:
        return _LAB[key]
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(float(v)) for v in key)
    X = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    Y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    Z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883
    f = lambda t: t ** (1 / 3) if t > 216 / 24389 else (24389 / 27 * t + 16) / 116
    fx, fy, fz = f(X), f(Y), f(Z)
    _LAB[key] = (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))
    return _LAB[key]


def delta_e(p, q):
    a, b = lab_of(p), lab_of(q)
    return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2) ** 0.5


def dark_gamma(im, t=None):
    t = t or THRESH
    """In the stock walker art black never covers more than ~38% of a Pokemon
    (art_src/walker_ref). A dark Pokemon (Zoroark: 82% black) gets its dark tones
    lifted (gamma < 1) until black is back under that; outlines, much darker than
    the body, stay black."""
    px = im.load()
    lums = [lum_of(px[x, y]) for y in range(im.height) for x in range(im.width) if px[x, y][3] >= 128]
    if not lums:
        return 1.0
    g = 1.0
    while g > 0.35:
        black = sum(1 for l in lums if 255 * (l / 255) ** g < t[2]) / len(lums)
        if black <= 0.38:
            break
        g -= 0.05
    return g


# 1.0 = never shrink. (Shrinking big Pokemon to fit was tried and REJECTED by the
# user, 2026-11-14: "why are we resizing??? We just had bad crops to fix" - sprites
# stay at full size, as the stock walker art; only the crop window is chosen better.)
MIN_SCALE = float(os.environ.get("WBX_MIN_SCALE", "1.0"))


def shrink(im, nw, nh):
    """Shrink an RGBA sprite without dark/transparent fringes: colours are averaged
    weighted by alpha (premultiplied), alpha separately."""
    from PIL import Image
    pre = Image.new("RGBA", im.size)
    px, pp = im.load(), pre.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            pp[x, y] = (r * a // 255, g * a // 255, b * a // 255, a)
    small = pre.resize((nw, nh), Image.BOX)
    sp = small.load()
    for y in range(nh):
        for x in range(nw):
            r, g, b, a = sp[x, y]
            if a:
                sp[x, y] = (min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a), a)
    return small


def detail_window(im, w, h, x0, y0):
    """Where to put the w x h window on a sprite still bigger than it: where the most
    is drawn (brightness steps between neighbours: eyes, mouth, markings - the face
    has the most), slightly favouring the top. Starts from the default (x0, y0) on ties."""
    sw, sh = im.size
    px = im.load()
    e = [[0.0] * sw for _ in range(sh)]
    for y in range(sh):
        for x in range(sw):
            if px[x, y][3] < 128:
                continue
            l = lum_of(px[x, y])
            v = 0.0
            for dx, dy in ((1, 0), (0, 1)):
                if x + dx < sw and y + dy < sh and px[x + dx, y + dy][3] >= 128:
                    v += abs(l - lum_of(px[x + dx, y + dy]))
            e[y][x] = v * (1.0 - 0.4 * y / max(1, sh)) + 8.0   # + body: prefer covering the Pokemon
    # summed-area table
    S = [[0.0] * (sw + 1) for _ in range(sh + 1)]
    for y in range(sh):
        row = 0.0
        for x in range(sw):
            row += e[y][x]
            S[y + 1][x + 1] = S[y][x + 1] + row
    best = None
    xs = range(0, sw - w + 1) if sw > w else [x0]
    ys = range(0, sh - h + 1) if sh > h else [y0]
    for yy in ys:
        for xx in xs:
            ax, ay = max(xx, 0), max(yy, 0)
            bx, by = min(xx + w, sw), min(yy + h, sh)
            v = S[by][bx] - S[ay][bx] - S[by][ax] + S[ay][ax]
            if best is None or v > best[0] + 1e-6 or (abs(v - best[0]) <= 1e-6 and (xx, yy) == (x0, y0)):
                best = (v, xx, yy)
    return best[1], best[2]


def drop_fragments(out, body, w, h):
    """User (Malamar): "sprites that have stray pixels of cut off body elements, those
    should just be erased". Pieces of the Pokemon that are separate from the rest in
    the picture (8-connected) and either touch the picture's edge and are small (< 12%
    of the biggest piece) or are tiny anywhere (< 5 pixels) are erased."""
    seen = [[False] * w for _ in range(h)]
    comps = []
    for y in range(h):
        for x in range(w):
            if body[y][x] and not seen[y][x]:
                stack, comp, edge = [(x, y)], [], False
                seen[y][x] = True
                while stack:
                    cx, cy = stack.pop()
                    comp.append((cx, cy))
                    if cx in (0, w - 1) or cy in (0, h - 1):
                        edge = True
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            nx, ny = cx + dx, cy + dy
                            if 0 <= nx < w and 0 <= ny < h and body[ny][nx] and not seen[ny][nx]:
                                seen[ny][nx] = True
                                stack.append((nx, ny))
                comps.append((comp, edge))
    if not comps:
        return
    biggest = max(len(c) for c, _ in comps)
    for comp, edge in comps:
        if len(comp) < 5 or (edge and len(comp) < 0.12 * biggest):
            for cx, cy in comp:
                out[cy][cx] = 0
                body[cy][cx] = False


def to_walker_frames(frames, w, h, num=None):
    """Stock walker style for one or more frames: the sprite at its own pixel size
    (no scaling), mirrored (stock walker sprites face right, DS sprites face left).
    Bigger than w x h: cropped, keeping the top and the FACING side (after mirroring
    the right side, where the head of a big left-facing DS sprite is), so big
    Pokemon run off the edges as on the real walker. All frames share one window,
    so an animation's movement stays."""
    from PIL import ImageOps
    frames = [ImageOps.mirror(f) for f in frames]
    bbox = None
    for f in frames:
        b = f.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if b:
            bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
    if bbox:
        frames = [f.crop(bbox) for f in frames]
    sw, sh = frames[0].size
    # Big Pokemon (user review 1: ~20 "odd" crops - the BW / Smogon sprites are drawn
    # larger than the HGSS ones, so cropping at 1:1 left only a random piece): the
    # LARGE picture is shrunk just enough to fit, but never below MIN_SCALE; the
    # small icons stay pixel for pixel.
    step = DETAIL_STEP
    if w == 64 and (sw > w or sh > h) and MIN_SCALE < 1.0:
        s = max(MIN_SCALE, min(w / sw, h / sh))
        nw, nh = max(1, round(sw * s)), max(1, round(sh * s))
        frames = [shrink(f, nw, nh) for f in frames]
        sw, sh = nw, nh
        # Shrinking blends neighbouring colours; only clear steps count as lines then
        # (at 40 the blends turned into speckles: Yveltal, Carracosta, Liepard).
        step = DETAIL_STEP_SHRUNK
    x0 = -(w - sw) // 2 if sw <= w else sw - w   # fits: centred; else the facing (right) side
    y0 = -(h - sh) if sh <= h else 0             # fits: on the bottom; else the top
    touch = review_mode(num) == "touch"
    if touch:
        # "touch": the reviewed version exactly (this window, original colours, no
        # stray-bit erase), plus only the tweaks listed for this Pokemon. The
        # walker_crops.txt shift moves the picture also when the Pokemon fits
        # (user: "raise a little", "centre it").
        dx, dy = crop_override(num, w)
        x0 = min(max(x0 + dx, 0), sw - w) if sw > w else x0 + dx
        y0 = min(max(y0 + dy, 0), sh - h) if sh > h else y0 + dy
    elif w == 64 and (sw > w or sh > h):
        head = head_point(num)
        if head:
            # Marked by hand: the face in the middle of the picture (user: "they
            # should be cropped so that the face is mostly visible", stock Venusaur).
            if sw > w:
                x0 = head[0] - w // 2
            if sh > h:
                y0 = head[1] - h // 2
        elif sw > w:
            x0 = (sw - w) // 2   # only a little too big: centred, the top kept (stock walker)
    if not touch:
        dx, dy = crop_override(num, w)
        if sw > w:
            x0 = min(max(x0 + dx, 0), sw - w)
        if sh > h:
            y0 = min(max(y0 + dy, 0), sh - h)
    # Colour review (user: "all of those left have coloring issues" - Roggenrola
    # turned white, Cosmog / Venipede black blobs, Patrat's / Gothita's / Marshadow's
    # eyes gone, Deerling's pink gone, Pansage too dark):
    #  - BLACK IS FOR LINES: a body colour never becomes black, however dark (stock:
    #    Gengar's body dark grey); only near-black pixels (LINE_LUM) and the silhouette
    #    edge are black. Replaces the earlier gamma lift of dark Pokemon.
    #  - colours that differ clearly but got the same grey are told apart: whole
    #    colour areas (>= 5% of the Pokemon, perceived difference >= REGION_DE) -> the
    #    lighter one a shade lighter; neighbouring pixels (eyes, markings, perceived
    #    difference >= DETAIL_DE) -> the darker one a shade darker (see below).
    p0 = frames[0].load()
    from collections import Counter
    cnt = Counter(p0[x, y][:3] for y in range(sh) for x in range(sw) if p0[x, y][3] >= 128)
    total = sum(cnt.values()) or 1
    mode = review_mode(num)
    use_de = mode == "colour"
    if mode != "colour":
        # ORIGINAL colour rules (the state the user reviewed, out_v1): the user's
        # rule (2026-11-14) is that only Pokemon they flagged may change; flagged
        # for crop / stray bits only, they keep these colours.
        g = dark_gamma(frames[0], THRESH_V1)
        lums = sorted(lum_of(p0[x, y]) for y in range(sh) for x in range(sw) if p0[x, y][3] >= 128)
        line_max = lums[int(len(lums) * 0.15)] + 1e-9 if (g < 1.0 and lums) else -1
        def base_v1(c):
            return walker_shade(255 * (lum_of(c) / 255) ** g, THRESH_V1)
        groups = Counter()
        for c, n in cnt.items():
            groups[tuple(v // 48 for v in c)] += n
        big = [k for k, n in groups.items() if n / total >= 0.08]
        bump = set()
        for i, a in enumerate(big):
            for b in big[i + 1:]:
                ca = tuple(v * 48 + 24 for v in a)
                cb = tuple(v * 48 + 24 for v in b)
                if base_v1(ca) == base_v1(cb) and sum(abs(x - y) for x, y in zip(ca, cb)) > 120:
                    bump.add(a if max(ca) > max(cb) else b)
        fixed = colour_overrides(num)
        def shade_for(c, l):
            if tuple(c) in fixed:
                return fixed[tuple(c)]
            s = walker_shade(255 * (l / 255) ** g, THRESH_V1)
            if tuple(v // 48 for v in c) in bump and s > 0:
                s -= 1
            return s
        return render_frames(frames, w, h, x0, y0, sw, sh, shade_for, line_max, fixed, step, use_de, num)
    def base_shade(c):
        s = walker_shade(lum_of(c))
        return 2 if s == 3 and lum_of(c) >= LINE_LUM else s
    # Colour areas: colours grouped by hue ramp (rounded); each group's main colour.
    groups, main = Counter(), {}
    for c, n in cnt.most_common():
        k = tuple(v // 48 for v in c)
        groups[k] += n
        main.setdefault(k, c)
    big = [k for k, n in groups.items() if n / total >= 0.05]
    lighter, darker = set(), set()
    for i, a in enumerate(big):
        for b in big[i + 1:]:
            ca, cb = main[a], main[b]
            if base_shade(ca) == base_shade(cb) and delta_e(ca, cb) >= REGION_DE:
                hi, lo = (a, b) if lum_of(ca) >= lum_of(cb) else (b, a)
                # The darker colour goes a shade darker (light colours stay light, as
                # in the stock art: Chespin's hood stays light, its brown goes dark);
                # only if it is already dark grey does the lighter one go lighter.
                if base_shade(ca) < 2:
                    darker.add(lo)
                else:
                    lighter.add(hi)
    fixed = colour_overrides(num)
    def shade_for(c, l):
        if tuple(c) in fixed:
            return fixed[tuple(c)]
        s = base_shade(c)
        k = tuple(v // 48 for v in c)
        if k in lighter and s > 0:
            s -= 1
        elif k in darker and s < 2:
            s += 1
        return s
    line_max = LINE_LUM - 1e-9
    return render_frames(frames, w, h, x0, y0, sw, sh, shade_for, line_max, fixed, step, use_de, num)


def render_frames(frames, w, h, x0, y0, sw, sh, shade_for, line_max, fixed, step, use_de, num):
    outs = []
    for f in frames:
        px = f.load()
        opaque = lambda sx, sy: 0 <= sx < sw and 0 <= sy < sh and px[sx, sy][3] >= 128
        out = [[0] * w for _ in range(h)]
        for y in range(h):
            for x in range(w):
                sx, sy = x + x0, y + y0
                if not opaque(sx, sy):
                    continue
                l = lum_of(px[sx, sy])
                edge = not all(opaque(sx + dx, sy + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                if edge or l < line_max:
                    out[y][x] = 3
                else:
                    out[y][x] = shade_for(px[sx, sy][:3], l)
        # Small details (user: "you can't see chespin's smile"): where the source has
        # a clear step in brightness between two neighbouring pixels (a drawn line, a
        # mouth, a marking) but both got the same shade, the darker one goes one shade
        # darker, so the line the colours show is still there in grey.
        base = [row[:] for row in out]
        for y in range(h):
            for x in range(w):
                sx, sy = x + x0, y + y0
                if not opaque(sx, sy) or base[y][x] == 3 or tuple(px[sx, sy][:3]) in fixed:
                    continue        # colours fixed by hand stay as given
                l = lum_of(px[sx, sy])
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if not (0 <= nx < w and 0 <= ny < h and opaque(sx + dx, sy + dy) and base[ny][nx] == base[y][x]):
                        continue
                    q = px[sx + dx, sy + dy]
                    # this pixel is the darker of a pair that differs clearly in
                    # brightness, or in colour (eyes of another hue: Gothita, Patrat)
                    if lum_of(q) - l >= step or (use_de and lum_of(q) >= l and delta_e(q, px[sx, sy]) >= DETAIL_DE):
                        out[y][x] = base[y][x] + 1
                        break
        body = [[opaque(x + x0, y + y0) for x in range(w)] for y in range(h)]
        if review_mode(num) != "touch":
            drop_fragments(out, body, w, h)
        for ex0, ey0, ex1, ey1 in erases(num, w):
            for yy in range(max(0, ey0), min(h - 1, ey1) + 1):
                for xx in range(max(0, ex0), min(w - 1, ex1) + 1):
                    out[yy][xx] = 0
                    body[yy][xx] = False
        for px0, py0, px1, py1, to in paints(num, w):
            for yy in range(max(0, py0), min(h - 1, py1) + 1):
                for xx in range(max(0, px0), min(w - 1, px1) + 1):
                    if body[yy][xx] and out[yy][xx] < to:
                        out[yy][xx] = to
        if w == 64:
            apply_fills(out, body, w, h, num)
        outs.append(out)
    return outs


# The rules the user reviewed (out_v1), kept for every Pokemon not flagged for colour.
THRESH_V1 = [220.0, 150.0, 95.0]


def review_mode(num):
    """tools/walker_review.txt: "dex crop" (flagged for crop / stray bits: new crop,
    original colours) or "dex colour" (flagged for colour: new colour rules). Not
    listed: the reviewed version is kept unchanged (convert() copies it)."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_review.txt")
    if num is None or not os.path.exists(path):
        return None
    for line in open(path):
        p = line.split("#")[0].split()
        if len(p) == 2 and int(p[0]) == num:
            return p[1]
    return None


# A brightness step of at least this between neighbours counts as a drawn line.
DETAIL_STEP = float(os.environ.get("WBX_DETAIL", "40"))
DETAIL_STEP_SHRUNK = float(os.environ.get("WBX_DETAIL_SHRUNK", "70"))
# Perceived colour difference (CIE76) for "different colour" between neighbours / areas.
DETAIL_DE = float(os.environ.get("WBX_DETAIL_DE", "35"))
REGION_DE = float(os.environ.get("WBX_REGION_DE", "25"))
# Darker than this (luma) counts as a drawn line and may be black.
LINE_LUM = float(os.environ.get("WBX_LINE_LUM", "45"))


def to_walker(im, w, h, num=None):
    return to_walker_frames([im], w, h, num)[0]


def all_frames(path):
    """Every frame of an APNG (icons have 2), else the one image."""
    from PIL import Image
    first_frame(path)            # sets the PIL limits
    im = Image.open(path)
    out = []
    for i in range(getattr(im, "n_frames", 1)):
        im.seek(i)
        out.append(im.convert("RGBA"))
    return out


def to_shades(im, w, h, mode):
    """RGBA sprite -> list of rows of shades 0..3 (0 white ... 3 black), fitted into w x h."""
    from PIL import Image
    if mode in ("walker", "icon"):
        return to_walker(im, w, h)
    bbox = im.getchannel("A").point(lambda a: 255 if a > 0 else 0).getbbox()
    if bbox:
        im = im.crop(bbox)
    sw, sh = im.size
    scale = min(w / sw, h / sh, 1.0 if mode == "nearest" else 10)
    nw, nh = max(1, round(sw * scale)), max(1, round(sh * scale))
    rs = Image.NEAREST if mode == "nearest" else Image.BOX
    small = im.resize((nw, nh), rs)
    canvas = [[0] * w for _ in range(h)]
    ox, oy = (w - nw) // 2, h - nh            # centred, standing on the bottom edge
    px = small.load()
    body = []
    for y in range(nh):
        for x in range(nw):
            r, g, b, a = px[x, y]
            if a >= 128:
                body.append((0.299 * r + 0.587 * g + 0.114 * b, x, y))
    # Walker art (HGSS) is mostly white with grey shading and black lines. Grade
    # each sprite on its own: its outline-dark pixels black, then of the rest the
    # lightest ~half white, the next third light grey, the darkest part dark grey.
    lines = [p for p in body if p[0] < 50]
    rest  = sorted((p for p in body if p[0] >= 50), key=lambda p: -p[0])
    n = len(rest)
    for i, (lum, x, y) in enumerate(rest):
        f = i / n if n else 0
        canvas[oy + y][ox + x] = 0 if f < 0.45 else 1 if f < 0.80 else 2
    for lum, x, y in lines:
        canvas[oy + y][ox + x] = 3
    # Body pixels that ended up white must still count as body for the outline.
    inside = [[False] * w for _ in range(h)]
    for lum, x, y in body:
        inside[oy + y][ox + x] = True
    # Outline: transparent pixels next to the body become black, as walker art has.
    # Outline inside the silhouette (the size stays): a white or light-grey body
    # pixel on the edge becomes black, so the shape never melts into the background.
    out = [row[:] for row in canvas]
    for y in range(h):
        for x in range(w):
            if inside[y][x] and canvas[y][x] in (0, 1) and any(
                not (0 <= y + dy < h and 0 <= x + dx < w) or not inside[y + dy][x + dx]
                for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                out[y][x] = 3
    return out


def encode(shades, w, h):
    """pw_img_encode: 8-px stripes, per column 2 bytes (msb plane, lsb plane), bit 0 = top."""
    out = bytearray()
    for s in range(h // 8):
        for x in range(w):
            hi = lo = 0
            for b in range(8):
                v = shades[s * 8 + b][x]
                hi |= ((v >> 1) & 1) << b
                lo |= (v & 1) << b
            out += bytes((hi, lo))
    return bytes(out)


def decode(data, w, h):
    sh = [[0] * w for _ in range(h)]
    i = 0
    for s in range(h // 8):
        for x in range(w):
            hi, lo = data[i], data[i + 1]
            i += 2
            for b in range(8):
                sh[s * 8 + b][x] = (((hi >> b) & 1) << 1) | ((lo >> b) & 1)
    return sh


GREYS = (255, 170, 85, 0)  # walker shades as grey for previews


def preview(shade_rows, scale=4):
    from PIL import Image
    h, w = len(shade_rows), len(shade_rows[0])
    im = Image.new("L", (w, h))
    im.putdata([GREYS[v] for row in shade_rows for v in row])
    return im.resize((w * scale, h * scale), Image.NEAREST)


def convert(cache, out):
    """<cache>/large/NNN.png + <cache>/small/NNN.png -> <out>/NNN.bin (walker layout:
    small 32x24 frame 1, frame 2, then large 64x48 frame 1, frame 2 - 0x180 + 0x600
    bytes) and <out>/preview.png. The large picture has one frame (the user asked for
    the first frame), so both large frames are the same; the small one uses the
    icon's own 2 frames when it has them."""
    from PIL import Image, ImageDraw
    os.makedirs(out, exist_ok=True)
    rows = []
    for f in sorted(os.listdir(os.path.join(cache, "large"))):
        if not f.endswith(".png"):
            continue
        num = int(f[:3])
        src = first_frame(os.path.join(cache, "large", f))
        frozen = os.environ.get("WBX_FROZEN")
        fp = os.path.join(frozen, "%03d.bin" % num) if frozen else None
        if fp and review_mode(num) is None and os.path.exists(fp):
            # Not flagged by the user: the reviewed version, byte for byte.
            data = open(fp, "rb").read()
            small = [decode(data[0:0xC0], 32, 24), decode(data[0xC0:0x180], 32, 24)]
            big = decode(data[0x180:0x480], 64, 48)
            data, big, small = apply_small_v2(num, cache, data, big, small)
            data, big, small = apply_hand_edits(num, data, big, small)
            open(os.path.join(out, "%03d.bin" % num), "wb").write(data)
            rows.append((num, src, big, small))
            continue
        big = to_walker(src, 64, 48, num)
        sp = os.path.join(cache, "small", f)
        if os.path.exists(sp):
            fr = all_frames(sp)[:2]
            small = to_walker_frames(fr, 32, 24, num)
            if len(small) == 1:
                small.append(small[0])
        else:
            small = [[[0] * 32 for _ in range(24)]] * 2
        data = encode(small[0], 32, 24) + encode(small[1], 32, 24) + encode(big, 64, 48) * 2
        data, big, small = apply_small_v2(num, cache, data, big, small)
        data, big, small = apply_hand_edits(num, data, big, small)
        open(os.path.join(out, "%03d.bin" % num), "wb").write(data)
        rows.append((num, src, big, small))
    if not rows:
        return
    S, gap = 3, 12
    W = 100 + 64 * S + 2 * 32 * S + gap * 5
    H = 24 + len(rows) * (48 * S + gap)
    sheet = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(sheet)
    for x, t in ((gap, "source"), (gap * 2 + 100, "LARGE 64x48"), (gap * 3 + 100 + 64 * S, "SMALL 32x24 frame 1"),
                 (gap * 4 + 100 + 64 * S + 32 * S, "frame 2")):
        d.text((x, 6), t, fill="black")
    for i, (num, src, big, small) in enumerate(rows):
        y = 24 + i * (48 * S + gap)
        t = src.copy()
        t.thumbnail((96, 96))
        bg = Image.new("RGB", t.size, "white")
        bg.paste(t, mask=t.getchannel("A"))
        sheet.paste(bg, (gap, y))
        d.text((gap, y + 100), "#%d" % num, fill="black")
        sheet.paste(preview(big, S).convert("RGB"), (gap * 2 + 100, y))
        sheet.paste(preview(small[0], S).convert("RGB"), (gap * 3 + 100 + 64 * S, y))
        sheet.paste(preview(small[1], S).convert("RGB"), (gap * 4 + 100 + 64 * S + 32 * S, y))
    sheet.save(os.path.join(out, "preview.png"))
    print("preview:", os.path.join(out, "preview.png"))
    # The same in pages of 40 (the whole sheet is too long to look through).
    per = 40
    rh = 48 * S + gap
    for p in range(0, len(rows), per):
        top = 24 + p * rh
        page = Image.new("RGB", (W, 24 + min(per, len(rows) - p) * rh), "white")
        page.paste(sheet.crop((0, 0, W, 24)), (0, 0))
        page.paste(sheet.crop((0, top, W, top + min(per, len(rows) - p) * rh)), (0, 24))
        name = "preview_%03d-%03d.png" % (rows[p][0], rows[min(p + per, len(rows)) - 1][0])
        page.save(os.path.join(out, name))
        print("page:", name)


# ---- hand edits (user, 2026-11-14: "can I edit the sprites directly to show you
# what I mean?") -------------------------------------------------------------------
# export writes, per Pokemon, <edit>/NNN_large.png (64x48), NNN_small1.png and
# NNN_small2.png (32x24): real size, palette PNG with exactly the 4 walker greys
# (index 0 white, 1 light, 2 dark, 3 black). The user edits them in any pixel editor
# and keeps the file name; convert then uses an edited picture as it is, ahead of
# every rule and setting. Only files the user changed differ from the export.
EDIT_GREYS = (255, 170, 85, 0)


def save_shades_png(shades, path):
    from PIL import Image
    h, w = len(shades), len(shades[0])
    im = Image.new("P", (w, h))
    pal = []
    for g in EDIT_GREYS:
        pal += [g, g, g]
    im.putpalette(pal + [0] * (768 - len(pal)))
    im.putdata([v for row in shades for v in row])
    im.save(path)


def load_shades_png(path, w, h):
    """Any PNG of the right size; each pixel -> the nearest walker grey."""
    from PIL import Image
    im = Image.open(path).convert("RGBA")
    if im.size != (w, h):
        raise ValueError("%s is %dx%d, expected %dx%d" % (path, im.width, im.height, w, h))
    out = []
    for y in range(h):
        row = []
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            l = 255 if a < 128 else lum_of((r, g, b))
            row.append(min(range(4), key=lambda i: abs(EDIT_GREYS[i] - l)))
        out.append(row)
    return out


def export_edits(out_dir, edit_dir, nums=None):
    os.makedirs(edit_dir, exist_ok=True)
    n = 0
    for f in sorted(os.listdir(out_dir)):
        if not f.endswith(".bin"):
            continue
        num = int(f[:3])
        if nums and num not in nums:
            continue
        d = open(os.path.join(out_dir, f), "rb").read()
        save_shades_png(decode(d[0x180:0x480], 64, 48), os.path.join(edit_dir, "%03d_large.png" % num))
        save_shades_png(decode(d[0:0xC0], 32, 24), os.path.join(edit_dir, "%03d_small1.png" % num))
        save_shades_png(decode(d[0xC0:0x180], 32, 24), os.path.join(edit_dir, "%03d_small2.png" % num))
        n += 1
    print("exported %d Pokemon to %s" % (n, edit_dir))


def hand_edit(num, part):
    """The user's edited picture for this Pokemon (part = large / small1 / small2)
    from the folder WBX_EDITS, if the file is there, else None. A file left unedited
    is identical to what convert made anyway."""
    d = os.environ.get("WBX_EDITS")
    if not d:
        return None
    p = os.path.join(d, "%03d_%s.png" % (num, part))
    if not os.path.exists(p):
        return None
    w, h = (64, 48) if part == "large" else (32, 24)
    return load_shades_png(p, w, h)


# ---- small sprites from menu icons, v2 (user, 2026-11-14: "go over the small
# sprites again. A lot of them are missing small details ... Outlines should remain
# and not be lost") --------------------------------------------------------------
# Icons are 1:1 pixel art where every pixel counts. Sun/Moon icons draw their
# outlines in a dark shade of each body colour, not black, so brightness rules
# turned the inner lines grey. Here:
#  1. outline colours = colours whose pixels mostly lie on the silhouette edge, plus
#     anything near-black: every pixel of them is black (inner lines included);
#  2. other colours: cut-offs 205 / 140 / 80, never black (black is for lines);
#  3. neighbours that clearly differ (brightness step >= ICON_STEP or colour
#     difference >= ICON_DE) but got the same grey: the darker one a shade darker
#     (up to dark grey), or, if it is already dark grey, the lighter one lighter.
ICON_STEP = 30.0
ICON_DE = 22.0
ICON_OUTLINE_SHARE = 0.4


def icon_frames(frames, num=None):
    from PIL import ImageOps
    from collections import Counter
    w, h = 32, 24
    frames = [ImageOps.mirror(f) for f in frames]
    bbox = None
    for f in frames:
        b = f.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
        if b:
            bbox = b if not bbox else (min(bbox[0], b[0]), min(bbox[1], b[1]), max(bbox[2], b[2]), max(bbox[3], b[3]))
    if bbox:
        frames = [f.crop(bbox) for f in frames]
    sw, sh = frames[0].size
    x0 = -(w - sw) // 2 if sw <= w else sw - w     # same window as before
    y0 = -(h - sh) if sh <= h else 0
    dx, dy = crop_override(num, 32)
    x0 = min(max(x0 + dx, 0), sw - w) if sw > w else x0 + dx
    y0 = min(max(y0 + dy, 0), sh - h) if sh > h else y0 + dy
    # outline colours, from all frames
    on_edge, total = Counter(), Counter()
    for f in frames:
        px = f.load()
        op = lambda x, y: 0 <= x < sw and 0 <= y < sh and px[x, y][3] >= 128
        for y in range(sh):
            for x in range(sw):
                if not op(x, y):
                    continue
                c = px[x, y][:3]
                total[c] += 1
                if not all(op(x + ex, y + ey) for ex in (-1, 0, 1) for ey in (-1, 0, 1)):
                    on_edge[c] += 1
    small_fixed = small_overrides(num)
    outline = {c for c in total if lum_of(c) < LINE_LUM or on_edge[c] / total[c] >= ICON_OUTLINE_SHARE and lum_of(c) < 120}
    # A dark Pokemon's body colours touch the edge a lot too (Venipede, Zorua went all
    # black): black may cover at most ~35% of the Pokemon, so the most common
    # outline colours (not near-black) are taken back out until it does.
    allpx = sum(total.values()) or 1
    while sum(total[c] for c in outline) / allpx > 0.35:
        soft = [c for c in outline if lum_of(c) >= LINE_LUM]
        if not soft:
            break
        outline.discard(max(soft, key=lambda c: total[c]))
    def shade(c):
        if c in small_fixed:
            return small_fixed[c]
        if c in outline:
            return 3
        s = walker_shade(lum_of(c))
        return 2 if s == 3 else s
    outs = []
    for f in frames:
        px = f.load()
        op = lambda x, y: 0 <= x < sw and 0 <= y < sh and px[x, y][3] >= 128
        out = [[0] * w for _ in range(h)]
        body = [[False] * w for _ in range(h)]
        col = [[None] * w for _ in range(h)]
        for y in range(h):
            for x in range(w):
                sx, sy = x + x0, y + y0
                if not op(sx, sy):
                    continue
                c = px[sx, sy][:3]
                body[y][x] = True
                col[y][x] = c
                edge = not all(op(sx + ex, sy + ey) for ex, ey in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                out[y][x] = 3 if edge else shade(c)
        base = [row[:] for row in out]
        for y in range(h):
            for x in range(w):
                if not body[y][x] or base[y][x] == 3 or small_fixed:
                    continue    # an icon with hand-set colours: no automatic detail rule (it made speckle)
                c = col[y][x]
                for ex, ey in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + ex, y + ey
                    if not (0 <= nx < w and 0 <= ny < h and body[ny][nx]) or base[ny][nx] != base[y][x]:
                        continue
                    q = col[ny][nx]
                    if lum_of(q) - lum_of(c) >= ICON_STEP or (lum_of(q) >= lum_of(c) and delta_e(q, c) >= ICON_DE):
                        # this pixel is the darker of the pair
                        if base[y][x] < 2:
                            out[y][x] = base[y][x] + 1
                        elif base[ny][nx] > 0:
                            out[ny][nx] = min(out[ny][nx], base[ny][nx] - 1)
                        break
        outs.append(out)
    return outs


def small_v2_list():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "walker_small_v2.txt")
    out = set()
    if os.path.exists(path):
        for line in open(path):
            out.update(int(t) for t in line.split("#")[0].split())
    return out


def apply_small_v2(num, cache, data, big, small):
    """For the Pokemon in walker_small_v2.txt the small sprite is made with
    icon_frames; the large sprite bytes stay exactly as they were."""
    if num not in small_v2_list():
        return data, big, small
    sp = os.path.join(cache, "small", "%03d.png" % num)
    if not os.path.exists(sp):
        return data, big, small
    s = icon_frames(all_frames(sp)[:2], num)
    if len(s) == 1:
        s.append(s[0])
    data = encode(s[0], 32, 24) + encode(s[1], 32, 24) + data[0x180:]
    return data, big, s


def apply_hand_edits(num, data, big, small):
    """The user's edited pictures replace the converted ones as they are."""
    e = hand_edit(num, "large")
    s1, s2 = hand_edit(num, "small1"), hand_edit(num, "small2")
    if e is None and s1 is None and s2 is None:
        return data, big, small
    big = e if e is not None else big
    small = [s1 if s1 is not None else small[0], s2 if s2 is not None else small[1]]
    data = encode(small[0], 32, 24) + encode(small[1], 32, 24) + encode(big, 64, 48) * 2
    return data, big, small


def ref(eeprom, outpng):
    from PIL import Image
    d = open(eeprom, "rb").read()
    small = [decode(d[0x91BE + f * 0xC0:0x91BE + (f + 1) * 0xC0], 32, 24) for f in range(2)]
    big = [decode(d[0x933E + f * 0x300:0x933E + (f + 1) * 0x300], 64, 48) for f in range(2)]
    ims = [preview(b, 3) for b in big] + [preview(s, 3) for s in small]
    W = sum(i.width for i in ims) + 40
    sheet = Image.new("L", (W, 48 * 3), 255)
    x = 0
    for i in ims:
        sheet.paste(i, (x, 0))
        x += i.width + 10
    sheet.save(outpng)
    print("reference:", outpng)


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "fetch":
        sp = sys.argv[3:]
        # 494-807: Gen 5-7 up to Zeraora. Meltan / Melmetal (808/809) are left out -
        # user: "they aren't in gen 7".
        nums = list(range(494, 808)) if sp == ["all"] else [int(s) for s in sp]
        fetch(sys.argv[2], nums)
    elif cmd == "convert":
        convert(sys.argv[2], sys.argv[3])
    elif cmd == "export":   # export <out_dir> <edit_dir> [dex...]
        export_edits(sys.argv[2], sys.argv[3], [int(s) for s in sys.argv[4:]] or None)
    elif cmd == "ref":
        ref(sys.argv[2], sys.argv[3])
