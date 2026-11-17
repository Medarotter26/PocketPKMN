"""Gen 6/7 mini sprite frame 2 - TEST round 3 (2026-11-16).

User: "they're still just a simple move up and down. and you cut off outlines on some pieces. it should be things
like a tail moving, ear moving, ribbons moving. look at ponyta as a good example".

Stock minis (Ponyta's flames, Pikachu's ears, Growlithe's tail, Shinx's tail star) redraw a part in a new position:
it swings, it does not slide. Here the part's own walker pixels (pack frame 1) are turned around its base
(RotSprite-like: enlarge 8x, turn with nearest neighbour, shrink), then outlines are redrawn ONLY around what
moved: a filled pixel next to empty space there becomes black, and 1 px cracks at the joint are closed.
frame 2 = stock body dip (upper body 1 px down, feet stay) + turned parts. Frame 1 + large sprite untouched (asserted).
Spec: tools/walker_mini67_swing.txt.  Writes art_src/mini67test/swing/NNN.bin, sheet.png, anim.gif.
Run from the app folder: python3 tools/walker_mini67_swing.py
"""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import walker_sprites as ws
from PIL import Image, ImageDraw

W, H = 32, 24
PACK = 'romfs/sprites/pack'
OUT = 'art_src/mini67test/swing'
SPEC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'walker_mini67_swing.txt')
GREY = [(255, 255, 255), (168, 168, 168), (80, 80, 80), (0, 0, 0)]
LCD = [(200, 208, 184), (140, 150, 128), (80, 88, 72), (24, 28, 20)]


def read_spec():
    spec = {}
    for line in open(SPEC, encoding='utf-8'):
        line = line.split('#')[0].strip()
        if not line:
            continue
        f = [p.split() for p in line.split('|')]
        num, base = int(f[0][0]), f[0][1]
        cut = int(f[0][2]) if base == 'squat' else None
        parts = [tuple(float(v) for v in p[:7]) for p in f[1:] if p]
        spec[num] = (base, cut, parts)
    return spec


def dip_shift(base, cut, y):
    if base == 'bob':
        return 1
    if base == 'rise':                 # floaters / flyers in the stock set go UP 1 px in frame 2
        return -1
    if base == 'squat':
        return 1 if y < cut else 0
    return 0


def dip(a, base, cut):
    out = [[0] * W for _ in range(H)]
    order = sorted(((x, y) for y in range(H) for x in range(W) if a[y][x]),
                   key=lambda p: (dip_shift(base, cut, p[1]) != 0, -p[1]))   # feet first, upper body over them
    for x, y in order:
        Y = y + dip_shift(base, cut, y)
        if Y < H:
            out[Y][x] = a[y][x]
    return out


def turn(pixels, pivot, angle, up=8):
    """pixels: {(x, y): shade}. Returns the turned set (RotSprite-like, nearest neighbour)."""
    if not pixels:
        return {}
    px, py = pivot
    t = math.radians(angle)
    c, s = math.cos(t), math.sin(t)
    hits = {}
    for (x, y), v in pixels.items():
        for j in range(up):
            for i in range(up):
                sx, sy = x + (i + 0.5) / up - (px + 0.5), y + (j + 0.5) / up - (py + 0.5)
                # anticlockwise on screen (y down): x' = x cos + y sin, y' = -x sin + y cos
                nx, ny = sx * c + sy * s + px + 0.5, -sx * s + sy * c + py + 0.5
                key = (math.floor(nx), math.floor(ny))
                hits.setdefault(key, {}).setdefault(v, 0)
                hits[key][v] += 1
    out = {}
    for key, votes in hits.items():
        if sum(votes.values()) >= up * up * 0.4:        # pixel mostly covered (keeps thin lines unbroken)
            out[key] = max(votes, key=lambda v: (votes[v], v))
    return out


def bend(pixels, pivot, amount):
    """Pixel-art bend: the part's base stays, its tip moves `amount` px. A part reaching sideways from its base
    (pivot x outside the part's middle) bends up/down: each column moves vertically by amount * distance / reach.
    A part reaching up/down from its base bends sideways: each row moves horizontally. Whole rows/columns move,
    so lines stay crisp (how the stock minis move Pikachu's ears, Growlithe's tail)."""
    if not pixels:
        return {}
    px, py = pivot
    xs = [x for x, _ in pixels]
    ys = [y for _, y in pixels]
    reach_x = max(abs(x - px) for x in xs)
    reach_y = max(abs(y - py) for y in ys)
    sideways = reach_x >= reach_y              # part reaches sideways -> columns move up/down
    out = {}
    for (x, y), v in pixels.items():
        if sideways:
            k = round(amount * abs(x - px) / max(reach_x, 1))
            out[(x, y + k)] = (v, k)
        else:
            k = round(amount * abs(y - py) / max(reach_y, 1))
            out[(x + k, y)] = (v, k)
    return out


def inside(a):
    """Pixels inside the Pokemon: everything the background can't reach. White (0) is both the background and
    white body colour, so the background is flood-filled from the picture edge through 4-neighbours (an
    8-connected outline stops it); white pixels it doesn't reach are white body."""
    from collections import deque
    out_ = [[False] * W for _ in range(H)]
    q = deque((x, y) for y in range(H) for x in range(W) if (x in (0, W - 1) or y in (0, H - 1)) and not a[y][x])
    for x, y in q:
        out_[y][x] = True
    while q:
        x, y = q.popleft()
        for i, j in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + i, y + j
            if 0 <= nx < W and 0 <= ny < H and not out_[ny][nx] and not a[ny][nx]:
                out_[ny][nx] = True
                q.append((nx, ny))
    return [[not out_[y][x] for x in range(W)] for y in range(H)]


N4 = ((1, 0), (-1, 0), (0, 1), (0, -1))


def frame2(f1, base, cut, parts):
    ins1 = inside(f1)
    # body dip on the whole inside (white body included): feet first, upper body painted over them
    val, M = {}, set()
    order = sorted(((x, y) for y in range(H) for x in range(W) if ins1[y][x]),
                   key=lambda p: (dip_shift(base, cut, p[1]) != 0, -p[1]))
    for x, y in order:
        Y = y + dip_shift(base, cut, y)
        if 0 <= Y < H:
            val[(x, Y)] = f1[y][x]
            M.add((x, Y))
    before = set(M)
    moved, shifted, changed = set(), set(), set()
    for (x0, y0, x1, y1, px, py, amt) in parts:
        x0, y0, x1, y1 = int(x0), int(y0), int(x1), int(y1)
        d = dip_shift(base, cut, int(py))
        part = {(x, y): val[(x, y)] for y in range(y0 + d, y1 + d + 1) for x in range(x0, x1 + 1)
                if (x, y) in M and (x, y) not in moved}
        for p in part:
            M.discard(p)
            changed.add(p)
        for (x, y), (v, k) in bend(part, (px, py + d), amt).items():
            if 0 <= x < W and 0 <= y < H:
                val[(x, y)] = v
                M.add((x, y))
                moved.add((x, y))
                changed.add((x, y))
                if k:
                    shifted.add((x, y))
    zone = {(x + i, y + j) for (x, y) in changed for i in (-1, 0, 1) for j in (-1, 0, 1)
            if 0 <= x + i < W and 0 <= y + j < H}
    # 1. cracks: an empty pixel between a bent pixel and something filled -> outline pixel (new detail)
    for (x, y) in sorted(zone):
        if (x, y) in M:
            continue
        for a_, b_ in (((x - 1, y), (x + 1, y)), ((x, y - 1), (x, y + 1))):
            if a_ in M and b_ in M and (a_ in shifted or b_ in shifted):
                src = a_ if a_ in shifted else b_          # stretch the piece itself across the crack
                val[(x, y)] = val[src]
                M.add((x, y))
                moved.add((x, y))
                break
    # 2. where the bent piece now lies against the body and neither side is black, draw the dividing line on
    #    the body side (new detail), so the piece reads as separate and keeps its own shading
    Ms = set(M)
    for (x, y) in shifted:
        if val[(x, y)] == 3:
            continue
        for i, j in N4:
            q = (x + i, y + j)
            if q in Ms and q not in moved and val[q] != 3:
                val[q] = 3
    # 3. body pixels that the piece used to cover and that now touch empty space get their outline back
    for (x, y) in zone:
        if (x, y) in Ms and (x, y) not in moved:
            now_open = any((x + i, y + j) not in Ms for i, j in N4)
            was_open = any((x + i, y + j) not in before for i, j in N4)
            if now_open and not was_open:
                val[(x, y)] = 3
    # 4. frame 1's own rule (0 exceptions in all 12): the silhouette edge is always black
    for (x, y) in M:
        if any((x + i, y + j) not in M for i, j in N4):
            val[(x, y)] = 3
    out = [[0] * W for _ in range(H)]
    for (x, y) in M:
        out[y][x] = val[(x, y)]
    return out


def outline_gaps(f, zone_from):
    """Check: filled pixels on the silhouette edge that are not black, near what moved (should be none,
    except where frame 1 itself had them)."""
    ins = inside(f)
    bad = []
    for y in range(H):
        for x in range(W):
            if ins[y][x] and f[y][x] != 3 and not all(0 <= x + i < W and 0 <= y + j < H and ins[y + j][x + i]
                                                      for i, j in N4):
                bad.append((x, y))
    return bad


def draw(d, s, ox, oy, sc, pal):
    for y in range(H):
        for x in range(W):
            d.rectangle([ox + x * sc, oy + y * sc, ox + x * sc + sc - 1, oy + y * sc + sc - 1], fill=pal[s[y][x]])


def main():
    os.makedirs(OUT, exist_ok=True)
    cells = []
    for num, (base, cut, parts) in sorted(read_spec().items()):
        data = open('%s/%03d.bin' % (PACK, num), 'rb').read()
        f1 = ws.decode(data[:0xC0], W, H)
        f2 = frame2(f1, base, cut, parts)
        print('%03d  silhouette-edge pixels that are not black: frame 1 %d, frame 2 %d'
              % (num, len(outline_gaps(f1, None)), len(outline_gaps(f2, None))))
        new = data[:0xC0] + ws.encode(f2, W, H) + data[0x180:]
        assert len(new) == 0x780 and new[:0xC0] == data[:0xC0] and new[0x180:] == data[0x180:]
        open('%s/%03d.bin' % (OUT, num), 'wb').write(new)
        cells.append((num, f1, f2))
    sc, cols = 6, 2
    cw, ch = 2 * (W * sc + 8) + 16, H * sc + 20
    im = Image.new('RGB', (cw * cols, ch * ((len(cells) + 1) // 2)), (228, 228, 228))
    d = ImageDraw.Draw(im)
    for i, (num, f1, f2) in enumerate(cells):
        ox, oy = (i % cols) * cw + 6, (i // cols) * ch + 14
        d.text((ox, oy - 12), '%03d   frame 1 | frame 2' % num, fill=(200, 0, 0))
        draw(d, f1, ox, oy, sc, GREY)
        draw(d, f2, ox + W * sc + 8, oy, sc, GREY)
    im.save('%s/sheet.png' % OUT)
    frames = []
    sc, cols = 7, 3
    cw, ch = W * sc + 24, H * sc + 30
    for t in (0, 1):
        g = Image.new('RGB', (cw * cols + 12, ch * ((len(cells) + cols - 1) // cols) + 12), (245, 245, 240))
        gd = ImageDraw.Draw(g)
        for i, (num, f1, f2) in enumerate(cells):
            ox, oy = 12 + (i % cols) * cw, 8 + (i // cols) * ch
            gd.text((ox, oy), '%03d' % num, fill=(0, 0, 0))
            draw(gd, f1 if t == 0 else f2, ox, oy + 16, sc, LCD)
        frames.append(g)
    frames[0].save('%s/anim.gif' % OUT, save_all=True, append_images=frames[1:], duration=500, loop=0)
    print('wrote', len(cells))


if __name__ == '__main__':
    main()
