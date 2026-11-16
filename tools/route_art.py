#!/usr/bin/env python3
"""route_art.py [out dir] - WalkerBox's own route pictures for the walker (32x24, 4 shades: 0 white .. 3 black).

Eight kinds (user 2026-11-15: "Make sure the pictures have depth and are detailed"): field, forest, city, river,
beach, cliffs, cave, snow. Each is built in layers from far (light) to near (dark) with perspective, then written
as art/routes/<kind>.txt (rows of '.', '-', '+', '#') for hand touch-ups; the app/tools read the .txt files.
`preview` renders a sheet: art_src/route_pictures.png (8x, walker LCD colours).
"""
import math
import os
import sys

W, H = 32, 24
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "art", "routes")
CH = ".-+#"


class C:
    def __init__(self, v=0):
        self.p = [[v] * W for _ in range(H)]

    def set(self, x, y, v):
        x, y = int(round(x)), int(round(y))
        if 0 <= x < W and 0 <= y < H:
            self.p[y][x] = v

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < W and 0 <= y < H else 0

    def rect(self, x0, y0, x1, y1, v):
        for y in range(max(0, y0), min(H, y1 + 1)):
            for x in range(max(0, x0), min(W, x1 + 1)):
                self.p[y][x] = v

    def line(self, x0, y0, x1, y1, v):
        n = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for i in range(n + 1):
            t = i / n
            self.set(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, v)

    def poly(self, pts, v):
        for y in range(H):
            yc = y + 0.5
            xs = []
            for i in range(len(pts)):
                (xa, ya), (xb, yb) = pts[i], pts[(i + 1) % len(pts)]
                if (ya <= yc < yb) or (yb <= yc < ya):
                    xs.append(xa + (yc - ya) * (xb - xa) / (yb - ya))
            xs.sort()
            for a, b in zip(xs[::2], xs[1::2]):
                for x in range(int(math.ceil(a - 0.5)), int(math.floor(b - 0.5)) + 1):
                    self.set(x, y, v)

    def disc(self, cx, cy, r, v):
        for y in range(H):
            for x in range(W):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    self.p[y][x] = v

    def ground(self, f, v, below=True):
        """Fill under (or above) the curve y = f(x)."""
        for x in range(W):
            yy = f(x)
            for y in range(H):
                if (y >= yy) if below else (y <= yy):
                    self.p[y][x] = v

    def dither(self, x0, y0, x1, y1, a, b, density):
        """Mix shades a/b in a 2x2 ordered pattern (density 0..1 of b)."""
        order = [[0, 2], [3, 1]]
        for y in range(max(0, y0), min(H, y1 + 1)):
            for x in range(max(0, x0), min(W, x1 + 1)):
                self.p[y][x] = b if (order[y % 2][x % 2] + 0.5) / 4 < density else a

    def outline(self, inner, v):
        """Draw v around every pixel of shade `inner` that touches another shade."""
        src = [r[:] for r in self.p]
        for y in range(H):
            for x in range(W):
                if src[y][x] != inner:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < W and 0 <= ny < H and src[ny][nx] not in (inner, v):
                        self.p[y][x] = v
                        break


def cloud(c, x, y, w):
    c.rect(x + 1, y, x + w - 2, y, 1)
    c.rect(x, y + 1, x + w - 1, y + 1, 1)
    c.set(x + w // 3, y - 1, 1)
    c.set(x + w // 3 + 1, y - 1, 1)


def tree_round(c, x, y, r, crown=2, trunk=3):
    c.rect(x, y, x, y + r + 2, trunk)
    c.disc(x, y - 1, r, crown)
    c.set(x - 1, y - 2, 1)
    c.set(x - r + 1, y - 1, 1)


def pine(c, x, base, h, v=3, snow=False):
    for i in range(h):
        half = (i + 2) // 2
        y = base - h + i
        c.rect(x - half, y, x + half, y, v)
        if snow and i % 3 == 0:
            c.rect(x - half, y, x + half, y, 0)
    c.rect(x, base, x, base + 1, 3)


def field():
    c = C(0)
    cloud(c, 3, 3, 8)
    cloud(c, 20, 2, 6)
    c.ground(lambda x: 10 + 1.5 * math.sin(x / 5.0), 1)                       # far hills
    c.ground(lambda x: 13 + 1.6 * math.sin(x / 4.0 + 2), 2)                   # middle hills
    c.ground(lambda x: 16 + 0.8 * math.sin(x / 3.0), 1)                       # meadow
    for x in range(0, W, 3):                                                   # grass texture, nearer = bigger
        c.set(x + (x // 3) % 2, 17, 2)
    for x in range(1, W, 2):
        c.set(x, 20 + (x % 3 == 0), 2)
    c.poly([(14.5, 16), (17.5, 16), (24, 24), (8, 24)], 0)                    # path to the horizon
    c.line(14, 16, 7.5, 23.5, 2)
    c.line(18, 16, 24.5, 23.5, 2)
    for x in (2, 5, 27, 30):                                                   # near tufts
        c.set(x, 22, 3)
        c.set(x - 1, 23, 3)
        c.set(x + 1, 23, 3)
    tree_round(c, 26, 11, 3)                                                   # a tree on the hill
    c.outline(2, 3) if False else None
    return c


def forest():
    c = C(0)
    c.rect(0, 0, W - 1, 5, 2)                                                  # canopy
    c.dither(0, 6, W - 1, 7, 2, 0, 0.5)
    for x in range(0, W, 2):
        c.set(x + (x // 2) % 2, 1 + (x % 4 == 0), 1)                          # leaf light
    c.rect(0, 16, W - 1, H - 1, 1)                                             # ground
    c.dither(0, 15, W - 1, 15, 0, 1, 0.5)
    for x, w, v in ((9, 1, 1), (13, 1, 1), (19, 1, 1), (22, 1, 1)):           # far trunks
        c.rect(x, 6, x + w - 1, 15, v)
    for x, w in ((5, 2), (17, 2), (25, 2)):                                    # middle trunks
        c.rect(x, 4, x + w - 1, 17, 2)
    for x, w in ((0, 3), (29, 3)):                                             # near trunks
        c.rect(x, 0, x + w - 1, H - 1, 3)
    c.set(1, 8, 2)
    c.set(30, 12, 2)
    c.poly([(14.5, 16), (16.5, 16), (21, 24), (10, 24)], 0)                    # path
    for y in range(8, 15, 3):                                                  # light shafts
        c.set(11 + (y - 8) // 2, y, 0)
        c.set(12 + (y - 8) // 2, y + 1, 0)
    for x in (4, 7, 24, 27):                                                   # ferns
        c.set(x, 22, 2)
        c.set(x - 1, 23, 3)
        c.set(x + 1, 23, 3)
    return c


def city():
    c = C(0)
    cloud(c, 12, 2, 6)
    for x0, x1, top in ((10, 12, 6), (13, 15, 8), (16, 18, 5), (19, 21, 7)):  # far skyline
        c.rect(x0, top, x1, 13, 1)
    c.poly([(0, 0), (9, 5), (9, 15), (0, 24)], 2)                              # left buildings in perspective
    c.poly([(32, 0), (22, 5), (22, 15), (32, 24)], 2)
    for i in range(4):                                                         # windows, smaller with distance
        x = 1 + i * 2
        for j in range(3):
            y0 = 4 + i // 2 + j * 4
            c.rect(x, y0, x, y0 + 1 + (i < 2), 0 if (i + j) % 3 else 1)
            c.rect(30 - i * 2, y0, 30 - i * 2, y0 + 1 + (i < 2), 0 if (i + j) % 2 else 1)
    c.line(9, 5, 9, 15, 3)
    c.line(22, 5, 22, 15, 3)
    c.poly([(9, 15), (22, 15), (32, 24), (0, 24)], 1)                          # street
    c.poly([(13, 15), (18, 15), (26, 24), (5, 24)], 0)
    for y in (16, 18, 21):                                                     # centre line dashes
        c.set(15 + 0.0, y, 3)
        c.set(15, y + 1, 3)
    c.line(0, 24, 9, 15, 3)
    c.line(31, 24, 22, 15, 3)
    return c


def river():
    c = C(0)
    cloud(c, 4, 3, 7)
    c.ground(lambda x: 9 + 1.2 * math.sin(x / 3.5), 1)                         # far trees/hills
    for x in range(0, W, 3):
        c.set(x, 8 + (x % 2), 2)
    c.rect(0, 12, W - 1, H - 1, 2)                                             # banks
    c.poly([(13, 12), (19, 12), (30, 24), (2, 24)], 1)                         # water, wide at the front
    for y, xs in ((14, (12, 18)), (17, (9, 16, 22)), (20, (6, 13, 20, 25)), (23, (4, 11, 18, 26))):
        for x in xs:                                                           # ripples, longer nearer
            ln = 1 + (y - 12) // 4
            c.rect(x, y, x + ln, y, 0)
    for x, h in ((1, 5), (3, 4), (28, 5), (30, 4)):                            # reeds in front
        c.rect(x, H - h, x, H - 1, 3)
        c.set(x, H - h - 1, 2)
    c.line(13, 12, 2, 24, 3)
    c.line(19, 12, 30, 24, 3)
    return c


def beach():
    c = C(0)
    c.disc(24, 4, 2.5, 1)                                                      # sun
    c.disc(24, 4, 1.4, 0)
    c.rect(0, 9, W - 1, 13, 1)                                                 # sea
    c.rect(0, 9, W - 1, 9, 2)                                                  # horizon line
    for y, step in ((11, 6), (13, 4)):
        for x in range((y * 3) % step, W, step):
            c.rect(x, y, x + 1, y, 2)
    c.rect(0, 14, W - 1, 14, 0)                                                # foam
    for x in range(0, W, 2):
        c.set(x, 15, 1 if x % 4 else 0)
    c.rect(0, 16, W - 1, H - 1, 0)                                             # sand
    for (x, y) in ((4, 18), (11, 21), (19, 19), (25, 22), (29, 18), (8, 23), (16, 23)):
        c.set(x, y, 1)
    c.line(5, 23, 7, 9, 3)                                                     # palm trunk (curved)
    c.line(6, 23, 8, 9, 3)
    for dx, dy in ((-5, 2), (-3, 3), (3, 3), (5, 2), (-4, -1), (4, -1)):       # fronds
        c.line(8, 8, 8 + dx, 8 + dy, 3)
    c.set(8, 9, 2)
    c.set(9, 10, 2)                                                            # coconuts
    c.rect(22, 20, 24, 21, 2)                                                  # a shell / rock
    c.set(23, 19, 3)
    return c


def cliffs():
    c = C(0)
    c.poly([(0, 12), (6, 4), (11, 9), (17, 2), (24, 10), (28, 6), (32, 11), (32, 24), (0, 24)], 1)  # far peaks
    for x, y in ((6, 4), (17, 2), (28, 6)):                                    # snow caps
        c.set(x, y + 1, 0)
        c.set(x - 1, y + 2, 0)
        c.set(x + 1, y + 2, 0)
    c.poly([(0, 24), (0, 10), (4, 9), (7, 13), (10, 12), (12, 24)], 2)         # near cliff left
    c.poly([(32, 24), (32, 8), (27, 10), (24, 14), (21, 13), (19, 24)], 2)     # near cliff right
    for y in (13, 16, 19, 22):                                                 # ledges
        c.line(0, y, 9, y + 1, 3)
        c.line(21, y + 1, 31, y - 1, 3)
    c.line(4, 9, 4, 24, 3)
    c.line(27, 10, 27, 24, 3)
    c.poly([(12, 24), (14, 16), (17, 16), (19, 24)], 0)                        # path between
    c.rect(14, 15, 17, 15, 1)
    return c


def cave():
    c = C(3)
    c.poly([(5, 24), (6, 9), (10, 4), (16, 2), (22, 4), (26, 9), (27, 24)], 2)  # cave mouth
    c.poly([(10, 24), (11, 11), (14, 7), (18, 7), (21, 11), (22, 24)], 1)      # deeper: lighter (light inside)
    c.poly([(13, 24), (14, 13), (16, 11), (18, 13), (19, 24)], 0)              # light at the end
    for x, ln in ((9, 3), (12, 2), (16, 3), (20, 2), (23, 3)):                 # stalactites
        for i in range(ln):
            c.set(x, 3 + i + (x % 3), 3)
    for x, y in ((7, 21), (8, 22), (24, 20), (25, 21), (26, 22)):              # rocks
        c.set(x, y, 3)
    c.rect(6, 23, 26, 23, 2)
    for x, y in ((3, 6), (29, 14), (2, 18)):                                   # crystal glints in the rock
        c.set(x, y, 1)
        c.set(x, y + 1, 2)
    return c


def snow():
    c = C(0)
    c.poly([(0, 14), (8, 5), (12, 8), (19, 1), (27, 9), (32, 6), (32, 24), (0, 24)], 1)  # far mountains
    c.poly([(0, 14), (8, 5), (12, 8), (19, 1), (27, 9), (32, 6), (32, 9), (27, 12), (19, 5), (12, 11), (8, 8),
            (0, 16)], 0)                                                       # snow on them
    c.ground(lambda x: 15 + 1.0 * math.sin(x / 4.0), 0)                        # snowfield
    c.ground(lambda x: 14 + 1.0 * math.sin(x / 4.0), 1, below=False) if False else None
    for x in range(W):
        c.set(x, int(15 + 1.0 * math.sin(x / 4.0)), 1)                         # shadow line of the snowfield
    pine(c, 4, 20, 9, 3, snow=True)                                            # near pines
    pine(c, 27, 21, 10, 3, snow=True)
    pine(c, 15, 15, 5, 2)                                                      # far pine
    pine(c, 20, 15, 4, 2)
    for x, y in ((2, 2), (11, 4), (24, 3), (30, 13), (9, 12), (21, 20), (13, 22)):  # snowflakes
        c.set(x, y, 1)
    for x in range(8, 24, 3):                                                  # footprints
        c.set(x, 22 - (x - 8) // 6, 1)
    return c


# --- round 2 (2026-11-15): SoulSilver's style, studied from the real route picture in the user's walker backup:
# white sky, bold dark silhouettes with a few light-grey highlight pixels, everything standing on ONE horizon line
# over a light-grey ground band, small grass tufts, no dithering. Depth = a light-grey far layer behind.
HZ = 18  # horizon row: the dark line; ground band below it


def base_ground(c, tufts=True):
    c.rect(0, HZ, W - 1, HZ, 2)
    c.rect(0, HZ + 1, W - 1, H - 1, 1)
    if tufts:
        for x in (3, 9, 17, 22, 28):
            c.set(x, HZ - 1, 2)
            c.set(x + 1, HZ - 2, 2) if x % 2 else c.set(x, HZ - 2, 2)
        for x in (5, 13, 24):  # pebbles / grass in the band, darker = nearer
            c.set(x, HZ + 2 + (x % 3), 2)
            c.set(x + 1, HZ + 2 + (x % 3), 2)


def far_hills(c, y0, amp, period, phase=0.0):
    for x in range(W):
        top = int(round(y0 + amp * math.sin(x / period + phase)))
        for y in range(top, HZ):
            c.p[y][x] = 1


def tree2(c, x, ground, r=3, h=4):
    """Round tree like the game's: solid dark crown, black edge pixels, light-grey highlight, black trunk."""
    c.rect(x, ground - h, x, ground - 1, 3)
    cy = ground - h - r + 1
    c.disc(x, cy, r, 2)
    for y in range(H):  # black lower-right rim
        for xx in range(W):
            if c.get(xx, y) == 2 and (xx - x) + (y - cy) >= r and (xx - x) ** 2 + (y - cy) ** 2 <= r * r + 1:
                c.p[y][xx] = 3
    c.set(x - 1, cy - r + 1, 1)
    c.set(x - r + 1, cy - 1, 1)
    c.set(x, cy - r + 1, 1)


def pine2(c, x, ground, h, snow=False, far=False):
    """Tiered pine: three stacked triangles, each wider; dark body, black shadow side, a light (or snow) edge on
    the lit upper-left of each tier; black trunk."""
    v = 1 if far else 2
    trunk = 2 if h >= 9 else 1
    body = h - trunk
    tiers = 3
    th = max(3, body // tiers + 1)
    top = ground - h
    for t in range(tiers):
        y0 = top + t * (body - th) // max(1, tiers - 1)
        for i in range(th):
            half = (i * (2 + t)) // th + t // 2
            y = y0 + i
            c.rect(x - half, y, x + half, y, v)
            if far:
                continue
            c.set(x + half, y, 3)                       # shadow side
            if snow and i == 0:
                c.rect(x - half, y, x + half - 1, y, 0)           # snow cap on top of each tier
            elif snow and i == 1:
                c.set(x - half, y, 0)                             # snow on the lit edge
            elif i == th - 1:
                c.set(x - half, y, 1)                   # light tip of the tier
    c.rect(x, ground - trunk, x, ground - 1, 3 if not far else 1)


def field2():
    c = C(0)
    far_hills(c, 13, 1.5, 4.0)
    base_ground(c)
    tree2(c, 6, HZ, 3, 4)
    tree2(c, 17, HZ, 4, 4)
    tree2(c, 26, HZ, 3, 3)
    for x in (11, 21, 30):  # flowers
        c.set(x, HZ - 1, 3)
        c.set(x, HZ - 2, 1)
    c.rect(4, 4, 8, 4, 1)  # one small cloud
    c.rect(3, 5, 10, 5, 1)
    return c


def forest2():
    c = C(0)
    for x in (1, 7, 12, 19, 23, 29):   # far pines, light: depth
        pine2(c, x, HZ, 8, far=True)
    base_ground(c)
    pine2(c, 5, HZ, 13)
    pine2(c, 16, HZ, 16)
    pine2(c, 26, HZ, 12)
    for x in (10, 21):  # mushrooms / ferns between the trees
        c.set(x, HZ - 1, 3)
        c.set(x - 1, HZ - 2, 2)
        c.set(x + 1, HZ - 2, 2)
    return c


def city2():
    c = C(0)
    for x0, x1, top in ((0, 4, 9), (5, 8, 6), (20, 23, 8), (24, 27, 5), (28, 31, 10)):  # far skyline
        c.rect(x0, top, x1, HZ - 1, 1)
    base_ground(c, tufts=False)
    for x0, x1, top in ((7, 13, 4), (15, 21, 8)):                                      # near buildings
        c.rect(x0, top, x1, HZ - 1, 2)
        c.rect(x1, top, x1, HZ - 1, 3)                                                 # shadow side
        c.rect(x0, top, x1, top, 3)                                                    # roof edge
        for y in range(top + 2, HZ - 2, 3):
            for x in range(x0 + 1, x1 - 1, 2):
                c.set(x, y, 0)                                                         # lit windows
        c.rect((x0 + x1) // 2, HZ - 3, (x0 + x1) // 2 + 1, HZ - 1, 3)                   # door
    for x in range(2, W, 6):                                                           # road markings
        c.rect(x, HZ + 3, x + 2, HZ + 3, 0)
    c.rect(26, HZ - 6, 26, HZ - 1, 3)                                                  # street lamp
    c.rect(25, HZ - 6, 27, HZ - 6, 2)
    return c


def river2():
    c = C(0)
    far_hills(c, 12, 1.0, 3.5, 1.0)
    for x in (4, 10, 26):  # far trees on the far bank
        c.disc(x, 11, 1.6, 2)
    c.rect(0, 13, W - 1, 13, 2)                       # far bank line
    c.rect(0, 14, W - 1, H - 1, 1)                    # water
    for y, xs in ((15, (3, 14, 25)), (17, (8, 19, 29)), (19, (1, 12, 22)), (21, (6, 16, 27)), (23, (2, 11, 21, 30))):
        for x in xs:
            c.rect(x, y, x + 2 + (y - 15) // 3, y, 0)  # ripples, longer nearer
    c.poly([(0, 24), (0, 19), (5, 20), (8, 24)], 2)   # near bank left
    c.poly([(32, 24), (32, 18), (27, 19), (24, 24)], 2)
    for x, h in ((1, 6), (3, 5), (29, 7), (31, 5)):    # reeds
        c.rect(x, 24 - h, x, 23, 3)
        c.set(x, 23 - h, 2)
    return c


def beach2():
    c = C(0)
    c.disc(24, 5, 3, 1)                               # sun
    c.disc(24, 5, 2, 0)
    c.rect(0, 11, W - 1, 11, 2)                       # horizon
    c.rect(0, 12, W - 1, 16, 1)                       # sea
    for y, step, off in ((13, 7, 2), (15, 5, 0)):
        for x in range(off, W, step):
            c.rect(x, y, x + 1, y, 0)                 # wave crests
    c.rect(0, 17, W - 1, 17, 0)                       # foam line
    for x in range(1, W, 3):
        c.set(x, 18, 1)
    for (x, y) in ((12, 20), (18, 22), (25, 21), (29, 23), (15, 23)):  # sand dots
        c.set(x, y, 1)
    # palm: a 2-pixel curved trunk with ring marks and drooping fronds, solid like the game's silhouettes
    trunk = ((4, 23), (4, 22), (5, 21), (5, 20), (5, 19), (6, 18), (6, 17), (6, 16), (7, 15), (7, 14), (7, 13),
             (8, 12))
    for i, (x, y) in enumerate(trunk):
        c.set(x, y, 3)
        c.set(x + 1, y, 2 if i % 2 else 3)
    cx, cy = 8, 10
    for side in (-1, 1):  # three fronds each side, drooping arcs
        for length, drop in ((7, 4), (5, 2), (4, 1)):
            for k in range(length + 1):
                x = cx + side * k
                y = cy + int(round(drop * (k / length) ** 2)) - (1 if length == 4 else 0)
                c.set(x, y, 3)
                if k % 2 == 0 and k > 1:
                    c.set(x, y + 1, 2)  # leaflets
    c.rect(cx - 1, cy - 1, cx + 1, cy, 3)  # crown
    c.set(cx - 1, cy - 1, 1)
    c.set(cx, cy + 1, 2)                   # coconuts
    c.set(cx + 1, cy + 1, 2)
    c.rect(20, 21, 22, 22, 2)                          # rock
    c.set(20, 21, 1)
    return c


def cliffs2():
    c = C(0)
    c.poly([(0, HZ), (5, 7), (10, 11), (17, 3), (24, 10), (28, 6), (32, 9), (32, HZ)], 1)  # far mountains
    for x, y in ((5, 7), (17, 3), (28, 6)):                                                # snow caps
        c.rect(x - 1, y + 1, x + 1, y + 1, 0)
        c.set(x, y, 0)
    base_ground(c, tufts=False)
    c.poly([(0, HZ), (0, 9), (4, 8), (8, 12), (11, HZ)], 2)                                # near crag left
    c.poly([(32, HZ), (32, 7), (28, 9), (25, 13), (22, HZ)], 2)                            # near crag right
    for y in (11, 14, 17):
        c.rect(0, y, 6 - (y - 11) // 3, y, 3)                                              # ledges
        c.rect(27 - (y - 11) // 4, y, 31, y, 3)
    c.line(4, 8, 8, 12, 3)
    c.line(28, 9, 25, 13, 3)
    c.set(2, 9, 1)
    c.set(30, 8, 1)
    for x in (13, 18):  # boulders
        c.rect(x, HZ - 2, x + 2, HZ - 1, 2)
        c.set(x, HZ - 2, 1)
    return c


def cave2():
    c = C(3)
    c.poly([(3, 24), (4, 10), (9, 4), (16, 2), (23, 4), (28, 10), (29, 24)], 2)   # cave wall lit by the opening
    c.poly([(9, 24), (10, 11), (14, 7), (18, 7), (22, 11), (23, 24)], 1)          # passage
    c.poly([(13, 24), (14, 13), (16, 11), (18, 13), (19, 24)], 0)                 # light at the far end
    for x, ln in ((8, 3), (12, 2), (16, 3), (20, 2), (24, 3)):                    # stalactites
        for i in range(ln):
            c.set(x, 4 + i, 3)
    c.rect(0, 21, W - 1, 21, 2)                                                   # floor line
    for x, y in ((6, 20), (7, 19), (25, 20), (26, 19), (24, 20)):                  # rocks
        c.set(x, y, 3)
    for x, y in ((5, 15), (27, 13), (26, 16)):                                     # crystals
        c.set(x, y, 0)
        c.set(x, y + 1, 1)
    return c


def snow2():
    c = C(0)
    c.poly([(0, 14), (7, 6), (12, 9), (19, 2), (26, 8), (32, 5), (32, HZ), (0, HZ)], 1)  # far mountains
    c.poly([(5, 8), (7, 6), (9, 8), (8, 9), (6, 9)], 0)                                  # snow caps
    c.poly([(17, 4), (19, 2), (21, 4), (20, 5), (18, 5)], 0)
    c.rect(0, HZ, W - 1, HZ, 1)                                                           # snowfield edge
    c.rect(0, HZ + 1, W - 1, H - 1, 0)
    for x in range(2, W, 5):
        c.rect(x, HZ + 3 + (x % 2), x + 2, HZ + 3 + (x % 2), 1)                           # snow drifts
    pine2(c, 6, HZ + 1, 13, snow=True)
    pine2(c, 25, HZ + 1, 14, snow=True)
    pine2(c, 15, HZ, 8, snow=True)
    for x, y in ((2, 2), (11, 3), (24, 1), (29, 11), (13, 12)):                           # snowflakes
        c.set(x, y, 1)
    return c


# --- round 3 (user 2026-11-15: "do those route pictures actually match our routes though?" -> one picture per
# course, 18). Same style rules as round 2.

def house(c, x, ground, w=7, h=5, roof=3, door=True):
    """Small house: dark roof triangle, light walls with a black outline, a window, a door."""
    c.rect(x, ground - h, x + w - 1, ground - 1, 0)
    c.rect(x, ground - h, x, ground - 1, 3)
    c.rect(x + w - 1, ground - h, x + w - 1, ground - 1, 3)
    for i in range(roof + 1):  # roof
        c.rect(x - 1 + i, ground - h - i, x + w - i, ground - h - i, 2 if i else 3)
    c.set(x + 1, ground - h - 1, 1)
    c.rect(x + 1, ground - h + 1, x + 2, ground - h + 2, 2)  # window
    if door:
        c.rect(x + w - 3, ground - 3, x + w - 2, ground - 1, 3)


def fence(c, x0, x1, ground):
    c.rect(x0, ground - 2, x1, ground - 2, 2)
    for x in range(x0, x1 + 1, 2):
        c.rect(x, ground - 3, x, ground - 1, 3)


def quiet_outskirts():
    c = C(0)
    far_hills(c, 12, 1.2, 4.5)
    base_ground(c, tufts=False)
    house(c, 3, HZ, 8, 5)
    house(c, 20, HZ, 9, 6)
    fence(c, 12, 18, HZ)
    tree2(c, 30, HZ, 2, 3)
    c.rect(13, 4, 17, 4, 1)  # cloud
    c.rect(12, 5, 19, 5, 1)
    for x in range(2, W, 7):  # path stones in the band
        c.rect(x, HZ + 3, x + 2, HZ + 3, 0)
    return c


def sunlit_woods():
    c = forest2()
    for x, y in ((9, 3), (11, 4), (21, 2)):  # sun rays through the canopy gaps
        c.line(x, y, x + 2, y + 6, 0)
    return c


def cafe_boulevard():
    c = C(0)
    c.rect(22, 2, 23, HZ - 1, 1)   # a tall tower in the distance (light: far)
    c.rect(21, 7, 24, 7, 1)
    c.rect(20, 12, 25, 12, 1)
    c.rect(22, 0, 23, 1, 1)
    c.rect(0, 9, 6, HZ - 1, 1)     # far buildings
    c.rect(27, 10, 31, HZ - 1, 1)
    base_ground(c, tufts=False)
    for x in (4, 14):               # parasols with tables
        c.rect(x - 4, HZ - 8, x + 4, HZ - 8, 3)
        c.rect(x - 3, HZ - 9, x + 3, HZ - 9, 2)
        c.rect(x - 1, HZ - 10, x + 1, HZ - 10, 2)
        for k in range(-4, 5, 2):
            c.set(x + k, HZ - 7, 2)      # scalloped edge
        c.rect(x, HZ - 7, x, HZ - 1, 3)  # pole
        c.rect(x - 2, HZ - 3, x + 2, HZ - 3, 3)  # table top
        c.set(x - 2, HZ - 2, 3)
        c.set(x + 2, HZ - 2, 3)
    c.rect(26, HZ - 4, 28, HZ - 1, 2)  # a cup sign / menu board
    c.rect(27, HZ - 5, 27, HZ - 5, 3)
    return c


def shopping_district():
    c = C(0)
    base_ground(c, tufts=False)
    for i, x0 in enumerate((0, 11, 22)):
        x1 = x0 + 9
        c.rect(x0, 3 + (i % 2) * 2, x1, HZ - 1, 0)
        c.rect(x0, 3 + (i % 2) * 2, x1, 3 + (i % 2) * 2, 3)  # roof line
        c.rect(x0, 3 + (i % 2) * 2, x0, HZ - 1, 3)
        top = 3 + (i % 2) * 2
        c.rect(x0 + 2, top + 2, x0 + 7, top + 3, 1)          # sign board
        c.rect(x0 + 3, top + 2, x0 + 3, top + 3, 3)
        c.rect(x0 + 5, top + 2, x0 + 6, top + 2, 3)
        ay = HZ - 7                                          # striped awning
        for x in range(x0, x1 + 1):
            c.set(x, ay, 3 if (x // 2) % 2 else 2)
            c.set(x, ay + 1, 3 if (x // 2) % 2 else 2)
        for x in range(x0, x1 + 1, 2):
            c.set(x, ay + 2, 2)
        c.rect(x0 + 2, HZ - 4, x0 + 4, HZ - 1, 1)            # shop window
        c.rect(x0 + 6, HZ - 4, x0 + 7, HZ - 1, 3)            # door
    return c


def palace_riverside():
    c = C(0)
    # palace on the far bank: light (far), symmetrical, with domes and flags
    c.rect(8, 7, 24, 12, 1)
    for x in (8, 15, 22):
        c.rect(x, 4, x + 2, 12, 1)
        c.set(x + 1, 3, 1)
        c.set(x + 1, 2, 2)        # flag poles
        c.set(x + 2, 2, 2)
    for x in range(10, 23, 3):
        c.set(x, 9, 0)            # windows
    c.rect(0, 12, W - 1, 12, 2)   # far bank
    c.rect(0, 13, W - 1, H - 1, 1)
    for y, xs in ((15, (3, 13, 24)), (18, (7, 18, 28)), (21, (1, 11, 21)), (23, (5, 15, 26))):
        for x in xs:
            c.rect(x, y, x + 2 + (y - 15) // 3, y, 0)
    c.poly([(0, 24), (0, 18), (5, 19), (8, 24)], 2)
    for x, h in ((1, 6), (3, 5)):
        c.rect(x, 24 - h, x, 23, 3)
    tree2(c, 29, 20, 2, 3)
    return c


def dojo_grounds():
    c = C(0)
    far_hills(c, 11, 1.0, 5.0)
    base_ground(c, tufts=False)
    # dojo: wide sweeping roof, dark walls, a sign above the door
    c.rect(4, 8, 22, 8, 3)
    c.rect(5, 7, 21, 7, 2)
    c.rect(7, 6, 19, 6, 2)
    c.set(3, 9, 3)
    c.set(23, 9, 3)              # roof tips curl up
    c.set(4, 9, 3)
    c.set(22, 9, 3)
    c.rect(6, 9, 20, HZ - 1, 2)
    c.rect(6, 9, 20, 9, 3)
    for x in range(7, 20, 3):
        c.rect(x, 11, x + 1, 13, 1)  # paper windows
    c.rect(12, HZ - 4, 14, HZ - 1, 3)  # door
    c.rect(11, 10, 15, 10, 0)          # sign
    # training post with a rope
    c.rect(27, HZ - 7, 28, HZ - 1, 3)
    c.rect(26, HZ - 5, 29, HZ - 5, 2)
    c.rect(26, HZ - 3, 29, HZ - 3, 2)
    return c


def seashell_coast():
    c = beach2()
    for x, y in ((14, 21), (26, 20)):  # shells: fans with ridges
        c.rect(x - 1, y, x + 1, y, 2)
        c.set(x - 2, y + 1, 2)
        c.set(x, y + 1, 3)
        c.set(x + 2, y + 1, 2)
        c.set(x, y - 1, 1)
    return c


def crystal(c, x, ground, h, v=1):
    for i in range(h):
        half = 1 if i < h - 2 else 0
        c.rect(x - half, ground - 1 - i, x + half, ground - 1 - i, v)
    c.set(x + 1, ground - 2, 3)          # dark facet
    c.set(x - 1, ground - 3, 0)          # glint


def crystal_cliffs():
    c = cliffs2()
    for x, h in ((6, 5), (8, 3), (24, 6), (27, 4), (15, 3)):
        crystal(c, x, HZ, h, 0)
    for x, h in ((2, 4), (30, 3)):       # crystals in the crags
        crystal(c, x, 12, h, 0)
    return c


def rusted_works():
    c = C(0)
    for x, h in ((22, 14), (26, 12)):    # smokestacks with smoke
        c.rect(x, HZ - h, x + 2, HZ - 1, 2)
        c.rect(x, HZ - h, x + 2, HZ - h, 3)
        c.set(x + 1, HZ - h - 2, 1)
        c.rect(x + 2, HZ - h - 4, x + 4, HZ - h - 3, 1)
        c.rect(x + 4, HZ - h - 6, x + 7, HZ - h - 5, 1)
    base_ground(c, tufts=False)
    # factory hall with a saw-tooth roof, broken windows, pipes
    c.rect(1, 8, 20, HZ - 1, 2)
    for x in range(1, 21, 5):
        c.line(x, 8, x + 4, 5, 3)
        c.line(x + 4, 5, x + 4, 8, 3)
    for x in range(3, 19, 4):
        c.rect(x, 11, x + 2, 13, 1)
    c.set(4, 12, 3)
    c.set(11, 11, 3)
    c.set(16, 13, 3)                     # broken panes
    c.rect(9, HZ - 4, 12, HZ - 1, 3)     # gate
    c.rect(20, 13, 27, 13, 3)            # pipe to the stacks
    c.rect(20, 15, 23, 15, 3)
    for x in (5, 17, 29):                # rubble and weeds
        c.set(x, HZ + 2, 2)
        c.set(x + 1, HZ + 2, 3)
    return c


def shadowed_thicket():
    c = C(1)                             # gloomy sky: light grey
    for x in (3, 13, 22, 30):            # far dead trees
        c.rect(x, 7, x, HZ - 1, 2)
        c.line(x, 10, x - 2, 8, 2)
        c.line(x, 9, x + 2, 7, 2)
    c.rect(0, HZ, W - 1, HZ, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 2)
    for x in (7, 26):                    # near twisted trees
        c.rect(x, 4, x + 1, HZ - 1, 3)
        c.line(x, 8, x - 4, 4, 3)
        c.line(x + 1, 6, x + 5, 2, 3)
        c.line(x - 4, 4, x - 5, 2, 3)
        c.line(x + 5, 2, x + 6, 3, 3)
        c.line(x + 1, 11, x + 4, 9, 3)
    for x, y in ((15, 12), (19, 10)):    # glowing eyes in the dark between
        c.set(x, y, 0)
        c.set(x + 2, y, 0)
    c.disc(17, 11, 0, 3)
    c.rect(13, 9, 22, 13, c.get(13, 9))  # keep sky
    c.set(15, 12, 0)
    c.set(17, 12, 0)
    c.set(20, 10, 0)
    c.set(22, 10, 0)
    c.rect(10, 14, 23, HZ - 1, 3)        # dark bushes the eyes peek from
    c.set(14, 15, 0)
    c.set(16, 15, 0)
    c.set(19, 16, 0)
    c.set(21, 16, 0)
    return c


def frost_ridge():
    return snow2()


def grand_estate():
    c = C(0)
    # mansion: wide, two floors, central gable, many windows
    c.rect(5, 5, 26, 13, 1)
    c.rect(5, 5, 26, 5, 3)
    for i in range(3):
        c.rect(13 + i, 4 - i, 18 - i, 4 - i, 3)
    c.rect(4, 5, 27, 5, 3)
    for x in range(7, 25, 3):
        c.rect(x, 7, x + 1, 8, 2)
        c.rect(x, 10, x + 1, 11, 2)
    c.rect(15, 10, 16, 13, 3)            # front door
    base_ground(c, tufts=False)
    # iron gate and hedges in front
    for x in range(0, W, 2):
        c.rect(x, 13, x, HZ - 1, 3)
    c.rect(0, 14, W - 1, 14, 3)
    for x in range(1, W, 4):
        c.set(x, 12, 3)                   # spear tips
    c.rect(0, 15, 7, HZ - 1, 2)           # hedges
    c.rect(24, 15, 31, HZ - 1, 2)
    c.set(2, 15, 1)
    c.set(26, 15, 1)
    return c


def starters_lane():
    c = C(0)
    far_hills(c, 12, 1.5, 4.0, 2.0)
    base_ground(c)
    c.poly([(14, 18), (18, 18), (22, 24), (10, 24)], 0)   # the path leading away
    # signpost
    c.rect(24, HZ - 8, 24, HZ - 1, 3)
    c.rect(21, HZ - 8, 28, HZ - 6, 2)
    c.rect(21, HZ - 8, 28, HZ - 8, 3)
    c.set(28, HZ - 7, 3)
    c.set(29, HZ - 7, 3)                                  # arrow tip
    c.rect(22, HZ - 7, 26, HZ - 7, 1)
    tree2(c, 6, HZ, 3, 4)
    for x in (11, 19):                                    # three little starter tufts: leaf, flame, droplet
        c.set(x, HZ - 1, 3)
    c.set(2, HZ - 2, 2)
    c.set(2, HZ - 1, 3)
    return c


def bone(c, x, y, ln, v=0):
    c.rect(x, y, x + ln, y, v)
    for xx in (x - 1, x + ln + 1):
        c.set(xx, y - 1, v)
        c.set(xx, y + 1, v)


def fossil_dig_site():
    c = C(0)
    far_hills(c, 10, 1.0, 4.0)
    c.rect(0, 12, W - 1, H - 1, 2)                         # exposed rock face
    c.rect(0, 12, W - 1, 12, 3)
    for y in (15, 19, 22):                                 # strata
        c.line(0, y, W - 1, y + (1 if y % 2 else -1), 3)
    bone(c, 5, 17, 5)                                      # bones in the rock
    bone(c, 20, 20, 4)
    c.disc(14, 17, 2, 1)                                   # an ammonite-like spiral
    c.set(14, 17, 3)
    c.set(15, 16, 3)
    c.set(13, 18, 2)
    # pickaxe stuck in the top
    c.line(25, 13, 28, 8, 3)
    c.line(26, 7, 30, 9, 3)
    c.set(25, 7, 3)
    # little flags marking the dig
    for x in (3, 10):
        c.rect(x, 7, x, 11, 3)
        c.rect(x + 1, 7, x + 2, 8, 1)
    return c


def old_battlefield():
    c = C(0)
    far_hills(c, 12, 1.0, 5.0, 1.0)
    base_ground(c, tufts=False)
    for x, h, broken in ((4, 12, 2), (11, 8, 1), (24, 13, 3)):   # ruined pillars
        c.rect(x, HZ - h, x + 2, HZ - 1, 2)
        c.rect(x + 2, HZ - h, x + 2, HZ - 1, 3)
        c.rect(x - 1, HZ - 1, x + 3, HZ - 1, 3)                   # base
        for k in range(broken):                                    # jagged broken top
            c.set(x + k, HZ - h - 1 + k, 0)
        c.set(x, HZ - h + 2, 1)
    c.rect(15, HZ - 3, 21, HZ - 1, 2)                              # fallen block
    c.rect(15, HZ - 3, 21, HZ - 3, 3)
    c.line(28, HZ - 1, 31, HZ - 6, 3)                              # a planted old sword
    c.rect(28, HZ - 5, 31, HZ - 5, 3)
    c.set(31, HZ - 7, 1)
    for x in (2, 14, 22):
        c.set(x, HZ + 2, 2)
    return c


def stall(c, x, ground, w=7):
    for k in range(w):                                    # striped canopy
        c.set(x + k, ground - 7, 3 if k % 2 else 2)
        c.set(x + k, ground - 6, 3 if k % 2 else 2)
    c.rect(x, ground - 5, x, ground - 1, 3)
    c.rect(x + w - 1, ground - 5, x + w - 1, ground - 1, 3)
    c.rect(x, ground - 3, x + w - 1, ground - 3, 2)       # counter
    c.set(x + 2, ground - 4, 1)
    c.set(x + 4, ground - 4, 2)                           # goods


def traders_crossing():
    c = C(0)
    far_hills(c, 11, 1.0, 4.5)
    c.rect(0, HZ, W - 1, HZ, 2)
    c.rect(0, HZ + 1, W - 1, H - 1, 1)
    c.poly([(14, 11), (18, 11), (24, 24), (8, 24)], 0)    # the road coming toward us
    c.rect(0, 14, W - 1, 15, 0)                            # the crossing road
    c.rect(0, 14, W - 1, 14, 2)
    stall(c, 1, 14, 7)
    stall(c, 23, 14, 8)
    c.rect(15, 5, 16, 11, 3)                               # signpost at the crossing
    c.rect(12, 6, 19, 7, 2)
    c.rect(13, 9, 18, 9, 2)
    return c


def wyrm_hollow():
    c = C(3)
    c.poly([(2, 24), (3, 8), (8, 3), (16, 1), (24, 3), (29, 8), (30, 24)], 2)
    c.poly([(8, 24), (9, 10), (13, 6), (19, 6), (23, 10), (24, 24)], 1)
    c.rect(0, 21, W - 1, 21, 3)
    for x, h in ((6, 9), (11, 6), (21, 7), (26, 10)):     # rock spires like horns / claws
        for i in range(h):
            half = 1 if i < h // 2 else 0
            c.rect(x - half, 21 - i, x + half, 21 - i, 3)
        c.set(x - 1, 21 - h // 2, 2)
    # a dragon silhouette's eye glinting in the dark at the back
    c.rect(14, 12, 18, 14, 2)
    c.set(15, 12, 0)
    c.set(17, 12, 0)
    c.rect(13, 10, 13, 11, 2)
    c.rect(19, 10, 19, 11, 2)                              # horns
    for x in (5, 27):
        c.set(x, 15, 0)                                    # glints on the scales of the walls
    return c


def elite_bazaar():
    c = C(0)
    base_ground(c, tufts=False)
    # grand tent: a high peaked top, striped sides, banners
    for i in range(10):
        half = 2 + i
        y = 3 + i
        for x in range(16 - half, 16 + half + 1):
            c.set(x, y, 3 if (x - 16) % 4 in (0, 1) else 2)
    c.rect(4, 13, 28, HZ - 1, 0)
    for x in range(4, 29, 3):
        c.rect(x, 13, x, HZ - 1, 2)
    c.rect(4, 13, 28, 13, 3)
    c.rect(13, 14, 18, HZ - 1, 3)                          # entrance
    c.rect(14, 15, 17, HZ - 1, 2)
    c.set(16, 1, 3)
    c.set(16, 2, 3)
    c.rect(17, 1, 19, 2, 2)                                # top flag
    for x in (1, 30):                                      # banners on poles
        c.rect(x, 4, x, HZ - 1, 3)
        c.rect(x + (1 if x < 16 else -2), 4, x + (2 if x < 16 else -1), 8, 2)
        c.set(x + (1 if x < 16 else -1), 9, 2)
    return c


# --- round 4 (user: "some of those location icons look a bit sloppy and hard to read"): the six weakest redrawn
# with fewer, bigger shapes - a dark subject against the white sky, little light-grey clutter.

def house3(c, x, ground, w=9, wall=5):
    """Clear house: black-outlined white walls, a solid dark triangular roof, one window, one door."""
    top = ground - wall
    c.rect(x, top, x + w - 1, ground - 1, 0)
    c.rect(x, top, x, ground - 1, 3)
    c.rect(x + w - 1, top, x + w - 1, ground - 1, 3)
    peak = (w + 1) // 2
    for i in range(peak + 1):  # roof
        c.rect(x - 1 + i, top - 1 - i, x + w - i, top - 1 - i, 2)
        c.set(x - 1 + i, top - 1 - i, 3)
        c.set(x + w - i, top - 1 - i, 3)
    c.rect(x - 1, top - 1, x + w, top - 1, 3)
    c.rect(x + 1, top + 1, x + 3, top + 2, 2)     # window
    c.set(x + 2, top + 1, 1)
    c.rect(x + w - 4, ground - 3, x + w - 3, ground - 1, 3)  # door


def quiet_outskirts3():
    c = C(0)
    far_hills(c, 13, 1.0, 5.0)
    base_ground(c, tufts=False)
    house3(c, 2, HZ, 9, 5)
    house3(c, 19, HZ, 9, 6)
    for x in range(12, 18, 2):                    # short picket fence between them
        c.rect(x, HZ - 3, x, HZ - 1, 3)
    c.rect(12, HZ - 2, 17, HZ - 2, 3)
    tree2(c, 30, HZ, 2, 3)
    c.rect(4, 3, 9, 3, 1)                          # a cloud
    c.rect(3, 4, 11, 4, 1)
    return c


def big_crystal(c, x, ground, h, w=1):
    """Tall crystal: white body, black outline, dark facet on the right, pointed top."""
    for i in range(h):
        y = ground - 1 - i
        half = w if i < h - 2 else (1 if i == h - 2 else 0)
        c.rect(x - half, y, x + half, y, 0)
        c.set(x - half - 1, y, 3)
        c.set(x + half + 1, y, 3)
        if half:
            c.set(x + half, y, 2)                  # facet
    c.set(x, ground - h - 1, 3)


def crystal_cliffs3():
    c = C(0)
    c.poly([(0, HZ), (6, 8), (11, 12), (16, 5), (22, 11), (27, 7), (32, 10), (32, HZ)], 1)  # far cliffs
    base_ground(c, tufts=False)
    c.rect(9, HZ - 2, 23, HZ - 1, 2)               # rock the crystals grow from
    c.rect(9, HZ - 2, 23, HZ - 2, 3)
    big_crystal(c, 16, HZ - 2, 11, 2)              # cluster: a tall one in the middle
    big_crystal(c, 11, HZ - 2, 7, 1)
    big_crystal(c, 21, HZ - 2, 8, 1)
    big_crystal(c, 5, HZ, 4, 1)                    # small ones on the ground
    big_crystal(c, 27, HZ, 5, 1)
    return c


def fossil_dig_site3():
    c = C(0)
    far_hills(c, 11, 1.0, 4.0)
    c.poly([(2, HZ), (6, 9), (26, 9), (30, HZ)], 2)   # a dug-out rock mound
    c.rect(6, 9, 26, 9, 3)
    c.rect(0, HZ, W - 1, HZ, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 1)
    # a large clear bone in the rock, white with black outline
    for x in range(9, 20):
        c.set(x, 13, 0)
        c.set(x, 12, 3)
        c.set(x, 14, 3)
    for cx in (8, 20):
        c.rect(cx - 1, 11, cx + 1, 15, 0)
        c.set(cx, 13, 0)
        for y in (10, 16):
            c.rect(cx - 1, y, cx + 1, y, 3)
        c.rect(cx - 2, 11, cx - 2, 15, 3) if cx == 8 else c.rect(cx + 2, 11, cx + 2, 15, 3)
    c.disc(24, 13, 2, 0)                            # an ammonite: white spiral shell
    c.set(24, 13, 3)
    c.set(25, 12, 3)
    c.set(23, 14, 3)
    # pickaxe leaning on the mound
    c.line(27, HZ - 1, 30, HZ - 7, 3)
    c.line(28, HZ - 8, 32, HZ - 6, 3)
    return c


def palace_riverside3():
    c = C(0)
    # palace on the far bank, now dark enough to read: main hall, two towers with pointed roofs, a centre dome
    c.rect(8, 6, 24, 11, 2)
    c.rect(8, 6, 24, 6, 3)
    for x in (6, 24):
        c.rect(x, 4, x + 2, 11, 2)
        c.rect(x, 4, x + 2, 4, 3)
        c.set(x + 1, 3, 3)
        c.set(x + 1, 2, 3)
    c.disc(16, 5, 2, 2)
    c.set(16, 2, 3)
    for x in range(10, 23, 3):
        c.rect(x, 8, x, 9, 0)                        # lit windows
    c.rect(15, 9, 17, 11, 3)                         # gate
    c.rect(0, 12, W - 1, 12, 3)                      # the bank
    c.rect(0, 13, W - 1, H - 1, 1)                   # the river
    for y, xs in ((15, (4, 20)), (18, (10, 26)), (21, (2, 16)), (23, (8, 23))):
        for x in xs:
            c.rect(x, y, x + 3, y, 0)
    for x, h in ((1, 6), (30, 5)):
        c.rect(x, 24 - h, x, 23, 3)
    return c


def seashell_coast3():
    c = C(0)
    c.disc(25, 4, 2.5, 1)
    c.disc(25, 4, 1.5, 0)
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, 15, 1)
    for x in range(1, W, 6):
        c.rect(x, 13, x + 2, 13, 0)
    c.rect(0, 16, W - 1, 16, 0)
    # a clear palm: thick trunk, four broad fronds
    for i, (x, y) in enumerate(((5, 23), (5, 22), (6, 21), (6, 20), (6, 19), (7, 18), (7, 17), (7, 16), (8, 15),
                                (8, 14), (8, 13), (9, 12))):
        c.set(x, y, 3)
        c.set(x + 1, y, 2)
    for dx, pts in ((-1, ((0, 0), (1, 0), (2, 1), (3, 1), (4, 2), (5, 3))), (1, ((0, 0), (1, 0), (2, 1), (3, 1),
                                                                              (4, 2), (5, 3)))):
        for k, (a, b) in enumerate(pts):
            c.set(9 + dx * a, 10 + b, 3)
            c.set(9 + dx * a, 9 + b, 2)              # broad leaf: two pixels thick
    c.rect(7, 9, 11, 10, 3)
    c.set(8, 9, 1)
    # two big shells on the sand: fan shape with ridges
    for sx in (18, 26):
        for i in range(3):
            c.rect(sx - 2 + i, 20 - i, sx + 2 - i, 20 - i, 0)
        c.rect(sx - 3, 21, sx + 3, 21, 3)
        for k in (-2, 0, 2):
            c.set(sx + k, 20, 2)
        c.set(sx - 3, 20, 3)
        c.set(sx + 3, 20, 3)
        c.set(sx - 2, 19, 3)
        c.set(sx + 2, 19, 3)
        c.set(sx - 1, 18, 3)
        c.set(sx + 1, 18, 3)
        c.set(sx, 17, 3)
    return c


def snow_pine(c, x, ground, h):
    """Solid dark pine in three tiers, each tier capped with white snow and a black shadow edge."""
    tiers = 3
    th = h // tiers + 1
    for t in range(tiers):
        y0 = ground - h + t * (h // tiers)
        for i in range(th):
            half = 1 + i + t
            y = y0 + i
            c.rect(x - half, y, x + half, y, 2)
            c.set(x + half, y, 3)
            if i == 0:
                c.rect(x - half, y, x + half - 1, y, 0)   # snow cap
    c.rect(x, ground, x, ground + 1, 3)


def frost_ridge3():
    c = C(0)
    c.poly([(0, 13), (7, 5), (12, 8), (19, 1), (26, 7), (32, 4), (32, HZ), (0, HZ)], 1)
    c.poly([(5, 7), (7, 5), (9, 7), (8, 8), (6, 8)], 0)
    c.poly([(17, 3), (19, 1), (21, 3), (20, 4), (18, 4)], 0)
    c.poly([(30, 5), (32, 4), (32, 6)], 0)
    c.rect(0, HZ, W - 1, HZ, 1)
    c.rect(0, HZ + 1, W - 1, H - 1, 0)
    for x in range(3, W, 6):
        c.rect(x, HZ + 3, x + 2, HZ + 3, 1)
    for x, base, h in ((6, HZ, 13), (26, HZ, 14), (16, HZ - 1, 9)):   # the Sunlit Woods pine (reads well) ...
        pine2(c, x, base, h)
        before = [row[:] for row in c.p]                              # check against the tree before any snow
        g = lambda xx, yy: before[yy][xx] if 0 <= xx < W and 0 <= yy < H else 0  # noqa: E731
        for yy in range(base - h - 1, base):                          # ... with snow on the top edges, left half
            for xx in range(x - 6, x + 1):
                if g(xx, yy) in (2, 3) and g(xx, yy - 1) in (0, 1):
                    c.set(xx, yy, 0)
    for x, y in ((2, 2), (12, 3), (24, 2), (29, 12), (13, 13)):
        c.set(x, y, 1)
    return c


COURSES = {
    "quiet_outskirts": quiet_outskirts, "sunlit_woods": sunlit_woods, "cafe_boulevard": cafe_boulevard,
    "shopping_district": shopping_district, "palace_riverside": palace_riverside, "dojo_grounds": dojo_grounds,
    "seashell_coast": seashell_coast, "crystal_cliffs": crystal_cliffs, "rusted_works": rusted_works,
    "shadowed_thicket": shadowed_thicket, "frost_ridge": frost_ridge, "grand_estate": grand_estate,
    "starters_lane": starters_lane, "fossil_dig_site": fossil_dig_site, "old_battlefield": old_battlefield,
    "traders_crossing": traders_crossing, "wyrm_hollow": wyrm_hollow, "elite_bazaar": elite_bazaar,
}
# --- round 5 (user marked 10 with red dots: "I have no idea what they are and they need a complete redesign";
# "the haunted forest trees look a little goofy"): one big unmistakable subject each; tiny 3x5 letters on signs
# where a building alone would be ambiguous (as the games label places).
FONT3 = {
    "A": [".#.", "#.#", "###", "#.#", "#.#"], "C": ["###", "#..", "#..", "#..", "###"],
    "D": ["##.", "#.#", "#.#", "#.#", "##."], "E": ["###", "#..", "##.", "#..", "###"],
    "F": ["###", "#..", "##.", "#..", "#.."], "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "J": ["..#", "..#", "..#", "#.#", "###"], "O": ["###", "#.#", "#.#", "#.#", "###"],
    "P": ["###", "#.#", "###", "#..", "#.."], "S": ["###", "#..", "###", "..#", "###"],
}


def text3(c, x, y, s, v=0):
    for ch in s:
        for r, row in enumerate(FONT3[ch]):
            for k, p in enumerate(row):
                if p == "#":
                    c.set(x + k, y + r, v)
        x += 4


def thick(c, x0, y0, x1, y1, v, w=2):
    for d in range(w):
        c.line(x0 + d, y0, x1 + d, y1, v)


def awning(c, x0, x1, y, rows=2):
    for x in range(x0, x1 + 1):
        for r in range(rows):
            c.set(x, y + r, 3 if (x // 2) % 2 else 2)
    for x in range(x0, x1 + 1, 2):
        c.set(x, y + rows, 3 if (x // 2) % 2 else 2)


def cafe_boulevard5():
    c = C(0)
    base_ground(c, tufts=False)
    c.rect(3, 2, 28, HZ - 1, 0)                 # café front
    c.rect(3, 2, 28, 2, 3)
    c.rect(3, 2, 3, HZ - 1, 3)
    c.rect(28, 2, 28, HZ - 1, 3)
    c.rect(7, 3, 24, 9, 2)                      # sign board with CAFE
    c.rect(7, 3, 24, 3, 3)
    text3(c, 8, 4, "CAFE", 0)
    awning(c, 2, 29, 10)
    c.rect(5, 13, 16, HZ - 1, 1)                # big window
    c.rect(5, 13, 16, 13, 3)
    c.rect(10, 13, 10, HZ - 1, 3)
    c.rect(12, 15, 14, 16, 3)                   # a cup in the window
    c.set(15, 15, 3)
    c.rect(20, 13, 24, HZ - 1, 3)               # door
    c.set(23, 15, 1)
    return c


def shopping_district5():
    c = C(0)
    base_ground(c, tufts=False)
    c.rect(2, 2, 25, HZ - 1, 0)                 # store front
    c.rect(2, 2, 25, 2, 3)
    c.rect(2, 2, 2, HZ - 1, 3)
    c.rect(25, 2, 25, HZ - 1, 3)
    c.rect(5, 3, 22, 9, 3)                      # dark sign with SHOP
    text3(c, 6, 4, "SHOP", 0)
    awning(c, 1, 26, 10)
    c.rect(4, 13, 15, HZ - 1, 1)                # display window with goods
    c.rect(4, 13, 15, 13, 3)
    for x in (6, 10, 13):
        c.rect(x, 15, x + 1, 17, 2)
    c.rect(18, 13, 22, HZ - 1, 3)               # door
    # a big shopping bag in front, right
    c.rect(26, 11, 31, HZ - 1, 0)
    c.rect(26, 11, 31, 11, 3)
    c.rect(26, 11, 26, HZ - 1, 3)
    c.rect(31, 11, 31, HZ - 1, 3)
    c.rect(26, HZ - 1, 31, HZ - 1, 3)
    c.set(27, 10, 3)
    c.set(28, 9, 3)
    c.set(29, 9, 3)
    c.set(30, 10, 3)                            # handle
    c.rect(28, 13, 29, 15, 2)                   # bag logo
    return c


def palace_riverside5():
    c = C(0)
    c.rect(0, 14, W - 1, 14, 3)                 # bank
    c.rect(0, 15, W - 1, H - 1, 1)              # river
    for y, xs in ((17, (3, 19)), (19, (9, 25)), (21, (1, 15)), (23, (7, 22))):
        for x in xs:
            c.rect(x, y, x + 3, y, 0)
    # palace: white classical front with black columns, dark dome, two towers
    c.rect(4, 7, 27, 13, 0)
    c.rect(4, 7, 27, 7, 3)
    c.rect(4, 13, 27, 13, 3)
    c.rect(4, 7, 4, 13, 3)
    c.rect(27, 7, 27, 13, 3)
    for x in range(7, 26, 3):
        c.rect(x, 8, x, 12, 2)                  # columns
    c.rect(14, 10, 17, 13, 3)                   # grand door
    for i in range(4):                          # pediment above the door
        c.rect(12 + i, 6 - i, 19 - i, 6 - i, 3)
    c.disc(15.5, 3, 2.5, 2)                     # dome
    c.set(15, 0, 3)
    c.set(16, 0, 3)
    for x in (1, 27):                           # towers
        c.rect(x, 4, x + 3, 13, 2)
        c.rect(x, 4, x + 3, 4, 3)
        c.set(x + 1, 3, 3)
        c.set(x + 2, 3, 3)
        c.set(x + 1, 2, 3)
        c.set(x + 2, 2, 3)
        c.rect(x + 1, 7, x + 2, 9, 0)           # tower window
    return c


def dojo_grounds5():
    c = C(0)
    far_hills(c, 12, 1.0, 5.0)
    base_ground(c, tufts=False)
    # two-tier roof with upturned tips
    for y, x0, x1 in ((2, 9, 22), (3, 7, 24), (4, 5, 26)):
        c.rect(x0, y, x1, y, 2)
    c.rect(5, 4, 26, 4, 3)
    c.set(4, 3, 3)
    c.set(27, 3, 3)
    for y, x0, x1 in ((7, 6, 25), (8, 4, 27), (9, 2, 29)):
        c.rect(x0, y, x1, y, 2)
    c.rect(2, 9, 29, 9, 3)
    c.set(1, 8, 3)
    c.set(30, 8, 3)
    c.rect(8, 5, 23, 6, 2)                       # wall between the tiers
    c.rect(4, 10, 27, HZ - 1, 0)                 # walls with sliding doors
    c.rect(4, 10, 4, HZ - 1, 3)
    c.rect(27, 10, 27, HZ - 1, 3)
    for x in range(8, 25, 4):
        c.rect(x, 13, x, HZ - 1, 2)
    c.rect(9, 10, 22, 12, 3)                     # DOJO plate
    text3(c, 9, 10, "DOJO", 0) if False else None
    c.rect(8, 10, 23, 16, 3)
    text3(c, 8, 11, "DOJO", 0)
    return c


def crystal_cliffs5():
    c = C(0)
    c.poly([(0, HZ), (0, 3), (6, 1), (12, 4), (20, 2), (26, 5), (32, 2), (32, HZ)], 2)   # dark cliff wall
    for y in (6, 10, 14):
        c.line(0, y, 7, y + 1, 3)
        c.line(24, y + 1, 31, y, 3)
    base_ground(c, tufts=False)

    def prism(x, base, h, lean):
        for i in range(h):
            y = base - 1 - i
            xo = x + (lean * i) // 4
            half = 1 if i < h - 2 else 0
            c.rect(xo - half - 1, y, xo + half + 1, y, 3)       # outline
            c.rect(xo - half, y, xo + half, y, 0)               # white body
            if half:
                c.set(xo + half, y, 1)                          # facet
        c.set(x + (lean * h) // 4, base - 1 - h, 3)

    prism(16, HZ, 12, 0)
    prism(12, HZ, 9, -2)
    prism(20, HZ, 9, 2)
    prism(8, HZ, 5, -1)
    prism(24, HZ, 6, 1)
    for x, y in ((5, 5), (27, 8), (10, 2)):                    # sparkles (4-point stars)
        c.set(x, y, 0)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 1)
    return c


def fossil_dig_site5():
    c = C(0)
    far_hills(c, 9, 1.0, 4.0)
    c.rect(0, 10, W - 1, HZ, 2)                  # rock layer
    c.rect(0, 10, W - 1, 10, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 1)
    # dinosaur skeleton, white: skull (left) with eye socket and teeth, spine arching right, ribs, tail
    c.rect(2, 12, 7, 15, 0)                      # skull
    c.set(4, 13, 3)                              # eye socket
    for x in (3, 5, 7):
        c.set(x, 16, 0)                          # teeth
    c.rect(2, 15, 7, 15, 3)
    for x in range(8, 26):                       # spine (a gentle arch)
        y = 12 + int(round(((x - 17) / 9.0) ** 2 * 3))
        c.set(x, y, 0)
        if x % 3 == 1 and 10 <= x <= 22:         # ribs
            c.rect(x, y + 1, x, y + 3, 0)
    for x in range(26, 31):                      # tail going down
        c.set(x, 15 + (x - 26) // 2, 0)
    # shovel stuck in the ground at the right
    c.rect(30, 2, 30, 9, 3)
    c.rect(29, 1, 31, 1, 3)
    c.rect(29, 9, 31, 11, 3)
    return c


def old_battlefield5():
    c = C(0)
    # far ruins: a broken arch and wall, light grey
    c.rect(2, 8, 9, HZ - 1, 1)
    c.rect(4, 11, 7, HZ - 1, 0)
    c.disc(5.5, 11, 1.6, 0)
    c.rect(22, 10, 30, HZ - 1, 1)
    for x in range(22, 31, 2):
        c.set(x, 9, 1)
    base_ground(c, tufts=False)
    c.poly([(9, HZ + 1), (12, 15), (20, 15), (23, HZ + 1)], 2)   # mound
    # two crossed swords planted in the mound: white blades with black edges, dark hilts
    for (x0, y0, x1, y1) in ((10, 2, 18, 15), (22, 2, 14, 15)):
        c.line(x0, y0, x1, y1, 3)
        c.line(x0 + 1, y0, x1 + 1, y1, 0)
        c.line(x0 + 2, y0, x1 + 2, y1, 3)
    for hx in (10, 21):                                          # cross-guards and grips
        c.rect(hx - 1, 4, hx + 3, 4, 3)
        c.rect(hx + 1, 1, hx + 1, 3, 2)
    # a round shield leaning on the mound
    c.disc(25, 16, 2.5, 2)
    c.set(25, 16, 0)
    return c


def traders_crossing5():
    c = C(0)
    far_hills(c, 12, 1.0, 4.5)
    base_ground(c, tufts=False)
    # covered merchant wagon: arched white canvas with ribs, wooden bed, two spoked wheels, a shaft
    for x in range(3, 20):
        top = 5 + int(round(((x - 11) / 8.0) ** 2 * 4))      # the arch of the canvas
        c.rect(x, top, x, 12, 0)
        c.set(x, top, 3)
    c.rect(3, 9, 3, 12, 3)
    c.rect(19, 9, 19, 12, 3)
    for x in (7, 11, 15):                                    # canvas ribs
        top = 5 + int(round(((x - 11) / 8.0) ** 2 * 4))
        c.rect(x, top + 1, x, 12, 1)
    c.rect(2, 12, 20, 13, 2)                                 # bed
    c.rect(2, 12, 20, 12, 3)
    for wx in (6, 16):                                       # wheels
        c.disc(wx, 15.5, 2.6, 3)
        c.disc(wx, 15.5, 1.5, 0)
        c.set(wx, 15, 3)
        c.set(wx, 16, 3)
    c.line(20, 13, 24, 16, 3)                                # shaft
    # signpost: two arrows pointing opposite ways (the crossing)
    c.rect(26, 4, 26, HZ - 1, 3)
    c.rect(23, 5, 30, 7, 2)
    c.set(31, 6, 2)
    c.rect(22, 9, 29, 11, 2)
    c.set(21, 10, 2)
    c.rect(23, 5, 30, 5, 3)
    c.rect(22, 9, 29, 9, 3)
    return c


def wyrm_hollow5():
    c = C(3)
    c.poly([(0, 24), (0, 6), (6, 1), (26, 1), (32, 6), (32, 24)], 3)
    c.poly([(3, 24), (4, 8), (9, 3), (23, 3), (28, 8), (29, 24)], 1)   # cave inside, lit
    c.rect(0, 21, W - 1, H - 1, 2)
    # dragon head in profile facing right, dark silhouette: horns, brow, snout, open jaw with white teeth, eye
    c.rect(6, 12, 13, 21, 2)                     # neck rising from below
    c.rect(9, 8, 18, 13, 2)                      # head
    c.rect(18, 9, 25, 11, 2)                     # upper snout
    c.rect(18, 13, 24, 14, 2)                    # lower jaw
    for x in (19, 21, 23):
        c.set(x, 12, 0)                          # teeth
    c.set(25, 12, 3)
    c.line(10, 8, 7, 4, 3)                       # horns
    c.line(12, 8, 11, 3, 3)
    c.set(15, 9, 0)                              # eye
    c.set(16, 9, 0)
    c.set(25, 9, 3)                              # nostril
    for y in range(13, 21, 2):                   # scales on the neck
        c.set(8, y, 1)
    return c


def elite_bazaar5():
    c = C(0)
    base_ground(c, tufts=False)
    for x in range(0, W, 2):                     # pennant string
        y = 2 + int(round(1.2 * math.sin(x / 5.0)))
        c.set(x, y, 3)
        if x % 4 == 0:
            c.set(x, y + 1, 2)
            c.set(x + 1, y + 2, 2)
    # big crown sign
    c.rect(11, 5, 21, 9, 2)
    c.rect(11, 9, 21, 9, 3)
    for x in (11, 16, 21):
        c.set(x, 4, 2)
        c.set(x, 3, 3)
    c.set(13, 7, 0)
    c.set(16, 7, 0)
    c.set(19, 7, 0)                              # jewels
    c.rect(16, 10, 16, 11, 3)                    # hanger
    # stall: striped canopy, counter with goods
    awning(c, 3, 28, 12)
    c.rect(4, 15, 4, HZ - 1, 3)
    c.rect(27, 15, 27, HZ - 1, 3)
    c.rect(4, 16, 27, HZ - 1, 0)
    c.rect(4, 16, 27, 16, 3)
    for x in (7, 11, 15, 19, 23):
        c.disc(x, 15, 1, 2)                       # orbs / goods
    return c


def gnarled_tree(c, x, ground, h, lean=1):
    """Crooked bare tree, deliberately asymmetric: a thick trunk bending to one side, one long branch that droops
    and one short one that reaches up, a knot hole, flaring roots."""
    s = 1 if lean >= 0 else -1                      # which way it bends
    pts = []
    for i in range(h):                             # trunk: thick at the bottom, bending in its upper half
        y = ground - 1 - i
        bend = int(round(s * max(0, i - h * 0.45) ** 1.3 * 0.35))
        w = 3 if i < 3 else 2 if i < h * 0.7 else 1
        c.rect(x + bend, y, x + bend + w - 1, y, 3)
        pts.append((x + bend, y))
    tx, ty = pts[-1]
    # long drooping branch to the outside, with a hanging twig
    c.line(tx, ty + 3, tx + s * 4, ty + 1, 3)
    c.line(tx + s * 4, ty + 1, tx + s * 7, ty + 3, 3)
    c.line(tx + s * 7, ty + 3, tx + s * 8, ty + 5, 3)
    c.set(tx + s * 5, ty + 2, 3)
    # short branch reaching up the other way
    c.line(tx, ty + 1, tx - s * 3, ty - 2, 3)
    c.set(tx - s * 4, ty - 2, 3)
    c.set(tx - s * 3, ty - 3, 3)
    c.set(x + 1, ground - 6, 1)                     # knot hole
    c.set(x - 1, ground - 1, 3)
    c.set(x + 3, ground - 1, 3)                     # roots


def shadowed_thicket5():
    c = C(1)                                     # gloomy light-grey sky
    c.disc(25, 5, 3, 0)                          # crescent moon
    c.disc(26.5, 4, 2.6, 1)
    for x in (12, 20):                           # far bare trees, dark grey
        c.rect(x, 9, x, HZ - 1, 2)
        c.line(x, 11, x - 2, 9, 2)
        c.line(x, 10, x + 2, 8, 2)
    c.rect(0, HZ, W - 1, HZ, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 2)
    for x in range(0, W, 3):                     # mist along the ground
        c.rect(x, HZ - 1, x + 1, HZ - 1, 0)
    gnarled_tree(c, 5, HZ, 13, 1)
    gnarled_tree(c, 27, HZ, 11, -1)
    c.rect(13, 14, 21, HZ - 1, 3)                # dark bush with two pairs of eyes
    c.rect(14, 13, 20, 13, 3)
    c.set(15, 15, 0)
    c.set(17, 15, 0)
    c.set(18, 16, 1)
    c.set(20, 16, 1)
    return c


COURSES.update({"cafe_boulevard": cafe_boulevard5, "shopping_district": shopping_district5,
                "palace_riverside": palace_riverside5, "dojo_grounds": dojo_grounds5,
                "crystal_cliffs": crystal_cliffs5, "fossil_dig_site": fossil_dig_site5,
                "old_battlefield": old_battlefield5, "traders_crossing": traders_crossing5,
                "wyrm_hollow": wyrm_hollow5, "elite_bazaar": elite_bazaar5, "shadowed_thicket": shadowed_thicket5})

# --- round 6 (user): "the dojo doesn't need dojo text"; cafe boulevard: no text, "make it look like an alleyway
# between shops or a street between shops"; "Crystal cliffs looks weird"; "Shadowed Thicket needs more trees";
# "Fossil Dig Site looks too goofy"; "Wyrm Hollow seems goofy"; Trader's Crossing: a Pokemon trade machine (their
# picture: a screen on top, two wings with Poke Ball pods); "Keep in mind that these are pictures for a pokemon game".

def dojo_grounds6():
    c = dojo_grounds5()
    c.rect(4, 10, 27, HZ - 1, 0)                 # walls again, without the plate / text
    c.rect(4, 10, 4, HZ - 1, 3)
    c.rect(27, 10, 27, HZ - 1, 3)
    for x in range(7, 26, 3):
        c.rect(x, 11, x, HZ - 1, 2)              # sliding door frames
    c.rect(5, 13, 26, 13, 2)                     # door rail
    c.rect(13, 12, 18, HZ - 1, 3)                # the open entrance
    return c


def poke_ball(c, cx, cy, r=3):
    c.disc(cx, cy, r, 0)
    for y in range(int(cy - r), int(cy)):        # top half dark
        for x in range(int(cx - r), int(cx + r) + 1):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                c.set(x, y, 2)
    c.rect(int(cx - r), int(cy), int(cx + r), int(cy), 3)   # band
    c.set(cx, cy, 0)
    c.set(cx - 1, cy, 3)
    c.set(cx + 1, cy, 3)
    for t in range(16):                          # outline
        a = t * math.pi / 8
        c.set(cx + (r + 0.4) * math.cos(a), cy + (r + 0.4) * math.sin(a), 3)


def cafe_boulevard6():
    c = C(0)
    vx, vy = 16, 9                                # vanishing point
    c.poly([(0, 0), (12, 6), (12, 15), (0, 21)], 0)        # left shop row (facades in perspective)
    c.poly([(32, 0), (20, 6), (20, 15), (32, 21)], 0)
    c.line(0, 0, 12, 6, 3)
    c.line(32, 0, 20, 6, 3)
    c.line(12, 6, 12, 15, 3)
    c.line(20, 6, 20, 15, 3)
    for i, x in enumerate((1, 5, 9)):            # shop doors, each with its own small awning, smaller with distance
        floor_l = 20 - i * 2                     # bottom of the facade at this x (left row)
        hh = 6 - i * 2
        for side in (0, 1):
            xx = x if side == 0 else 31 - x - 1
            c.rect(xx, floor_l - hh, xx + 1, floor_l - 1, 3)                 # door
            c.rect(xx, floor_l - hh - 4, xx + 1, floor_l - hh - 3, 1)        # upper window
    c.poly([(12, 15), (20, 15), (32, 24), (0, 24)], 1)     # the street
    c.line(0, 21, 12, 15, 3)
    c.line(32, 21, 20, 15, 3)
    for y in (17, 20, 23):
        c.set(16, y, 0)                          # centre dashes
    for (ux, uy) in ((5, 17), (26, 17)):         # café parasols on the pavement
        c.rect(ux - 2, uy, ux + 2, uy, 3)
        c.rect(ux - 1, uy - 1, ux + 1, uy - 1, 2)
        c.rect(ux, uy + 1, ux, uy + 3, 3)
        c.rect(ux - 1, uy + 3, ux + 1, uy + 3, 3)
    c.rect(13, 9, 19, 14, 1)                     # far end of the street: more shops, light (far)
    for x in (14, 17):
        c.rect(x, 11, x, 14, 2)
    return c


def gem(c, cx, base, w, h):
    """Crystal: flat base, straight vertical sides, a pointed top tapering in straight steps; white left face,
    light-grey right face split by a facet line, black outline."""
    tip = w + 1                                   # rows the point takes
    for i in range(h):
        y = base - 1 - i
        half = w if i < h - tip else max(0, w - (i - (h - tip)))
        c.rect(cx - half - 1, y, cx + half + 1, y, 3)
        if half:
            c.rect(cx - half, y, cx - 1, y, 0)
            c.rect(cx + 1, y, cx + half, y, 1)
            c.set(cx, y, 2)                       # the facet ridge
        else:
            c.set(cx, y, 3)
    c.rect(cx - w - 1, base - 1, cx + w + 1, base - 1, 3)


def crystal_cliffs6():
    c = C(0)
    c.poly([(0, HZ), (0, 2), (9, 0), (23, 0), (32, 3), (32, HZ)], 1)        # rock face, light
    for (a, b) in (((2, 4), (7, 9)), ((24, 2), (29, 8)), ((12, 2), (14, 6)), ((4, 12), (9, 14))):
        c.line(a[0], a[1], b[0], b[1], 2)                                  # cracks
    base_ground(c, tufts=False)
    c.rect(6, HZ - 2, 26, HZ - 1, 2)                                         # rock ledge
    c.rect(6, HZ - 2, 26, HZ - 2, 3)
    gem(c, 16, HZ - 2, 2, 13)                    # three crystals, apart so each reads on its own
    gem(c, 8, HZ - 2, 2, 8)
    gem(c, 24, HZ - 2, 2, 10)
    for x, y in ((7, 3), (25, 13), (13, 4)):                                 # sparkles
        c.set(x, y, 0)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 3)
    return c


def shadowed_thicket6():
    c = C(1)
    c.disc(26, 4, 2.5, 0)                          # crescent moon
    c.disc(27.3, 3.2, 2.2, 1)
    for x in (2, 8, 15, 21, 29):                   # far layer: many thin crooked trees, dark grey
        c.rect(x, 8, x, HZ - 1, 2)
        c.line(x, 11, x + (2 if x % 2 else -2), 8, 2)
        c.line(x, 13, x + (-2 if x % 2 else 2), 11, 2)
    c.rect(0, HZ, W - 1, HZ, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 2)
    for x in range(1, W, 3):                       # mist
        c.rect(x, HZ - 1, x + 1, HZ - 1, 0)
    gnarled_tree(c, 4, HZ, 14, 1)                  # near layer: black, big
    gnarled_tree(c, 13, HZ, 10, -1)
    gnarled_tree(c, 20, HZ, 12, 1)
    gnarled_tree(c, 28, HZ, 15, -1)
    c.set(17, 15, 0)                               # eyes between the trees
    c.set(18, 15, 0)
    return c


def helix_fossil(c, cx, cy):
    c.disc(cx, cy, 3.2, 0)
    for t in range(40):                            # spiral
        a = t * 0.45
        r = 3.0 - t * 0.07
        c.set(cx + r * math.cos(a), cy + r * math.sin(a), 3)
    c.set(cx, cy, 3)


def dome_fossil(c, cx, cy):
    for y in range(4):
        half = [2, 3, 4, 4][y]
        c.rect(cx - half, cy - 3 + y, cx + half, cy - 3 + y, 0)
    c.rect(cx - 5, cy + 1, cx + 5, cy + 1, 3)
    for x in (cx - 2, cx, cx + 2):
        c.rect(x, cy - 2, x, cy, 2)               # ridges
    for t in range(12):
        a = math.pi + t * math.pi / 11
        c.set(cx + 4.5 * math.cos(a), cy + 0.5 + 4.2 * math.sin(a), 3)


def fossil_dig_site6():
    c = C(0)
    c.rect(0, 4, W - 1, HZ, 2)                     # layered rock wall
    c.rect(0, 4, W - 1, 4, 3)
    for y in (8, 13):
        c.rect(0, y, W - 1, y, 1)                  # strata lines (light, quiet)
    helix_fossil(c, 9, 10)                         # Pokemon fossils set in the rock
    dome_fossil(c, 22, 11)
    c.rect(0, HZ, W - 1, HZ, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 1)
    c.line(26, HZ + 4, 30, HZ - 3, 3)              # pickaxe leaning on the wall
    c.line(28, HZ - 4, 32, HZ - 2, 3)
    c.line(28, HZ - 4, 27, HZ - 2, 3)
    for x in (2, 14):                              # rock chips
        c.set(x, HZ + 2, 2)
        c.set(x + 1, HZ + 2, 2)
    return c


def wyrm_hollow6():
    c = C(3)
    c.poly([(1, 24), (2, 7), (7, 2), (25, 2), (30, 7), (31, 24)], 2)   # cave interior
    for x, ln in ((6, 3), (11, 2), (16, 3), (21, 2), (26, 3)):         # stalactites
        for i in range(ln):
            c.set(x, 3 + i, 3)
    c.rect(2, 14, 30, 23, 1)                       # still water in the cave
    for y, xs in ((16, (4, 22)), (19, (8, 25)), (22, (3, 18))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    c.rect(11, 13, 21, 14, 2)                      # island
    c.rect(11, 14, 21, 14, 3)
    # small shrine: dark pagoda roof, white walls, dark door
    for y, x0, x1 in ((6, 15, 17), (7, 13, 19), (8, 12, 20)):
        c.rect(x0, y, x1, y, 3)
    c.set(11, 7, 3)
    c.set(21, 7, 3)
    c.rect(13, 9, 19, 12, 0)
    c.rect(13, 9, 13, 12, 3)
    c.rect(19, 9, 19, 12, 3)
    c.rect(15, 10, 17, 12, 3)
    c.set(16, 5, 3)
    return c


def traders_crossing6():
    c = C(0)
    far_hills(c, 13, 1.0, 5.0)
    base_ground(c, tufts=False)
    # trade machine: a screen on top, a console, two wings each with a Poke Ball pod
    c.rect(10, 1, 21, 9, 3)                        # screen frame
    c.rect(11, 2, 20, 8, 2)                        # screen
    c.rect(12, 3, 14, 3, 1)                        # glare
    c.rect(14, 10, 17, 10, 2)                      # neck
    c.rect(9, 11, 22, HZ - 1, 0)                   # console body
    c.rect(9, 11, 22, 11, 3)
    c.rect(9, 11, 9, HZ - 1, 3)
    c.rect(22, 11, 22, HZ - 1, 3)
    c.rect(12, 13, 19, 14, 2)                      # control panel
    c.set(13, 13, 0)
    c.set(18, 13, 0)
    c.rect(13, 16, 18, 16, 3)
    for side in (-1, 1):                           # wings with Poke Ball pods
        x0 = 1 if side < 0 else 22
        c.rect(x0, 12, x0 + 8, HZ - 1, 1)
        c.rect(x0, 12, x0 + 8, 12, 3)
        c.rect(x0 if side < 0 else x0 + 8, 12, x0 if side < 0 else x0 + 8, HZ - 1, 3)
        poke_ball(c, x0 + 4, 15, 2.6)
        # arm from the screen to the wing
        ax = 9 if side < 0 else 22
        c.line(ax, 5, ax + side * 4, 5, 2)
        c.line(ax + side * 4, 5, ax + side * 5, 11, 2)
    return c


def cafe_boulevard7():
    """User: "hard to tell these are distinct shops. Also, the eiffel tower in the far distance would look good"
    -> three separate shops per side (own facade shade, divider walls, own awning pattern, door) and a lattice
    tower at the far end of the street."""
    c = C(0)
    # lattice tower at the vanishing point (light grey: far away)
    for y in range(1, 14):
        half = 0 if y < 5 else 1 if y < 9 else 2 if y < 12 else 3
        c.set(16 - half, y, 2)
        c.set(16 + half, y, 2)
        if y % 2 == 0 and half:
            c.rect(16 - half + 1, y, 16 + half - 1, y, 1)   # cross-bracing
    c.rect(14, 8, 18, 8, 2)                                  # platforms
    c.rect(13, 11, 19, 11, 2)
    c.set(16, 0, 2)
    shades = (0, 1, 0)
    for side in (0, 1):
        for k in range(3):                                   # three shops along each side
            x0, x1 = k * 4, k * 4 + 3
            for x in range(x0, x1 + 1):
                t = x / 12.0
                top, bot = int(round(6 * t)), int(round(21 - 6 * t))
                xx = x if side == 0 else 31 - x
                c.rect(xx, top, xx, bot, shades[k])
                c.set(xx, top, 3)                            # roof line
                aw = top + int((bot - top) * 0.45)           # awning: own pattern per shop
                pat = (k + side) % 3
                for r in range(2):
                    v = (3 if (x + r) % 2 else 2) if pat == 0 else 3 if pat == 1 else (2 if r == 0 else (3 if x % 2 else 1))
                    c.set(xx, aw + r, v)
            # divider wall between shops, door at the bottom of each
            xd = x1 + 1 if k < 2 else None
            if xd is not None:
                t = xd / 12.0
                top, bot = int(round(6 * t)), int(round(21 - 6 * t))
                c.rect(xd if side == 0 else 31 - xd, top, xd if side == 0 else 31 - xd, bot, 3)
            xm = (x0 + x1) // 2                      # a small clean door per shop (1 px wide, smaller with distance)
            t = xm / 12.0
            bot = int(round(21 - 6 * t))
            hh = max(2, 4 - k)
            dx = xm if side == 0 else 31 - xm
            c.rect(dx, bot - hh, dx, bot - 1, 3)
    c.line(0, 21, 12, 15, 3)
    c.line(31, 21, 19, 15, 3)
    c.poly([(12, 15), (20, 15), (32, 24), (0, 24)], 1)       # the street
    c.line(0, 22, 12, 15, 3)
    c.line(31, 22, 20, 15, 3)
    for y in (17, 20, 23):
        c.set(16, y, 0)
    for (ux, uy) in ((5, 19), (26, 19)):                     # café parasols
        c.rect(ux - 2, uy, ux + 2, uy, 3)
        c.rect(ux - 1, uy - 1, ux + 1, uy - 1, 2)
        c.rect(ux, uy + 1, ux, uy + 3, 3)
    return c


def shard(c, bx, by, ux, uy, length, width):
    """A sharp crystal shard growing from (bx, by) in direction (ux, uy): white body tapering to a point, light-grey
    shaded half, black outline."""
    n = math.hypot(ux, uy)
    ux, uy = ux / n, uy / n
    px, py = -uy, ux                                   # perpendicular
    body = {}
    for i in range(int(length * 3) + 1):
        t = i / 3.0
        half = width * (1 - t / length)
        cx, cy = bx + ux * t, by + uy * t
        for j in range(-int(half * 3), int(half * 3) + 1):
            s = j / 3.0
            xx, yy = int(round(cx + px * s)), int(round(cy + py * s))
            if 0 <= xx < W and 0 <= yy < H:
                body[(xx, yy)] = 0 if s <= 0 else 1    # lit half white, shaded half light grey
    for (xx, yy), v in body.items():
        c.p[yy][xx] = v
    for (xx, yy) in body:                              # black outline all round, on rock or sky alike
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = xx + dx, yy + dy
            if (nx, ny) not in body and 0 <= nx < W and 0 <= ny < H:
                c.p[ny][nx] = 3


def crystal_cliffs7():
    """User: "Crystal cliffs still looks weird. Maybe just draw a cliffside with sharp crystals coming out of the
    side" -> a tall dark cliff on the left, shards jutting up and out of its face."""
    c = C(0)
    c.poly([(16, HZ), (22, 9), (27, 12), (32, 8), (32, HZ)], 1)      # far cliffs on the right, light
    base_ground(c, tufts=False)
    c.poly([(0, HZ), (0, 0), (9, 0), (11, 4), (10, 8), (13, 12), (12, 15), (15, HZ)], 2)   # the near cliff face
    for (a, b) in (((2, 3), (6, 6)), ((1, 10), (8, 12)), ((5, 15), (11, 16))):
        c.line(a[0], a[1], b[0], b[1], 3)                             # cracks / ledges
    c.line(9, 0, 11, 4, 3)
    c.line(11, 4, 10, 8, 3)
    c.line(10, 8, 13, 12, 3)
    c.line(13, 12, 12, 15, 3)
    c.line(12, 15, 15, HZ, 3)                                         # the face's edge
    shard(c, 10, 6, 3, -2, 7, 1.6)                                    # crystals jutting out of the face
    shard(c, 12, 13, 4, -1, 8, 1.8)
    shard(c, 13, 16, 3, -3, 5, 1.2)
    shard(c, 6, 2, 2, -3, 4, 1.0)
    shard(c, 17, HZ - 1, 1, -3, 4, 1.0)                               # one on the ground
    for x, y in ((24, 4), (20, 2)):                                   # sparkles
        c.set(x, y, 3)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 2)
    return c


def elite_bazaar7():
    """User: "the final route picture needs a redesign too" -> a grand market arch crowned at the top, lanterns,
    and a counter showing items (Poke Ball, potion, gem): the end-game item course."""
    c = C(0)
    base_ground(c, tufts=False)
    # arch: two pillars and a rounded top, dark with a light trim
    c.rect(3, 6, 5, HZ - 1, 2)
    c.rect(26, 6, 28, HZ - 1, 2)
    c.rect(5, 6, 5, HZ - 1, 3)
    c.rect(28, 6, 28, HZ - 1, 3)
    for x in range(3, 29):
        t = (x - 15.5) / 12.5
        top = int(round(3 + 4 * t * t))
        c.rect(x, top, x, top + 2, 2)
        c.set(x, top, 3)
        c.set(x, top + 2, 1)
    # crown on top
    c.rect(13, 1, 18, 2, 3)
    for x in (13, 15, 16, 18):
        c.set(x, 0, 3)
    c.set(14, 1, 0)
    c.set(17, 1, 0)
    # counter with items on it (spaced out so each reads on its own)
    c.rect(7, 14, 24, HZ - 1, 0)
    c.rect(7, 14, 24, 14, 3)
    c.rect(7, 14, 7, HZ - 1, 3)
    c.rect(24, 14, 24, HZ - 1, 3)
    c.rect(8, 16, 23, 16, 1)
    poke_ball(c, 10, 11, 2.2)                     # Poke Ball
    c.rect(15, 9, 17, 13, 1)                      # potion bottle
    c.rect(14, 9, 14, 13, 3)
    c.rect(18, 9, 18, 13, 3)
    c.rect(14, 13, 18, 13, 3)
    c.rect(14, 9, 18, 9, 3)
    c.rect(16, 7, 16, 8, 3)                       # its neck / cap
    c.set(15, 10, 0)
    gem(c, 22, 14, 1, 6)                          # a gem (held item)
    return c


def cliff_wall(c, bands):
    """A real cliff: rock bands stacked as ledges. bands = [(y0, y1, right_x)] from top to bottom. Each band: dark
    rock with block cracks, a light ledge line on top where it sticks out, a black shadow line under the band
    above, a sheer black right edge."""
    for k, (y0, y1, rx) in enumerate(bands):
        c.rect(0, y0, rx, y1, 2)
        for y in range(y0, y1 + 1):                  # block cracks, staggered per band (rock blocks)
            for x in range((k * 3) % 5, rx, 5):
                if (y - y0) % 4 != 3:
                    c.set(x, y, 3) if (y - y0) in (1, 2) else None
        c.rect(0, y1, rx, y1, 3) if k < len(bands) - 1 else None   # shadow under the band
        c.rect(rx, y0, rx, y1, 3)                    # sheer face edge
        prev_rx = bands[k - 1][2] if k else -1
        if rx > prev_rx:                              # the ledge top sticking out is lit
            c.rect(max(0, prev_rx), y0, rx - 1, y0, 1)
    # jagged top of the cliff
    for x in range(0, bands[0][2] + 1):
        if x % 3 == 1:
            c.set(x, bands[0][0], 0)


def crystal_cliffs8():
    """User: "the cliffsides of the crystal cliffs dont really look like cliffsides" -> stepped rock wall with
    ledges, rock blocks and a sheer face; crystal shards jut out of that face."""
    c = C(0)
    c.poly([(16, HZ), (21, 10), (25, 13), (29, 9), (32, 11), (32, HZ)], 1)   # far cliffs, light
    base_ground(c, tufts=False)
    cliff_wall(c, [(0, 4, 11), (5, 9, 13), (10, 13, 12), (14, HZ - 1, 15)])
    shard(c, 12, 3, 3, -2, 6, 1.4)                # shards jutting out of the face, one per band
    shard(c, 14, 8, 4, -1, 8, 1.8)
    shard(c, 13, 12, 3, -2, 5, 1.2)
    shard(c, 16, 16, 4, -3, 6, 1.4)
    for x, y in ((24, 4), (20, 2), (28, 6)):     # sparkles
        c.set(x, y, 3)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 2)
    return c


def crystal_cliffs9():
    """crystal_cliffs8's even block cracks read as a brick wall -> a rock face: uneven edge that steps out in
    ledges (lit top, black underside), irregular vertical cracks with a lit side, small shards from the face."""
    c = C(0)
    c.poly([(17, HZ), (21, 11), (24, 13), (28, 8), (32, 10), (32, HZ)], 1)   # far cliffs, light
    base_ground(c, tufts=False)
    edge = [8, 9, 9, 10, 13, 13, 12, 12, 12, 13, 15, 15, 14, 14, 15, 17, 17, 18]   # right edge per row 0..17
    for y, rx in enumerate(edge):
        c.rect(0, y, rx, y, 2)
        c.set(rx, y, 3)                               # shaded face edge
    for y in range(1, len(edge)):                     # ledges where the face steps out
        if edge[y] > edge[y - 1]:
            c.rect(edge[y - 1] + 1, y, edge[y], y, 1)     # lit ledge top
            if y in (4, 10, 15):                          # black underside of the ledge above, broken
                for x in range(1, edge[y - 1] + 1):
                    if x % 4 != 0:
                        c.set(x, y - 1, 3)
    for x, y0, y1 in ((3, 0, 3), (6, 1, 3), (2, 5, 9), (8, 5, 8), (5, 7, 9), (4, 11, 14), (10, 11, 14),
                      (7, 12, 14), (2, 16, 17), (12, 16, 17), (8, 16, 17)):   # vertical cracks
        for y in range(y0, y1 + 1):
            c.set(x, y, 3)
            if (y + x) % 2 == 0:
                c.set(x - 1, y, 1)                    # lit side of the crack
    shard(c, 13, 6, 3, -2, 5, 1.2)                # shards jutting from the face
    shard(c, 15, 12, 4, -1, 6, 1.4)
    shard(c, 18, 16, 3, -2, 4, 1.0)
    shard(c, 9, 2, 3, -2, 4, 1.0)
    for x, y in ((25, 3), (21, 5), (29, 15)):     # sparkles
        c.set(x, y, 3)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 2)
    return c


def crystal_cliffs10():
    """crystal_cliffs9's speckled cracks were noise -> a plain rock face: lit cliff top, long continuous fissures
    (black with a lit left side), one ledge stepping out, a shaded edge; three shards spaced out along the face."""
    c = C(0)
    c.poly([(18, HZ), (22, 11), (25, 13), (28, 8), (32, 10), (32, HZ)], 1)   # far cliffs, light
    base_ground(c, tufts=False)
    top = 2
    edge = {y: (10 if y < 6 else 11 if y < 10 else 14 if y < 14 else 16) for y in range(top, HZ)}
    for y, rx in edge.items():
        c.rect(0, y, rx, y, 2)
        c.set(rx, y, 3)                                   # shaded face edge
    c.rect(0, top - 1, edge[top], top - 1, 3)             # cliff top outline
    c.rect(0, top, edge[top] - 1, top, 1)                 # lit top lip
    c.rect(11, 10, 13, 10, 1)                             # ledge top where the face steps out
    c.rect(0, 9, 10, 9, 3)                                # shadow under the upper block
    c.rect(14, 14, 15, 14, 1)
    for x, y0, y1 in ((4, 3, 8), (8, 4, 8), (3, 11, 17), (7, 10, 15), (11, 12, 17)):   # long fissures
        c.rect(x, y0, x, y1, 3)
        c.rect(x - 1, y0, x - 1, y1, 1)
    shard(c, 11, 5, 3, -2, 5, 1.2)                        # shards jutting from the face
    shard(c, 14, 12, 4, -1, 6, 1.4)
    shard(c, 16, 16, 4, -3, 4, 1.0)
    for x, y in ((25, 3), (20, 4), (28, 15)):             # sparkles
        c.set(x, y, 3)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 2)
    return c


def prism(c, bx, by, ux, uy, length, width):
    """A crystal prism from (bx, by) toward (ux, uy): even width, then a pointed tip over the last third; lit half
    white, shaded half light grey, black outline all round."""
    n = math.hypot(ux, uy)
    ux, uy = ux / n, uy / n
    px, py = -uy, ux
    body = {}
    for i in range(int(length * 3) + 1):
        t = i / 3.0
        half = width if t < length * 0.62 else width * (length - t) / (length * 0.38)
        cx, cy = bx + ux * t, by + uy * t
        for j in range(-int(half * 3), int(half * 3) + 1):
            s = j / 3.0
            xx, yy = int(round(cx + px * s)), int(round(cy + py * s))
            if 0 <= xx < W and 0 <= yy < H:
                body[(xx, yy)] = 0 if -0.4 <= s <= 0.05 and half >= 0.6 else 1   # light body, white highlight
    for (xx, yy), v in body.items():
        c.p[yy][xx] = v
    for (xx, yy) in body:
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = xx + dx, yy + dy
            if (nx, ny) not in body and 0 <= nx < W and 0 <= ny < H:
                c.p[ny][nx] = 3


def crystal_cliffs11():
    """crystal_cliffs10 still read as a shelf -> a craggy cliff in profile: uneven edge slanting down-right, lit
    where it faces up, diagonal rock layers; crystals in two fanned clusters growing out of the edge."""
    c = C(0)
    c.poly([(18, HZ), (22, 11), (25, 13), (28, 8), (32, 10), (32, HZ)], 1)   # far cliffs, light
    base_ground(c, tufts=False)
    edge = [6, 7, 7, 9, 10, 9, 8, 9, 11, 12, 12, 11, 12, 14, 15, 15, 16, 18]   # craggy right edge, rows 0..17
    for y, rx in enumerate(edge):
        c.rect(0, y, rx, y, 2)
    for k in range(-24, 18, 5):                           # diagonal rock layers (down-left)
        for t in range(0, 30):
            x, y = k + 18 - t // 2, t
            if 0 <= y < HZ and 0 <= x < edge[y] - 1:
                c.set(x, y, 3 if t % 7 else 1)
    for y, rx in enumerate(edge):
        c.set(rx, y, 3)                                   # edge outline
        if y and rx > edge[y - 1]:
            c.rect(edge[y - 1] + 1, y, rx, y, 3)          # step outline
            c.rect(edge[y - 1] + 1, y + 1, rx - 1, y + 1, 1) if y + 1 < HZ else None   # lit upward face
    for base, shards in (((10, 4), ((1, -1, 5, 0.9), (3, -1, 7, 1.2))),
                         ((15, 14), ((1, -2, 6, 1.0), (3, -1, 8, 1.4)))):
        for ux, uy, ln, wd in shards:                     # back prisms first, front outlines split them
            prism(c, base[0], base[1], ux, uy, ln, wd)
    for x, y in ((26, 3), (22, 7), (28, 15)):             # sparkles, clear of the crystal tips
        c.set(x, y, 3)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 2)
    return c


COURSES.update({"elite_bazaar": elite_bazaar7})
COURSES.update({"dojo_grounds": dojo_grounds6, "cafe_boulevard": cafe_boulevard7, "crystal_cliffs": crystal_cliffs11,
                "shadowed_thicket": shadowed_thicket6,
                "fossil_dig_site": fossil_dig_site6, "wyrm_hollow": wyrm_hollow6,
                "traders_crossing": traders_crossing6})

COURSES.update({"quiet_outskirts": quiet_outskirts3, "seashell_coast": seashell_coast3,
                "frost_ridge": frost_ridge3})  # crystal / fossil / palace: replaced again in round 5
KINDS = COURSES


# --- GSC courses, set 1 (2026-11-15): eight new pictures for the Gold/Silver/Crystal courses. Same style: white
# sky, solid dark silhouettes with light-grey highlights, one horizon line, light far layer, ONE big subject.

TUFT_BIG = ["....#....",
            "#...#...#",
            "#+..#..+#",
            ".#+.#.+#.",
            ".#+-#-+#.",
            "..#+#+#..",
            "..#+++#..",
            "...###..."]
TUFT_SMALL = ["#.#.#",
              ".###."]


def stamp(c, art, x0, y0):
    """Copy a small hand-drawn sprite ('.' = leave, '-' '+' '#' = shades 1-3) onto the canvas."""
    for dy, row in enumerate(art):
        for dx, ch in enumerate(row):
            if ch != ".":
                c.set(x0 + dx, y0 + dy, CH.index(ch))


def gsc_bird(c, x, y):
    """A small flying bird seen from the side, wings up: a black 'v' with a body pixel."""
    c.set(x - 2, y - 1, 3)
    c.set(x - 1, y, 3)
    c.set(x, y, 3)
    c.set(x + 1, y, 3)
    c.set(x + 2, y - 1, 3)
    c.set(x + 2, y + 0, 2)


def refreshing_field():
    """Open grassy field: rolling light hills, a dirt path running to the horizon, big tall-grass clumps either
    side, a short wooden fence, birds in the sky."""
    c = C(0)
    far_hills(c, 13, 1.6, 3.8, 0.8)                                # low rolling hills
    base_ground(c, tufts=False)
    c.poly([(14, HZ + 1), (18, HZ + 1), (23, H), (9, H)], 0)        # path into the distance
    c.line(14, HZ + 1, 9, H - 1, 2)
    c.line(18, HZ + 1, 23, H - 1, 2)
    fence(c, 20, 30, HZ)
    for x, y in ((3, HZ - 2), (10, HZ - 1), (8, HZ + 3)):
        stamp(c, TUFT_SMALL, x, y)                                 # small far tufts
    stamp(c, TUFT_BIG, 0, H - 8)                                   # big near tall-grass tufts
    stamp(c, TUFT_BIG, 23, H - 8)
    gsc_bird(c, 8, 5)
    gsc_bird(c, 14, 3)
    cloud(c, 22, 4, 7)
    return c


def boulder(c, x, ground, r):
    """Round boulder: dark body, black lower-right rim and outline, a light highlight top-left."""
    cy = ground - r + 0.5
    m = {(xx, y) for y in range(H) for xx in range(W)
         if y < ground and ((xx - x) / (r + 0.6)) ** 2 + ((y - cy) / (r + 0.1)) ** 2 <= 1}
    for (xx, y) in m:
        edge = any((xx + dx, y + dy) not in m for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
        shade = (xx - x) + (y - cy) > r * 0.9
        c.set(xx, y, 3 if edge or shade else 2)
    c.set(x - r // 2, int(cy) - r // 2, 1)
    c.set(x - r // 2 + 1, int(cy) - r // 2, 1)
    c.set(x - r // 2, int(cy) - r // 2 + 1, 1)


def rugged_road():
    """Rocky mountain road: a dark rock wall with ledges on the left, a pale road running to the horizon, a big
    round boulder beside it, far light peaks."""
    c = C(0)
    c.poly([(10, HZ), (17, 5), (21, 9), (26, 3), (32, 8), (32, HZ)], 1)    # far peaks
    base_ground(c, tufts=False)
    c.poly([(0, HZ), (0, 3), (5, 2), (8, 6), (10, 9), (12, 14), (14, HZ)], 2)   # rock wall
    c.line(5, 2, 8, 6, 3)
    c.line(8, 6, 10, 9, 3)
    c.line(10, 9, 12, 14, 3)
    c.line(12, 14, 14, HZ, 3)
    c.rect(0, 2, 4, 2, 3)
    for (a, b) in (((1, 7), (8, 8)), ((2, 13), (11, 14))):
        c.line(a[0], a[1], b[0], b[1], 3)                          # ledges
        c.line(a[0], a[1] - 1, b[0] - 1, b[1] - 1, 1)
    c.poly([(14, HZ + 1), (17, HZ + 1), (20, H), (9, H)], 0)        # the road
    c.line(14, HZ + 1, 9, H - 1, 2)
    c.line(17, HZ + 1, 20, H - 1, 2)
    boulder(c, 25, HZ + 2, 5)                     # a big boulder beside the road
    for x, y in ((13, 21), (16, 22), (5, 21), (6, 21)):
        c.set(x, y, 2)
    return c


def ranch():
    """Farm ranch: a big barn (dark gambrel roof, white walls, X-braced doors), a silo, open pasture, a wooden
    rail fence across the front."""
    c = C(0)
    far_hills(c, 13, 1.0, 5.0, 1.0)
    base_ground(c, tufts=False)
    # silo behind the barn's right side
    c.rect(22, 5, 26, HZ - 1, 0)
    c.rect(22, 5, 22, HZ - 1, 3)
    c.rect(26, 5, 26, HZ - 1, 3)
    c.rect(25, 6, 25, HZ - 1, 1)
    for y in (8, 11, 14):
        c.rect(23, y, 25, y, 2)
    c.rect(23, 3, 25, 4, 2)
    c.rect(22, 4, 26, 4, 3)
    c.set(24, 2, 3)
    c.set(23, 3, 3)
    c.set(25, 3, 3)
    # barn: gambrel roof
    x0, x1, wall = 4, 20, 10
    for y, (a, b) in zip(range(2, 9), ((9, 15), (7, 17), (5, 19), (4, 20), (3, 21), (3, 21), (2, 22))):
        c.rect(a, y, b, y, 2)
        c.set(a, y, 3)
        c.set(b, y, 3)
    c.rect(9, 2, 15, 2, 3)
    c.rect(10, 3, 12, 3, 1)                       # light on the roof
    c.rect(2, 8, 22, 8, 3)
    c.rect(x0, 9, x1, HZ - 1, 0)                  # walls
    c.rect(x0, 9, x0, HZ - 1, 3)
    c.rect(x1, 9, x1, HZ - 1, 3)
    c.rect(11, 4, 13, 6, 3)                       # hay loft door
    c.set(12, 5, 2)
    c.rect(8, 11, 16, HZ - 1, 2)                  # big doors with X braces
    c.rect(8, 11, 16, 11, 3)
    c.rect(8, 11, 8, HZ - 1, 3)
    c.rect(16, 11, 16, HZ - 1, 3)
    c.rect(12, 11, 12, HZ - 1, 3)
    c.line(9, 12, 11, HZ - 1, 0)
    c.line(11, 12, 9, HZ - 1, 0)
    c.line(13, 12, 15, HZ - 1, 0)
    c.line(15, 12, 13, HZ - 1, 0)
    # rail fence across the front
    for y in (HZ + 1, HZ + 3):
        c.rect(0, y, W - 1, y, 2)
    for x in range(1, W, 5):
        c.rect(x, HZ, x, H - 1, 3)
        c.set(x, HZ - 1, 3)
    return c


def flower(c, x, y, v=0):
    """Flower: four petals round a black centre."""
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        c.set(x + dx, y + dy, v)
    c.set(x, y, 3)


def nursery_meadow():
    """Soft flower meadow: a small cosy house with a round window and a chimney, a round tree, the ground band full
    of white flowers."""
    c = C(0)
    far_hills(c, 13, 1.0, 4.0, 2.0)
    base_ground(c, tufts=False)
    house3(c, 9, HZ, 13, 6)
    c.rect(18, 3, 19, 6, 3)                       # chimney
    c.set(19, 1, 1)
    c.set(18, 0, 1)
    tree2(c, 4, HZ, 3, 4)
    tree2(c, 28, HZ, 3, 3)
    for x, y in ((2, 20), (7, 22), (12, 20), (17, 22), (22, 20), (27, 22), (30, 19), (25, 18)):
        flower(c, x, y)
    for x in (5, 15, 20, 31):
        c.set(x, 21, 2)
    return c


def utility_pole(c, x, top, ground):
    c.rect(x, top, x, ground - 1, 3)
    c.rect(x - 2, top + 1, x + 2, top + 1, 3)
    c.set(x - 2, top, 2)
    c.set(x + 2, top, 2)


def suburban_area():
    """Suburban street: two houses side by side, utility poles with sagging power lines across the sky, the road
    with dashes in front and a car on it."""
    c = C(0)
    for x0, x1, top in ((12, 15, 11), (27, 31, 12)):              # far roofs
        c.rect(x0, top, x1, HZ - 1, 1)
    base_ground(c, tufts=False)
    for x in range(1, W, 6):
        c.rect(x, HZ + 3, x + 2, HZ + 3, 0)                         # road dashes
    utility_pole(c, 2, 1, HZ)
    utility_pole(c, 29, 1, HZ)
    for y0 in (2, 4):                                             # two wires, sagging between the poles
        for x in range(W):
            t = min(1.0, max(0.0, (x - 2) / 27.0))
            c.set(x, y0 + 2.2 * 4 * t * (1 - t), 3)
    house3(c, 4, HZ, 9, 5)
    house3(c, 19, HZ, 9, 5)
    tree2(c, 16, HZ, 2, 3)
    return c


def ruin_column(c, x, ground, h, cut=0, capital=False):
    """3-px stone column: white lit side, light-grey middle, dark shade side, black outline; either a capital
    block on top or a broken top slanting by `cut` rows."""
    top = ground - h
    for i in range(3):
        for y in range(top, ground):
            c.set(x + i, y, (0, 1, 2)[i])
        c.rect(x - 1, top, x - 1, ground - 1, 3)
        c.rect(x + 3, top, x + 3, ground - 1, 3)
    if capital:
        c.rect(x - 2, top - 2, x + 4, top - 1, 1)
        c.rect(x - 2, top - 2, x + 4, top - 2, 0)
        c.rect(x - 3, top - 2, x - 3, top - 1, 3)
        c.rect(x + 5, top - 2, x + 5, top - 1, 3)
        c.rect(x - 2, top - 3, x + 4, top - 3, 3)
        c.rect(x - 2, top, x + 4, top, 3)
        return
    for k in range(5):                            # broken top: a jagged slant
        yy = top + (k * cut) // 4 if cut >= 0 else top + ((4 - k) * -cut) // 4
        yy += (1 if k == 2 else 0)
        for y in range(top, yy):
            c.set(x - 1 + k, y, 0)
        c.set(x - 1 + k, yy, 3)


def whispering_ruins():
    """Ancient ruins: a stone platform with a row of columns - two still carry a lintel stone, the rest stand
    broken at different heights - a carved eye stone in the platform, fallen column drums."""
    c = C(0)
    far_hills(c, 13, 1.0, 5.0, 0.3)
    c.rect(0, HZ, W - 1, HZ, 3)
    c.rect(0, HZ + 1, W - 1, H - 1, 1)
    c.rect(1, 15, 30, 17, 2)                                          # platform of carved blocks
    c.rect(1, 14, 30, 14, 3)
    c.rect(1, 15, 30, 15, 1)
    c.rect(0, 14, 0, HZ, 3)
    c.rect(31, 14, 31, HZ, 3)
    for x in range(5, 30, 5):
        c.rect(x, 16, x, 17, 3)
    c.rect(14, 16, 16, 17, 1)                                         # the carved eye block
    c.set(15, 16, 3)
    c.set(15, 17, 3)
    ruin_column(c, 4, 14, 8, capital=True)
    ruin_column(c, 11, 14, 8, capital=True)
    c.rect(1, 1, 17, 2, 1)                                            # lintel across the two
    c.rect(1, 1, 17, 1, 0)
    c.rect(0, 0, 18, 0, 3)
    c.rect(0, 0, 0, 3, 3)
    c.rect(18, 0, 18, 3, 3)
    c.rect(0, 3, 18, 3, 3)
    for x, y, v in ((17, 0, 0), (18, 0, 0), (18, 1, 0), (17, 1, 3), (18, 2, 3)):   # a chipped corner
        c.set(x, y, v)
    ruin_column(c, 19, 14, 6, cut=3)                                  # broken columns
    ruin_column(c, 26, 14, 9, cut=-3)
    for x0, y0 in ((4, 20), (22, 21)):                                # fallen column drums on the ground
        c.rect(x0, y0, x0 + 4, y0 + 1, 1)
        c.rect(x0, y0, x0 + 4, y0, 0)
        c.rect(x0 - 1, y0 - 1, x0 + 5, y0 - 1, 3)
        c.rect(x0 - 1, y0 + 2, x0 + 5, y0 + 2, 3)
        c.rect(x0 - 1, y0, x0 - 1, y0 + 1, 3)
        c.rect(x0 + 5, y0, x0 + 5, y0 + 1, 3)
        c.rect(x0 + 4, y0, x0 + 4, y0 + 1, 2)
    return c


def spike(c, x, y0, ln, w, down=True, v=3):
    """Pointed rock: w half-width at the root, tapering to a 1-px tip after ln rows (down from or up to y0)."""
    for i in range(ln):
        half = int(round(w * (1 - i / float(ln))))
        y = y0 + i if down else y0 - i
        c.rect(x - half, y, x + half, y, v)


def dim_cave():
    """Dark cave interior: black rock framing a dim chamber, big black stalactites hanging from the ceiling and
    stalagmites rising from the floor, a lighter passage behind (user: "Remove the bat from the cave")."""
    c = C(3)
    c.poly([(1, 24), (2, 8), (7, 2), (25, 2), (30, 8), (31, 24)], 2)      # chamber, dim
    c.poly([(9, 20), (10, 12), (13, 8), (19, 8), (22, 12), (23, 20)], 1)  # deeper passage, lighter
    c.rect(0, 20, W - 1, 20, 3)                                           # floor edge
    c.rect(0, 21, W - 1, H - 1, 2)
    for x, ln, w in ((5, 8, 2), (10, 5, 1), (21, 6, 1), (27, 9, 2), (15, 3, 1)):
        spike(c, x, 2, ln, w)                                             # stalactites
    for x, ln, w in ((4, 7, 2), (26, 6, 2), (11, 3, 1), (20, 4, 1)):
        spike(c, x, 20, ln, w, down=False)                                # stalagmites
    for x, y in ((7, 22), (17, 22), (29, 22)):                            # rocks on the floor
        c.rect(x, y, x + 2, y + 1, 3)
        c.set(x, y, 1)
    return c


def harbor_docks():
    """Harbour: the sea to a far horizon, a big ship moored (dark hull, white cabin with portholes, funnel), a
    wooden pier seen from the side with piles and bollards, a gull."""
    c = C(0)
    c.rect(0, 8, W - 1, 8, 2)                                     # sea horizon
    c.rect(0, 9, W - 1, H - 1, 1)
    for y, xs in ((10, (4, 12)), (18, (17, 26)), (21, (20, 28)), (23, (15,))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    # ship
    c.poly([(14, 11), (32, 11), (32, 16), (17, 16)], 2)            # hull, bow on the left
    c.rect(14, 11, 31, 11, 3)
    c.line(14, 11, 17, 16, 3)
    c.rect(17, 16, 31, 16, 3)
    c.rect(18, 13, 31, 13, 1)                     # stripe
    c.rect(19, 6, 29, 10, 0)                      # cabin
    c.rect(19, 6, 29, 6, 3)
    c.rect(19, 6, 19, 10, 3)
    c.rect(29, 6, 29, 10, 3)
    for x in range(21, 28, 2):
        c.set(x, 8, 2)
    c.rect(23, 2, 25, 5, 3)                       # funnel
    c.rect(23, 3, 25, 3, 0)
    c.set(24, 1, 1)
    c.set(25, 0, 1)
    # pier, side view: deck, piles into the water, bollards, a mooring rope to the bow
    c.rect(0, 13, 12, 13, 0)
    c.rect(0, 12, 12, 12, 3)
    c.rect(0, 14, 12, 15, 2)
    c.rect(0, 15, 12, 15, 3)
    c.set(12, 13, 3)
    c.set(12, 14, 3)
    for x in (1, 6, 11):
        c.rect(x, 16, x, H - 1, 3)
    for x in (3, 9):
        c.rect(x, 10, x + 1, 11, 3)
        c.rect(x - 1, 10, x + 2, 10, 3)
        c.set(x, 10, 1)
    c.line(10, 11, 15, 12, 3)
    for x in (2, 7):
        c.rect(x, 18, x + 2, 18, 0)              # ripples round the piles
    gsc_bird(c, 7, 4)
    gsc_bird(c, 13, 2)
    return c


GSC1 = {"refreshing_field": refreshing_field, "rugged_road": rugged_road, "ranch": ranch,
        "nursery_meadow": nursery_meadow, "suburban_area": suburban_area, "whispering_ruins": whispering_ruins,
        "dim_cave": dim_cave, "harbor_docks": harbor_docks}
COURSES.update(GSC1)


# --- Sun / Moon courses, set 1 (2026-11-15, user: "Yes, draw them"): 14 pictures for the Gen 7 courses that had
# stand-ins. Same house style: light sky, one horizon, solid dark shapes with light-grey highlights, ONE big subject.
FONT3.update({"M": ["#.#", "###", "###", "#.#", "#.#"], "L": ["#..", "#..", "#..", "#..", "###"]})


def palm(c, x, ground, h, lean=1):
    """Palm tree: a thin curving black trunk with ring marks, a crown of drooping fronds, two coconuts."""
    pts = []
    for i in range(h):
        y = ground - 1 - i
        bend = int(round(lean * (i / h) ** 2 * 3))
        c.set(x + bend, y, 3)
        if i % 3 == 1:
            c.set(x + bend + 1, y, 2)
        pts.append((x + bend, y))
    tx, ty = pts[-1]
    for dx, dy, ln in ((-5, 2, 5), (5, 2, 5), (-4, -1, 4), (4, -1, 4), (-2, 3, 3), (2, 3, 3)):
        for k in range(1, ln + 1):                      # fronds arc out and droop at the tips
            fx = tx + dx * k / ln
            fy = ty - 1 + dy * (k / ln) ** 2 - (1 if 1 < k < ln - 1 else 0) * (dy < 0)
            c.set(fx, fy, 3)
            if k < ln - 1:
                c.set(fx, fy + 1, 2)
    c.set(tx - 1, ty + 1, 3)
    c.set(tx + 1, ty + 1, 3)


def boardwalk():
    """Boardwalk: a wooden pier running straight out to sea in perspective (light planks, black edges, posts),
    calm sea to the horizon, a low sun, a palm tree on the sandy shore at the left, two gulls."""
    c = C(0)
    c.rect(0, 9, W - 1, 9, 2)                                      # sea horizon
    c.rect(0, 10, W - 1, H - 1, 1)
    for y, xs in ((11, (2, 26)), (13, (24,)), (16, (27,)), (19, (25,)), (14, (3,))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)                              # ripples
    c.disc(25, 5, 2.3, 1)                                         # sun just above the sea
    c.rect(23, 9, 27, 9, 2)
    # the pier: wide at the bottom, narrow at the far end
    c.poly([(9, 24), (22, 24), (17.5, 10), (14.5, 10)], 0)
    c.line(9, 23, 14.5, 10, 3)
    c.line(22, 23, 17, 10, 3)
    c.rect(14, 10, 17, 10, 3)
    for y in (12, 14, 17, 20, 23):                                 # plank joins, further apart nearer
        t = (y - 10) / 14.0
        x0, x1 = 14.5 - 5.5 * t, 17.5 + 4.5 * t
        c.rect(int(x0) + 1, y, int(x1) - 1, y, 2)
    for y, h in ((11, 1), (14, 2), (18, 2), (22, 3)):             # posts along both edges
        t = (y - 10) / 14.0
        for x in (14.5 - 5.5 * t, 17.5 + 4.5 * t):
            c.rect(int(round(x)), y - h, int(round(x)), y, 3)
    # sandy shore with a palm
    c.poly([(0, 15), (5, 17), (8, 24), (0, 24)], 0)
    c.line(0, 15, 5, 17, 2)
    c.line(5, 17, 8, 24, 2)
    palm(c, 3, 21, 13, lean=1)
    gsc_bird(c, 9, 3)
    gsc_bird(c, 15, 5)
    return c


def lamp_post(c, x, top, ground):
    c.rect(x, top + 2, x, ground - 1, 3)
    c.rect(x - 1, top, x + 1, top + 1, 3)
    c.set(x, top + 1, 0)
    c.rect(x - 1, ground - 1, x + 1, ground - 1, 3)


def city_square():
    """City square: a tiered fountain in the middle of a paved plaza with water arcing down, a lamp post either
    side, low town buildings along the back."""
    c = C(0)
    for x0, x1, top in ((0, 7, 6), (8, 13, 8), (18, 24, 7), (25, 31, 5)):   # buildings at the back
        c.rect(x0, top, x1, 13, 1)
        c.rect(x0, top, x1, top, 2)
        for wy in range(top + 2, 12, 3):
            for wx in range(x0 + 1, x1, 3):
                c.set(wx, wy, 0)
                c.set(wx + 1, wy, 0)
    c.rect(0, 14, W - 1, 14, 2)                                    # plaza edge
    for y in range(15, H):                                         # paving joints in perspective
        if y % 3 == 0:
            c.rect(0, y, W - 1, y, 1)
    for x in range(-8, 41, 6):
        c.line(16 + (x - 16) * 0.25, 15, x, 23, 1)
    # fountain: wide basin, pedestal, upper bowl, water arcs, spray
    c.rect(7, 18, 24, 20, 2)
    c.rect(7, 17, 24, 17, 3)
    c.rect(7, 21, 24, 21, 3)
    c.rect(8, 18, 23, 18, 1)                                       # water surface in the basin
    c.rect(15, 11, 16, 16, 3)
    c.rect(11, 10, 20, 10, 3)
    c.rect(12, 11, 19, 11, 2)
    for side in (-1, 1):
        for k in range(6):                                         # arcs of water falling to the basin
            x = 15.5 + side * (5 + k * 0.6)
            y = 10 + k * 1.2 + (k * k) * 0.1
            c.set(x, y, 1)
    for x, y in ((15, 7), (16, 6), (14, 8), (17, 8), (15, 9), (16, 9)):
        c.set(x, y, 1)                                             # spray rising from the top
    lamp_post(c, 3, 9, 23)
    lamp_post(c, 28, 9, 23)
    return c


def shopping_plaza():
    """Shopping plaza: a big mall front with a black sign saying MALL, a glass entrance in the middle, rows of shop
    windows, small potted trees, a paved forecourt."""
    c = C(0)
    c.rect(1, 6, 30, 17, 1)                                        # the building
    c.rect(1, 6, 30, 6, 3)
    c.rect(1, 6, 1, 17, 3)
    c.rect(30, 6, 30, 17, 3)
    c.rect(7, 0, 24, 5, 3)                                         # sign
    text3(c, 8, 0, "MALL", 0)
    c.rect(11, 5, 20, 5, 3)
    for x in range(3, 29, 4):                                      # upper windows
        if 11 <= x <= 19:
            continue
        c.rect(x, 8, x + 2, 10, 0)
        c.rect(x, 10, x + 2, 10, 2)
    c.rect(11, 9, 20, 17, 2)                                       # glass entrance
    c.rect(11, 9, 20, 9, 3)
    for x in (13, 15, 16, 18):
        c.rect(x, 10, x, 17, 0 if x in (13, 18) else 3)
    c.rect(12, 11, 12, 13, 1)                                      # glints on the glass
    c.rect(19, 11, 19, 12, 1)
    for x in range(3, 10, 3):                                      # shop windows, ground floor
        c.rect(x, 13, x + 1, 16, 0)
    for x in range(22, 29, 3):
        c.rect(x, 13, x + 1, 16, 0)
    c.rect(0, 18, W - 1, 18, 2)
    c.rect(0, 19, W - 1, H - 1, 0)
    for y in (20, 23):
        c.rect(0, y, W - 1, y, 1)
    for x in (4, 27):                                              # potted trees on the forecourt
        c.rect(x - 1, 20, x + 1, 22, 3)
        c.rect(x, 17, x, 19, 3)
        c.disc(x, 15, 2, 2)
        c.set(x - 1, 14, 1)
    return c


def seaside_cliff():
    """Seaside cliff: a tall rock cliff on the left with grass on top and layered strata, the sea to a far horizon,
    big white waves breaking at the foot of the cliff, gulls."""
    c = C(0)
    c.rect(0, 9, W - 1, 9, 2)
    c.rect(0, 10, W - 1, H - 1, 1)
    for y, xs in ((12, (24,)), (15, (28,)), (19, (26,)), (22, (21, 29))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    cliff = [(0, 3), (9, 3), (12, 5), (13, 9), (15, 12), (15, 16), (17, 20), (18, 24), (0, 24)]
    c.poly(cliff, 2)
    for i in range(len(cliff) - 2):                                # black outline down the face
        (xa, ya), (xb, yb) = cliff[i], cliff[i + 1]
        c.line(xa, ya, xb, yb, 3)
    for y, x1 in ((8, 12), (12, 14), (16, 15), (20, 17)):         # strata
        c.line(0, y + 1, x1, y, 3)
    c.rect(0, 2, 9, 2, 1)                                          # grass on top
    for x in (2, 5, 8):
        c.set(x, 1, 2)
    c.rect(1, 5, 4, 5, 1)                                          # light catching the face
    c.rect(3, 10, 7, 10, 1)
    # waves breaking against the foot
    for cx, cy, r in ((19, 17, 2.4), (22, 19, 2.0), (17, 21, 2.2), (24, 22, 1.6)):
        c.disc(cx, cy, r, 0)
    for x, y in ((18, 14), (20, 13), (21, 15), (23, 16), (25, 19)):
        c.set(x, y, 0)                                             # spray
    c.line(20, 20, 27, 20, 3)                                      # curl of the wave
    c.line(27, 20, 29, 18, 3)
    gsc_bird(c, 22, 4)
    gsc_bird(c, 28, 6)
    return c


def clear_lake():
    """Clear lake: soft hills far away mirrored in a still lake, a few ripple lines, lily pads with flowers, tall
    reeds with cattail heads framing both sides of the near shore."""
    c = C(0)
    far_hills(c, 6, 1.6, 3.2, 0.5)
    c.rect(0, 10, W - 1, 10, 2)                                    # far shore
    c.rect(0, 11, W - 1, 20, 0)                                    # the lake
    for x in range(W):                                             # the hills' reflection
        top = int(round(6 + 1.6 * math.sin(x / 3.2 + 0.5)))
        for y in range(11, 11 + (10 - top) // 2 + 1):
            if (x + y) % 2 == 0:
                c.set(x, y, 1)
    for y, x0, x1 in ((15, 9, 14), (17, 18, 24), (19, 6, 10)):
        c.rect(x0, y, x1, y, 1)
    for cx, cy in ((13, 18), (21, 15), (25, 19)):                   # lily pads
        c.rect(cx - 2, cy, cx + 2, cy, 2)
        c.rect(cx - 1, cy - 1, cx + 1, cy - 1, 2)
        c.set(cx, cy - 1, 0)
        c.set(cx - 2, cy, 3)
    c.set(21, 13, 0)                                               # a flower on one pad
    c.rect(20, 14, 22, 14, 0)
    c.set(21, 14, 3)
    c.rect(0, 21, W - 1, 21, 2)
    c.rect(0, 22, W - 1, H - 1, 1)
    for x, top in ((1, 8), (3, 11), (5, 13), (27, 12), (29, 9), (31, 11)):   # reeds
        c.rect(x, top, x, 23, 3)
        c.rect(x, top + 1, x, top + 3, 2)
        c.rect(x - 1, top + 1, x - 1, top + 3, 3)
    return c


def volcanic_trail():
    """Volcanic trail: a big volcano filling the middle, a crater rim with smoke billowing out, bright lava running
    down its slopes, a rocky trail winding up to it with boulders."""
    c = C(0)
    for cx, cy, r in ((17, 4, 2.0), (20, 2, 2.4), (24, 1, 2.0), (14, 3, 1.5)):
        c.disc(cx, cy, r, 1)                                       # smoke
    c.poly([(1, 20), (12, 7), (20, 7), (31, 20)], 2)              # the cone
    c.line(1, 20, 12, 7, 3)
    c.line(20, 7, 31, 20, 3)
    c.rect(12, 6, 20, 6, 3)                                        # crater rim
    c.rect(13, 7, 19, 7, 3)
    for path in (((15, 8), (14, 11), (12, 14), (11, 18)), ((17, 8), (18, 12), (21, 15), (22, 19))):
        for (xa, ya), (xb, yb) in zip(path, path[1:]):              # lava streams
            c.line(xa, ya, xb, yb, 0)
    c.line(16, 8, 16, 10, 0)
    for x, y in ((7, 15), (25, 16), (9, 12), (23, 12)):            # dark rock texture on the slopes
        c.rect(x, y, x + 1, y, 3)
    c.rect(0, 20, W - 1, 20, 3)
    c.rect(0, 21, W - 1, H - 1, 2)
    c.poly([(12, 24), (20, 24), (17, 21), (15, 21)], 1)            # the trail
    boulder(c, 5, 24, 2)
    boulder(c, 27, 23, 2)
    return c


def vanished_woods():
    """Vanished Woods: a jungle - big tropical leaves (split, with veins) framing the picture from both sides,
    vines hanging from the top, tall trunks behind, light falling in shafts onto a path into the trees."""
    c = C(0)
    for x in (9, 14, 19, 24):                                      # trunks behind
        c.rect(x, 0, x + 1, 17, 1)
    for i in range(0, 32, 2):                                      # light shafts (dithered, diagonal)
        c.line(10 + i * 0.4, 0, 14 + i * 0.4, 17, 0)
    c.rect(0, 17, W - 1, 17, 2)
    c.rect(0, 18, W - 1, H - 1, 2)
    c.poly([(13, 24), (19, 24), (17, 18), (15, 18)], 1)            # path
    for x, ln in ((11, 6), (16, 4), (22, 7)):                       # vines
        for y in range(ln):
            c.set(x + (y % 3 == 2), y, 3)
        c.set(x - 1, ln - 1, 2)
    def leaf(cx, cy, dx, dy, ln, wd):
        for k in range(ln):
            t = k / ln
            mx, my = cx + dx * k, cy + dy * k
            half = int(round(wd * math.sin(math.pi * min(1, t * 1.15))))
            for s in range(-half, half + 1):
                c.set(mx - dy * s, my + dx * s, 3)
        for k in range(1, ln - 1):                                 # the midrib
            c.set(cx + dx * k, cy + dy * k, 2)
        for k in range(2, ln - 1, 2):                              # splits in the leaf edge
            c.set(cx + dx * k - dy * wd, cy + dy * k + dx * wd, 0)
    leaf(0, 4, 0.9, 0.45, 10, 3)
    leaf(0, 14, 0.85, -0.5, 9, 3)
    leaf(31, 3, -0.9, 0.45, 10, 3)
    leaf(31, 14, -0.85, -0.45, 9, 3)
    leaf(4, 23, 0.4, -0.9, 7, 2)
    leaf(27, 23, -0.4, -0.9, 7, 2)
    return c


def gemstone_cavern():
    """Gemstone Cavern: a dark cave, rough rock walls and stalactites, a cluster of big glittering crystals growing
    from the floor in the middle with sparkles round them."""
    c = C(3)
    c.poly([(2, 24), (3, 7), (9, 2), (23, 2), (29, 7), (30, 24)], 2)      # the chamber
    for x, ln, w in ((7, 4, 1), (12, 3, 1), (20, 5, 1), (25, 3, 1)):
        spike(c, x, 2, ln, w, down=True, v=3)                      # stalactites
    for x, y in ((5, 12), (26, 10), (6, 18), (27, 17)):            # rock texture
        c.rect(x, y, x + 1, y, 3)
    c.rect(0, 21, W - 1, H - 1, 3)                                 # floor
    c.rect(3, 21, 28, 21, 2)
    gem(c, 16, 22, 3, 13)                                          # the crystals
    gem(c, 11, 22, 2, 8)
    gem(c, 21, 22, 2, 9)
    gem(c, 7, 22, 1, 5)
    gem(c, 25, 22, 1, 5)
    for cx, cy in ((9, 9), (23, 8), (16, 6)):                      # sparkles
        c.set(cx, cy, 0)
        c.set(cx - 1, cy, 1)
        c.set(cx + 1, cy, 1)
        c.set(cx, cy - 1, 1)
        c.set(cx, cy + 1, 1)
    return c


def rural_station():
    """Rural station: a small station building with a deep platform roof on the left, railway tracks running into
    the distance across open country, telegraph poles along the line, low hills."""
    c = C(0)
    far_hills(c, 9, 1.0, 4.0, 1.0)
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, H - 1, 1)
    # tracks to a vanishing point at (21, 11)
    c.line(14, 24, 20.5, 11, 3)
    c.line(30, 24, 21.5, 11, 3)
    for y in (12, 13, 15, 17, 20, 23):                             # sleepers
        t = (y - 11) / 13.0
        c.rect(int(20.5 - 7 * t) - 1, y, int(21.5 + 9 * t) + 1, y, 2)
    c.line(14, 24, 20.5, 11, 3)
    c.line(30, 24, 21.5, 11, 3)
    for x, top in ((27, 4), (23, 8), (21, 9)):                     # telegraph poles, smaller further away
        c.rect(x, top, x, 11, 3)
        c.rect(x - 1, top + 1, x + 1, top + 1, 3)
    c.line(27, 5, 23, 9, 2)
    # station: platform roof on posts, the building under it, the platform
    c.rect(0, 5, 11, 6, 3)
    c.rect(0, 7, 11, 7, 2)
    c.rect(0, 8, 8, 15, 1)
    c.rect(0, 8, 8, 8, 3)
    c.rect(2, 10, 4, 12, 0)                                        # window
    c.rect(6, 11, 7, 15, 3)                                        # door
    c.rect(10, 7, 10, 16, 3)                                       # roof post
    c.rect(4, 2, 7, 4, 3)                                          # clock on the roof
    c.set(5, 3, 0)
    c.rect(0, 16, 13, 17, 0)                                       # platform
    c.rect(0, 16, 13, 16, 3)
    c.rect(0, 18, 12, 18, 2)
    return c


def illusionary_manor():
    """Illusionary manor: night - a dark dithered sky with a big white moon, a black haunted mansion with two pointed
    towers, a few dimly lit and broken windows, a crooked fence and a bare tree in front."""
    c = C(0)
    c.dither(0, 0, W - 1, 19, 1, 2, 0.5)                           # night sky
    c.disc(25, 5, 3.4, 0)                                          # moon
    c.set(24, 4, 1)
    c.set(26, 6, 1)
    # the manor
    c.rect(7, 9, 24, 19, 3)
    for tx in (8, 21):                                             # towers with pointed roofs
        c.rect(tx, 5, tx + 3, 19, 3)
        for i in range(4):
            c.rect(tx + 1 - (i > 1), 4 - i + (i > 1), tx + 2 + (i > 1), 4 - i + (i > 1), 3)
        c.set(tx + 1, 0, 3)
    c.poly([(11, 9), (15.5, 4), (20, 9)], 3)                       # main gable
    for wx, wy, lit in ((9, 8, 1), (22, 8, 0), (12, 11, 1), (18, 11, 2), (12, 15, 2), (18, 15, 1), (15, 7, 1)):
        c.rect(wx, wy, wx + 1, wy + 1, lit)                        # windows: lit, dark, broken
    c.set(18, 11, 1)
    c.rect(15, 14, 16, 19, 1)                                      # door
    c.rect(0, 20, W - 1, H - 1, 3)
    c.rect(0, 20, W - 1, 20, 2)
    for x in range(1, 31, 3):                                      # crooked fence
        c.rect(x, 18 + (x % 2), x, 21, 3)
    c.rect(0, 19, W - 1, 19, 3)
    gnarled_tree(c, 3, 21, 12, lean=1)
    return c


def sunbaked_desert():
    """Sunbaked desert: a big sun with rays, rolling sand dunes in layers (lit faces light, shaded faces darker,
    wind ripples), a tall cactus in front."""
    c = C(0)
    c.disc(7, 5, 3, 1)                                             # sun
    for dx, dy in ((0, -5), (0, 5), (-5, 0), (5, 0), (-4, -4), (4, -4), (-4, 4), (4, 4)):
        c.set(7 + dx, 5 + dy, 1)
    c.ground(lambda x: 11 + 2.2 * math.sin(x / 4.5 + 2.0), 1)       # far dunes
    c.ground(lambda x: 15 + 2.6 * math.sin(x / 5.5 + 0.3), 0)       # middle dune, lit
    for x in range(W):                                             # its shaded side
        crest = 15 + 2.6 * math.sin(x / 5.5 + 0.3)
        if math.cos(x / 5.5 + 0.3) < -0.2:
            c.rect(x, int(crest) + 1, x, int(crest) + 3, 1)
    for x in range(W):
        y = int(15 + 2.6 * math.sin(x / 5.5 + 0.3))
        c.set(x, y, 2)                                             # crest line
    c.ground(lambda x: 20 + 1.4 * math.sin(x / 6.0 + 4.0), 1)       # near sand
    for y, x0, x1 in ((22, 3, 8), (23, 12, 18), (21, 20, 24)):
        c.rect(x0, y, x1, y, 2)                                    # wind ripples
    # cactus
    c.rect(23, 7, 25, 21, 3)
    c.rect(24, 8, 24, 20, 2)
    c.rect(20, 11, 22, 12, 3)
    c.rect(20, 8, 21, 12, 3)
    c.rect(26, 13, 28, 14, 3)
    c.rect(27, 10, 28, 14, 3)
    return c


def coral_reef():
    """Coral reef, underwater: the surface far above with light rays slanting down, deepening water, branching and
    round corals on a sandy floor, a striped fish, bubbles rising."""
    c = C(1)
    c.rect(0, 0, W - 1, 1, 0)                                      # the surface, bright
    for x in range(0, W, 4):
        c.set(x + 1, 2, 0)
    for x0 in (4, 13, 22):                                         # light rays
        c.line(x0, 2, x0 + 5, 16, 0)
    c.dither(0, 14, W - 1, 20, 1, 2, 0.35)                         # deeper water
    c.rect(0, 21, W - 1, H - 1, 0)                                 # sand
    c.rect(0, 21, W - 1, 21, 2)
    def branch(x, ground, h, side):
        c.rect(x, ground - h, x, ground - 1, 3)
        for k, (bx, by) in enumerate(((x - 2, ground - h + 2), (x + 2, ground - h + 1), (x - 3, ground - h + 5))):
            c.line(x, by + 2, bx, by, 3)
            c.set(bx, by - 1, 3)
    branch(5, 22, 9, 1)
    branch(26, 22, 11, -1)
    c.disc(12, 20, 3, 2)                                           # brain coral
    c.line(10, 19, 14, 20, 3)
    c.line(10, 21, 14, 21, 3)
    c.disc(20, 21, 2, 3)                                           # round coral
    c.set(19, 20, 2)
    # fish: body, tail, eye, stripes
    c.poly([(13, 9), (17, 7), (21, 9), (17, 11)], 3)
    c.poly([(21, 9), (24, 7), (24, 11)], 3)
    c.set(15, 8, 0)
    c.rect(17, 8, 17, 10, 0)
    c.rect(19, 8, 19, 10, 0)
    for x, y in ((9, 6), (10, 4), (9, 2), (28, 9), (27, 6)):       # bubbles
        c.set(x, y, 0)
    return c


def snowy_summit():
    """Snowy summit: one big mountain peak filling the picture, a thick snow cap with a jagged lower edge, a lit
    left face and a shadowed right face, snow falling, small snowy pines at its foot."""
    c = C(0)
    c.poly([(0, 21), (16, 2), (31, 21)], 2)                        # the peak
    for y in range(2, 21):                                         # shadowed right face
        half = (y - 2) * 16 / 19.0
        c.rect(int(16 + half * 0.15), y, int(16 + half), y, 3)
    snow_line = [(16 - (y - 2) * 0.84, y) for y in range(2, 12)]
    for y in range(2, 12):                                         # snow cap, jagged at the bottom
        half = (y - 2) * 16 / 19.0
        jag = 0 if y < 9 else (1 if (y % 2) else 0)
        c.rect(int(16 - half), y, int(16 + half * 0.15) - jag, y, 0)
        c.rect(int(16 + half * 0.15), y, int(16 + half * 0.5) - jag, y, 1)
    c.line(0, 21, 16, 2, 3)
    c.line(16, 2, 31, 21, 3)
    for x, y in ((7, 13), (10, 15), (12, 12)):                      # snow patches lower down
        c.rect(x, y, x + 1, y, 0)
    c.rect(0, 20, W - 1, H - 1, 0)                                 # snowy ground
    c.rect(0, 20, W - 1, 20, 2)
    for x, h in ((3, 7), (28, 8), (24, 5)):
        pine2(c, x, 22, h, snow=True)
    for x, y in ((3, 3), (8, 6), (25, 2), (29, 6), (21, 8), (5, 10)):
        c.set(x, y, 1)                                             # falling snow
    return c


def dragon_hollow():
    """Dragon Hollow: a deep canyon - tall layered rock walls on both sides - and a big dragon flying through the gap
    in the sky, wings spread; a rocky floor below."""
    c = C(0)
    c.poly([(0, 0), (8, 0), (10, 6), (9, 12), (12, 18), (12, 24), (0, 24)], 2)     # left wall
    c.poly([(31, 0), (24, 0), (22, 5), (24, 11), (21, 18), (21, 24), (31, 24)], 2)  # right wall
    for y in (4, 9, 14, 19):                                       # strata
        c.line(0, y, 10, y + 1, 3)
        c.line(22, y + 1, 31, y, 3)
    c.line(8, 0, 10, 6, 3)
    c.line(10, 6, 9, 12, 3)
    c.line(9, 12, 12, 18, 3)
    c.line(24, 0, 22, 5, 3)
    c.line(22, 5, 24, 11, 3)
    c.line(24, 11, 21, 18, 3)
    c.rect(2, 2, 5, 2, 1)
    c.rect(26, 7, 29, 7, 1)
    c.rect(12, 18, 21, 18, 3)                                      # canyon floor
    c.rect(12, 19, 21, H - 1, 1)
    boulder(c, 16, 23, 2)
    # dragon: body, neck and head to the left, tail to the right, two big wings up
    c.line(13, 10, 19, 10, 3)
    c.line(12, 9, 13, 10, 3)
    c.rect(11, 8, 12, 9, 3)                                        # head
    c.set(10, 9, 3)
    c.line(19, 10, 22, 12, 3)                                      # tail
    c.set(23, 12, 3)
    c.poly([(14, 10), (11, 3), (13, 4), (15, 3), (17, 9)], 3)      # wing (near)
    c.poly([(16, 10), (19, 4), (20, 6), (22, 5), (19, 10)], 3)     # wing (far)
    c.line(13, 4, 15.5, 9, 2)                                      # wing bones
    c.line(20, 6, 18, 10, 2)
    c.set(11, 8, 0)                                                # eye
    return c


SM1 = {"boardwalk": boardwalk, "city_square": city_square, "shopping_plaza": shopping_plaza,
       "seaside_cliff": seaside_cliff, "clear_lake": clear_lake, "volcanic_trail": volcanic_trail,
       "vanished_woods": vanished_woods, "gemstone_cavern": gemstone_cavern, "rural_station": rural_station,
       "illusionary_manor": illusionary_manor, "sunbaked_desert": sunbaked_desert, "coral_reef": coral_reef,
       "snowy_summit": snowy_summit, "dragon_hollow": dragon_hollow}
COURSES.update(SM1)


# --- Sun / Moon round 2 (own review of round 1: MALL read "HALL", pier and tracks looked alike, the fountain read as
# a bench, jungle leaves were blobs, the summit read as a tent, the dragon was a blob, the fish was unclear)

def big_m(c, x, y, v):
    for r, row in enumerate(["#...#", "##.##", "#.#.#", "#...#", "#...#"]):
        for k, p in enumerate(row):
            if p == "#":
                c.set(x + k, y + r, v)


def shopping_plaza2():
    c = shopping_plaza()
    c.rect(5, 0, 26, 5, 3)                                         # wider sign, 5-pixel M
    big_m(c, 7, 0, 0)
    text3(c, 13, 0, "ALL", 0)
    return c


def boardwalk2():
    """Boardwalk, side view: a long wooden walkway on stilts over the water with a railing, a small hut at its far
    end, the beach at the left with a palm tree, the sun low over the sea, gulls."""
    c = C(0)
    c.rect(0, 10, W - 1, 10, 2)                                    # sea horizon
    c.rect(0, 11, W - 1, H - 1, 1)
    c.disc(24, 6, 2.5, 1)
    c.rect(21, 10, 27, 10, 2)
    for y, xs in ((13, (20, 27)), (20, (14, 24)), (23, (9, 19, 29))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    c.poly([(0, 14), (7, 16), (11, 24), (0, 24)], 0)               # beach
    c.line(0, 14, 7, 16, 2)
    c.line(7, 16, 11, 24, 2)
    # walkway: deck, railing, stilts
    c.rect(6, 15, 31, 15, 3)
    c.rect(6, 16, 31, 16, 0)
    c.rect(6, 17, 31, 17, 3)
    for x in range(9, 32, 2):
        c.set(x, 16, 2)                                            # plank ends
    c.rect(6, 12, 31, 12, 3)                                       # hand rail
    for x in range(7, 32, 4):
        c.rect(x, 13, x, 14, 3)
        c.rect(x, 18, x, 22, 3)                                    # stilts into the water
        c.rect(x - 1, 22, x + 1, 22, 0)
    c.rect(24, 6, 30, 11, 1)                                       # hut at the end
    c.rect(24, 11, 30, 11, 3)
    c.poly([(23, 7), (27, 3), (31, 7)], 3)
    c.rect(26, 8, 27, 10, 3)
    palm(c, 3, 15, 12, lean=1)
    gsc_bird(c, 11, 4)
    gsc_bird(c, 17, 6)
    return c


def city_square2():
    """City square: a big round fountain in front of the town hall - white jets of water against the grey building,
    a wide stone basin, lamp posts either side, a paved plaza."""
    c = C(0)
    c.rect(4, 3, 27, 15, 1)                                        # the building behind
    c.poly([(3, 4), (15.5, 0), (28, 4)], 2)
    c.line(3, 4, 15.5, 0, 3)
    c.line(15.5, 0, 28, 4, 3)
    for x in range(6, 26, 4):
        c.rect(x, 6, x + 1, 9, 2)
    c.rect(0, 15, W - 1, 15, 2)
    c.rect(0, 16, W - 1, H - 1, 0)
    for y in (18, 21):
        c.rect(0, y, W - 1, y, 1)
    # fountain
    c.rect(6, 17, 25, 21, 3)                                       # basin wall
    c.rect(7, 17, 24, 18, 1)                                       # water in it
    c.rect(7, 20, 24, 20, 2)
    c.rect(15, 9, 16, 17, 3)                                       # column
    c.rect(11, 9, 20, 9, 3)                                        # upper bowl
    c.rect(12, 10, 19, 10, 3)
    for side in (-1, 1):                                           # white jets falling into the basin
        for k in range(7):
            c.set(15.5 + side * (5.5 + k * 0.5), 9 + k * 1.2, 0)
            c.set(15.5 + side * (6.5 + k * 0.5), 9 + k * 1.2, 0)
    for x, y in ((15, 6), (16, 5), (14, 7), (17, 7), (15, 8), (16, 8)):
        c.set(x, y, 0)                                             # top spray
    lamp_post(c, 2, 8, 23)
    lamp_post(c, 29, 8, 23)
    return c


def vanished_woods2():
    """Vanished Woods: deep jungle - a dark leafy canopy across the top, two thick trunks with buttress roots framing
    the picture, long vines with leaves hanging down, ferns at the bottom, a light clearing in the middle."""
    c = C(0)
    c.rect(0, 0, W - 1, 3, 3)                                      # canopy
    for x in range(0, W, 3):
        c.rect(x, 4, x + 1, 4 + (x * 7) % 3, 3)
    for x in range(1, W, 3):
        c.set(x, 3, 2)
    for x0 in (11, 19):                                            # far trunks
        c.rect(x0, 4, x0 + 1, 17, 1)
    for x0, s in ((2, 1), (27, -1)):                               # big trunks with buttress roots
        c.rect(x0, 4, x0 + 2, 19, 3)
        c.rect(x0 + 1, 6, x0 + 1, 16, 2)
        c.line(x0 + (3 if s > 0 else -1), 15, x0 + (6 if s > 0 else -4), 19, 3)
        c.line(x0 + (1 if s > 0 else 1), 16, x0 + (-1 if s > 0 else 3), 19, 3)
    for x, ln in ((8, 9), (14, 6), (22, 10), (17, 4)):              # vines with leaves
        c.rect(x, 4, x, 4 + ln, 3)
        for y in range(6, 4 + ln, 3):
            c.set(x - 1, y, 2)
            c.set(x + 1, y + 1, 2)
    c.rect(0, 19, W - 1, 19, 2)
    c.rect(0, 20, W - 1, H - 1, 1)
    c.poly([(13, 24), (19, 24), (17, 19), (15, 19)], 0)            # path into the clearing
    for fx, fs in ((6, 1), (24, -1), (10, 1), (21, -1)):           # ferns: arching fronds
        for k in range(5):
            c.set(fx + fs * k, 22 - (k if k < 3 else 5 - k), 3)
            c.set(fx + fs * k, 23 - (k if k < 3 else 5 - k), 2)
    return c


def rural_station2():
    """Rural station: railway tracks running straight to the horizon through open fields, a small station house with
    a pitched roof and its platform beside the line, a station sign, telegraph poles getting smaller."""
    c = C(0)
    far_hills(c, 9, 1.0, 4.5, 2.0)
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, H - 1, 1)
    vx = 20                                                        # vanishing point
    for y in range(12, H):                                         # gravel bed
        t = (y - 11) / 12.0
        c.rect(int(vx - 1 - 9 * t), y, int(vx + 1 + 9 * t), y, 0)
    for y in (13, 15, 18, 22):                                     # sleepers
        t = (y - 11) / 12.0
        c.rect(int(vx - 7 * t), y, int(vx + 7 * t), y, 2)
    c.line(vx - 0.5, 11, vx - 7, 23, 3)                            # rails
    c.line(vx + 0.5, 11, vx + 7, 23, 3)
    for x, top in ((30, 3), (26, 7), (24, 9)):                     # telegraph poles
        c.rect(x, top, x, 11 + (x - 24) // 2, 3)
        c.rect(x - 1, top + 1, x + 1, top + 1, 3)
    c.line(30, 4, 26, 8, 2)
    c.line(26, 8, 24, 10, 2)
    # station house and platform on the left
    c.poly([(0, 7), (5, 3), (10, 7)], 3)
    c.rect(1, 7, 9, 14, 0)
    c.rect(1, 7, 1, 14, 3)
    c.rect(9, 7, 9, 14, 3)
    c.rect(3, 9, 4, 11, 2)
    c.rect(6, 10, 7, 14, 3)
    c.rect(0, 15, 11, 16, 0)                                       # platform
    c.rect(0, 15, 11, 15, 3)
    c.rect(0, 17, 11, 17, 2)
    c.rect(12, 9, 12, 15, 3)                                       # station sign
    c.rect(11, 8, 15, 10, 3)
    c.rect(12, 9, 14, 9, 0)
    return c


def snowy_summit2():
    """Snowy summit: one big mountain - dark rock with a black shadow side, a thick white snow cap with a jagged
    edge, outlined against the sky - a smaller grey peak behind, snow falling, snowy pines at its foot."""
    c = C(0)
    c.poly([(16, 21), (25, 7), (32, 15), (32, 21)], 1)             # peak behind
    c.poly([(0, 21), (13, 2), (28, 21)], 2)                        # the big peak
    c.poly([(13, 2), (28, 21), (17, 21)], 3)                       # shadow side
    c.poly([(13, 2), (7, 10), (9, 9), (10, 11), (12, 9), (14, 11), (16, 9), (18, 10), (13, 2)], 0)  # snow cap
    c.line(0, 21, 13, 2, 3)
    c.line(13, 2, 28, 21, 3)
    c.line(13, 2, 18, 10, 1)                                       # snow on the shadow side, grey
    c.rect(0, 20, W - 1, H - 1, 0)
    c.rect(0, 20, W - 1, 20, 2)
    for x, h in ((3, 7), (29, 8), (25, 6)):
        pine2(c, x, 23, h, snow=True)
    for x, y in ((3, 3), (8, 1), (22, 2), (28, 5), (20, 7), (4, 10)):
        c.set(x, y, 1)
    return c


def dragon_hollow2():
    """Dragon Hollow: a deep canyon with layered walls, and a big dragon in the gap of sky - long neck and head to
    the left, two bat-like wings raised with scalloped edges, a long tail - over a rocky floor."""
    c = C(0)
    c.poly([(0, 0), (6, 0), (7, 8), (6, 14), (9, 24), (0, 24)], 2)
    c.poly([(31, 0), (25, 0), (24, 7), (26, 14), (23, 24), (31, 24)], 2)
    for y in (5, 11, 17):
        c.line(0, y, 7, y + 1, 3)
        c.line(24, y + 1, 31, y, 3)
    for pts in (((6, 0), (7, 8), (6, 14), (9, 24)), ((25, 0), (24, 7), (26, 14), (23, 24))):
        for a, b in zip(pts, pts[1:]):
            c.line(a[0], a[1], b[0], b[1], 3)
    c.rect(8, 21, 23, 21, 3)
    c.rect(8, 22, 23, H - 1, 1)
    boulder(c, 13, 24, 1)
    boulder(c, 19, 24, 2)
    # the dragon
    for x in range(12, 21):                                        # body
        c.rect(x, 12, x, 13, 3)
    c.line(12, 12, 9, 9, 3)                                        # neck
    c.line(12, 13, 10, 10, 3)
    c.rect(7, 8, 9, 9, 3)                                          # head + snout
    c.set(6, 9, 3)
    c.set(8, 7, 3)                                                 # horn
    c.set(8, 8, 0)                                                 # eye
    c.line(20, 13, 23, 15, 3)                                      # tail
    c.line(23, 15, 22, 18, 3)
    c.set(21, 18, 3)
    c.poly([(13, 12), (9, 2), (11, 4), (12, 3), (14, 6), (15, 5), (17, 12)], 3)     # near wing
    c.poly([(17, 12), (21, 1), (22, 4), (23, 3), (23, 7), (20, 12)], 3)          # far wing
    c.line(12, 4, 15, 11, 2)                                       # wing bones
    c.line(21, 4, 19, 11, 2)
    c.rect(13, 14, 13, 15, 3)                                      # legs
    c.rect(18, 14, 18, 15, 3)
    return c


def coral_reef2():
    """Coral reef: as round 1, with a bigger, clearly drawn fish (round body, white stripe, forked tail, eye)."""
    c = coral_reef()
    c.rect(12, 5, 25, 12, 1)                                       # clear the old fish
    for y in range(6, 12):                                         # body: an ellipse
        half = int(round(4.5 * math.sqrt(max(0, 1 - ((y - 8.5) / 3.0) ** 2))))
        c.rect(17 - half, y, 17 + half, y, 3)
    c.rect(18, 6, 18, 11, 0)                                       # white stripe
    c.poly([(21, 8.5), (25, 5), (24, 8.5), (25, 12)], 3)           # forked tail
    c.set(14, 8, 0)                                                # eye
    c.set(12, 9, 3)                                                # mouth
    return c


SM1.update({"boardwalk": boardwalk2, "city_square": city_square2, "shopping_plaza": shopping_plaza2,
            "vanished_woods": vanished_woods2, "rural_station": rural_station2, "snowy_summit": snowy_summit2,
            "dragon_hollow": dragon_hollow2, "coral_reef": coral_reef2})
COURSES.update(SM1)


# --- Sun / Moon round 3 (own review: the summit still a perfect triangle = a tent; the dragon's wings read as ears)

def snowy_summit3():
    """Snowy summit: a craggy mountain - an off-centre main peak with a lower shoulder on each side, dark rock with a
    black shadow side, a white snow cap with a jagged edge reaching down the ridges - snow falling, snowy pines."""
    c = C(0)
    ridge = [(0, 21), (4, 15), (7, 12), (9, 13), (14, 3), (17, 7), (20, 6), (25, 13), (28, 12), (32, 18), (32, 21)]
    c.poly(ridge, 2)
    c.poly([(14, 3), (17, 7), (20, 6), (25, 13), (28, 12), (32, 18), (32, 21), (19, 21), (16, 12)], 3)  # shadow
    c.poly([(14, 3), (8, 13), (10, 12), (11, 14), (13, 11.5), (14.5, 14), (16, 11), (17.5, 12.5), (18.5, 9),
            (17, 7)], 0)                                           # snow cap, well down the lit face
    c.poly([(17, 7), (20, 6), (23, 10.5), (21.5, 10), (20.5, 12), (19, 10), (18, 11)], 1)  # snow, shadow side
    for a, b in zip(ridge, ridge[1:]):
        c.line(a[0], a[1], b[0], b[1], 3)
    c.set(5, 14, 0)                                                # snow patches lower down
    c.rect(7, 13, 8, 13, 0)
    c.rect(0, 20, W - 1, H - 1, 0)
    c.rect(0, 20, W - 1, 20, 2)
    for x, h in ((3, 7), (29, 8), (25, 6)):
        pine2(c, x, 23, h, snow=True)
    for x, y in ((3, 3), (8, 1), (24, 2), (29, 5), (22, 9), (5, 8)):
        c.set(x, y, 1)
    return c


def dragon_hollow3():
    """Dragon Hollow: a dragon seen from the front with its great wings spread wide like a bat's (pointed tips,
    scalloped lower edge, wing bones), horned head, body and tail - flying over a rocky canyon floor between low
    layered cliffs."""
    c = C(0)
    c.poly([(0, 12), (4, 13), (6, 18), (8, 24), (0, 24)], 2)       # low canyon walls
    c.poly([(31, 11), (27, 13), (25, 18), (23, 24), (31, 24)], 2)
    for pts in (((0, 12), (4, 13), (6, 18), (8, 24)), ((31, 11), (27, 13), (25, 18), (23, 24))):
        for a, b in zip(pts, pts[1:]):
            c.line(a[0], a[1], b[0], b[1], 3)
    c.line(0, 17, 5, 17, 3)
    c.line(26, 17, 31, 16, 3)
    c.rect(7, 21, 24, 21, 3)
    c.rect(7, 22, 24, H - 1, 1)
    boulder(c, 12, 24, 1)
    boulder(c, 20, 24, 1)
    # wings: from the shoulders up to pointed tips, the lower edge scalloped between the finger bones
    for s in (-1, 1):
        sx = 16 + s * 2                                            # shoulder
        tip = (16 + s * 15, 2)
        lower = [(16 + s * 14, 6), (16 + s * 12, 9), (16 + s * 10, 7), (16 + s * 8, 10), (16 + s * 6, 8),
                 (16 + s * 4, 11), (sx, 11)]
        c.poly([(sx, 6), (16 + s * 6, 3), tip] + lower, 3)
        for (fx, fy) in ((16 + s * 12, 8), (16 + s * 8, 9), (16 + s * 4, 10)):
            c.line(sx, 6.5, fx, fy, 2)                             # wing bones
        c.line(sx, 6, tip[0], tip[1], 3)
    c.rect(15, 5, 16, 14, 3)                                       # body
    c.rect(14, 7, 17, 12, 3)
    c.rect(15, 8, 16, 11, 2)                                       # belly
    c.rect(14, 1, 17, 4, 3)                                        # head
    c.set(13, 0, 3)                                                # horns
    c.set(18, 0, 3)
    c.set(15, 2, 0)                                                # eyes
    c.set(16, 2, 0)
    c.rect(15, 15, 15, 17, 3)                                      # tail, curling
    c.rect(16, 17, 17, 17, 3)
    c.set(18, 16, 3)
    c.rect(13, 13, 13, 14, 3)                                      # legs
    c.rect(18, 13, 18, 14, 3)
    return c


SM1.update({"snowy_summit": snowy_summit3, "dragon_hollow": dragon_hollow3})
COURSES.update(SM1)


# --- Sun / Moon round 4 (user on the dragon and the fish: "I dont like these two. They too clearly show real
# animals"): places only, no creatures.

def dragon_hollow4():
    """Dragon Hollow: a deep, narrow canyon - tall layered walls on both sides - with a rope-and-plank bridge slung
    across the gap (sagging, with hand ropes and posts on each edge), a river far below."""
    c = C(0)
    far_hills(c, 2, 0.8, 3.0, 0.0)                                 # hills far beyond the canyon (drawn first: it
                                                                    # fills down to the horizon row)
    left = [(0, 6), (9, 6), (10, 11), (8, 16), (11, 24), (0, 24)]
    right = [(31, 5), (22, 5), (21, 10), (23, 16), (20, 24), (31, 24)]
    c.poly(left, 2)
    c.poly(right, 2)
    c.poly([(9, 6), (10, 11), (8, 16), (11, 24), (8, 24), (5, 16), (7, 11), (6, 6)], 3)    # shadowed inner faces
    for pts in (left[:5], right[:5]):
        for a, b in zip(pts, pts[1:]):
            c.line(a[0], a[1], b[0], b[1], 3)
    for y in (10, 14, 19):                                         # strata
        c.line(0, y, 7, y + 1, 1)
        c.line(23, y + 1, 31, y, 1)
    c.rect(0, 5, 9, 5, 1)                                          # grassy rims
    c.rect(22, 4, 31, 4, 1)
    c.poly([(11, 24), (20, 24), (19, 20), (12, 20)], 1)            # river at the bottom
    c.rect(13, 22, 15, 22, 0)
    c.rect(16, 21, 17, 21, 0)
    # the bridge: posts on both rims, a sagging deck of planks, hand ropes above it
    for x in (8, 23):
        c.rect(x, 2, x, 6, 3)
    for x in range(9, 23):
        sag = 6 + 2.2 * math.sin(math.pi * (x - 8) / 15.0)
        c.set(x, sag, 3)                                           # deck
        if x % 2 == 0:
            c.set(x, sag + 1, 3)                                   # plank ends
        c.set(x, sag - 3.2 + 0.4 * math.sin(math.pi * (x - 8) / 15.0), 3)   # hand rope
        if x % 3 == 0:
            c.line(x, sag - 3, x, sag, 2)                          # ropes down to the deck
    return c


def coral_reef4():
    """Coral reef, underwater, no creatures: the bright surface far above with light rays slanting down, deepening
    water, one big branching coral in the middle, tube sponges and swaying seaweed either side, bubbles rising."""
    c = C(1)
    c.rect(0, 0, W - 1, 1, 0)
    for x in range(0, W, 4):
        c.set(x + 1, 2, 0)
    for x0 in (3, 22):                                             # light rays
        c.line(x0, 2, x0 + 5, 16, 0)
    c.dither(0, 14, W - 1, 20, 1, 2, 0.35)
    c.rect(0, 21, W - 1, H - 1, 0)                                 # sand
    c.rect(0, 21, W - 1, 21, 2)
    # big branching coral: a trunk splitting into upward branches, rounded tips
    def coral_branch(x0, y0, dx, ln):
        x, y = x0, y0
        for k in range(ln):
            x += dx * (0.6 if k < ln // 2 else 0.2)
            y -= 1
            c.set(x, y, 3)
            c.set(x + 1, y, 3)
        c.rect(int(x) - 1, int(y) - 1, int(x) + 2, int(y), 3)
        c.set(int(x), int(y) - 1, 2)
    c.rect(15, 15, 17, 21, 3)                                      # trunk
    for x0, y0, dx, ln in ((15, 16, -1, 8), (17, 16, 1, 8), (16, 14, -0.4, 9), (16, 14, 0.5, 10),
                           (14, 18, -1.2, 5), (18, 18, 1.2, 5)):
        coral_branch(x0, y0, dx, ln)
    c.rect(16, 16, 16, 20, 2)
    for x, h in ((4, 6), (6, 4), (26, 5)):                         # tube sponges
        c.rect(x, 21 - h, x + 1, 20, 3)
        c.set(x, 21 - h, 1)
        c.set(x + 1, 21 - h, 1)
    for x, h, s in ((9, 9, 1), (23, 8, -1), (29, 10, 1)):           # seaweed, swaying
        for k in range(h):
            c.set(x + (s if (k // 2) % 2 else 0), 20 - k, 2)
    for x, y in ((10, 6), (11, 4), (10, 2), (22, 8), (23, 5)):     # bubbles
        c.set(x, y, 0)
    return c


SM1.update({"dragon_hollow": dragon_hollow4, "coral_reef": coral_reef4})
COURSES.update(SM1)


# --- Sun / Moon round 5 (user: "Vanished Woods needs more dense forestry"; "Rural station doesn't look much like a
# train station")

def vanished_woods5():
    """Vanished Woods: a dense forest in three layers - far trunks (light grey), a middle row (dark grey), near trees
    (black) - with overlapping leaf masses closing the whole top, thick undergrowth along the bottom, a narrow path."""
    c = C(0)
    for x in range(1, W, 3):                                       # far layer: many thin trunks
        c.rect(x, 4, x, 17, 1)
    for cx, cy, r in ((3, 3, 4), (10, 2, 4), (17, 3, 4), (24, 2, 4), (31, 3, 4)):
        c.disc(cx, cy, r, 1)                                       # far canopy
    for x in (4, 9, 15, 21, 26):                                   # middle row of trunks
        c.rect(x, 5, x + 1, 18, 2)
    for cx, cy, r in ((1, 1, 4), (7, 3, 4), (13, 1, 4), (19, 2, 4), (25, 1, 4), (31, 2, 4)):
        c.disc(cx, cy, r, 2)                                       # middle canopy
    for cx, cy in ((7, 3), (19, 2), (31, 2)):
        c.set(cx - 2, cy - 2, 1)                                   # light on the leaves
    for x0 in (0, 12, 28):                                         # near trees: thick black trunks, black crowns
        c.rect(x0, 3, x0 + 2, 19, 3)
        c.rect(x0 + 1, 6, x0 + 1, 15, 2)
    for cx, cy, r in ((1, 0, 4), (13, 0, 3), (29, 0, 4)):
        c.disc(cx, cy, r, 3)
    for x, y in ((5, 8), (18, 10), (24, 7)):                       # vines
        c.rect(x, 4, x, y, 3)
        c.set(x + 1, y - 2, 2)
    c.rect(0, 18, W - 1, H - 1, 2)                                 # forest floor
    c.poly([(15, 24), (20, 24), (18, 18), (17, 18)], 1)            # narrow path
    for cx, cy, r in ((3, 20, 3), (9, 21, 2.5), (24, 20, 3), (30, 21, 2.5), (13, 22, 2)):
        c.disc(cx, cy, r, 3)                                       # undergrowth
        c.set(cx - 1, cy - 1, 2)
    return c


def rural_station5():
    """Rural station, side view: a long platform canopy on posts across the top, a station clock hanging under it,
    a passenger train standing at the platform (windows, doors, a roof line, wheels on the rail), the platform edge
    in front."""
    c = C(0)
    far_hills(c, 7, 0.8, 4.0, 1.5)
    c.rect(0, 0, W - 1, 2, 3)                                      # canopy roof
    c.rect(0, 3, W - 1, 3, 2)
    for x in (2, 15, 28):                                          # canopy posts
        c.rect(x, 4, x, 18, 3)
    c.rect(19, 4, 19, 4, 3)                                        # round station clock under the canopy
    c.disc(19, 7, 2.3, 3)
    c.disc(19, 7, 1.4, 0)
    c.set(19, 6, 3)                                                # hands
    c.set(19, 7, 3)
    c.set(20, 7, 3)
    # the train: one long carriage across the picture, its nose at the right
    c.rect(0, 9, 26, 16, 1)
    c.rect(0, 9, 26, 9, 3)                                         # roof line
    c.rect(0, 16, 26, 16, 3)
    c.poly([(26, 9), (29, 11), (30, 16), (26, 16)], 2)             # rounded nose
    c.line(26, 9, 29, 11, 3)
    c.line(29, 11, 30, 16, 3)
    c.rect(27, 11, 28, 12, 0)                                      # driver's window
    c.rect(0, 13, 26, 13, 2)                                       # stripe
    for x in range(1, 25, 4):                                      # windows
        if x in (9, 21):
            continue
        c.rect(x, 10, x + 2, 12, 0)
    for x in (9, 21):                                              # doors
        c.rect(x, 10, x + 2, 16, 2)
        c.rect(x + 1, 10, x + 1, 16, 3)
    for x in (4, 7, 18, 22, 27):                                   # wheels
        c.rect(x, 17, x + 1, 17, 3)
    c.rect(0, 18, W - 1, 18, 3)                                    # rail
    c.rect(0, 19, W - 1, 19, 2)
    c.rect(0, 20, W - 1, 20, 3)                                    # platform edge
    c.rect(0, 21, W - 1, H - 1, 0)                                 # platform
    for x in range(0, W, 4):
        c.rect(x, 21, x + 1, 21, 1)                                # yellow-line dashes
    return c


SM1.update({"vanished_woods": vanished_woods5, "rural_station": rural_station5})
COURSES.update(SM1)


# --- Diamond / Pearl / Platinum, set 1 (2026-11-15, user: "Yes, start" on DPPt): pictures for the 10 courses that
# share no name with another game. Places only, no creatures (user: "They too clearly show real animals").
FONT3.update({"R": ["##.", "#.#", "##.", "#.#", "#.#"], "I": ["###", ".#.", ".#.", ".#.", "###"],
              "T": ["###", ".#.", ".#.", ".#.", ".#."]})


def lakefront():
    """Lakefront: a wide calm lake below far mountains, a curved sandy shore in front with a rowboat pulled up,
    a few pines on the right, a cloud."""
    c = C(0)
    c.poly([(0, 9), (6, 4), (11, 7), (17, 2), (23, 6), (28, 3), (32, 7), (32, 9)], 1)   # far mountains
    c.rect(0, 9, W - 1, 9, 2)
    c.rect(0, 10, W - 1, 18, 1)                                    # the lake
    for y, x0, x1 in ((11, 3, 9), (13, 14, 22), (15, 4, 8), (16, 20, 27)):
        c.rect(x0, y, x1, y, 0)
    c.poly([(0, 17), (12, 19), (22, 22), (26, 24), (0, 24)], 0)     # sandy shore
    c.line(0, 17, 12, 19, 2)
    c.line(12, 19, 22, 22, 2)
    c.line(22, 22, 26, 24, 2)
    c.poly([(5, 20), (13, 20), (11, 22), (7, 22)], 3)               # rowboat
    c.rect(7, 20, 11, 20, 2)
    c.line(9, 19, 14, 17, 3)                                       # an oar
    for x, h in ((27, 9), (30, 11), (24, 7)):
        pine2(c, x, 20, h)
    cloud(c, 4, 2, 6)
    return c


def deep_woods():
    """Deep Woods: rows of tall pines getting smaller into the distance, a narrow path leading in between them,
    mushrooms by the path."""
    c = C(0)
    for x in range(0, 32, 3):                                      # far row
        pine2(c, x, 14, 6, far=True)
    c.rect(0, 14, W - 1, H - 1, 1)
    c.poly([(13, 24), (19, 24), (17, 14), (15, 14)], 0)            # path
    for x, h in ((2, 15), (7, 13), (24, 13), (29, 16)):            # near tall pines
        pine2(c, x, 22, h)
    for x, h in ((11, 9), (21, 9)):
        pine2(c, x, 17, h)
    for x, y in ((12, 21), (20, 22)):                              # mushrooms
        c.rect(x - 1, y - 1, x + 1, y - 1, 3)
        c.set(x, y - 2, 3)
        c.set(x, y, 2)
        c.set(x - 1, y - 1, 0)
    return c


def blossom_garden():
    """Blossom Garden: rows of big flowers filling the ground, a white garden arch with climbing flowers in the
    middle, a picket fence along the back, a sun."""
    c = C(0)
    c.disc(27, 4, 2.4, 1)
    c.rect(0, 12, W - 1, H - 1, 1)                                 # the garden bed
    c.rect(0, 12, W - 1, 12, 2)
    for x in range(1, 32, 3):                                      # picket fence along the back
        c.rect(x, 9, x, 12, 3)
        c.set(x, 8, 2)
    c.rect(0, 10, W - 1, 10, 3)
    for x in (10, 21):                                             # garden arch
        c.rect(x, 4, x, 16, 3)
    for k in range(12):
        c.set(10 + k, 4 - round(2 * math.sin(math.pi * k / 11)), 3)
    for x, y in ((10, 6), (21, 8), (11, 3), (20, 3), (10, 11), (21, 13)):
        flower(c, x, y, 0)                                         # climbing flowers
    for y, xs in ((15, (2, 6, 25, 29)), (18, (4, 9, 14, 18, 23, 28)), (21, (1, 7, 12, 17, 22, 27, 31))):
        for x in xs:
            c.set(x, y + 1, 2)                                     # stem
            flower(c, x, y, 0)
    return c


def night_meadow():
    """Night Meadow: a dark night sky full of stars with a crescent moon, a black meadow of tall grass blades, one
    lone tree in silhouette."""
    c = C(0)
    c.dither(0, 0, W - 1, 15, 1, 2, 0.6)                           # night sky (light enough for the black tree)
    c.disc(24, 4, 3, 0)                                            # crescent moon
    c.disc(25.5, 3, 2.6, 2)
    for x, y in ((3, 2), (8, 5), (13, 1), (17, 7), (6, 10), (29, 9), (20, 11), (11, 13), (27, 13)):
        c.set(x, y, 0)
    c.set(13, 0, 1)
    c.rect(0, 16, W - 1, H - 1, 3)                                 # meadow
    for x in range(0, W, 2):                                       # grass blades against the sky
        h = 2 + (x * 7) % 4
        c.rect(x, 16 - h, x, 15, 3)
    tree_round(c, 7, 9, 4, crown=3, trunk=3)                       # lone tree
    for x, y in ((14, 19), (22, 21), (4, 22)):
        c.set(x, y, 2)
    return c


def crumbling_crag():
    """Crumbling Crag: a tall cracked rock face on the right, a narrow ledge path climbing along it, big fallen
    boulders and rubble at its foot."""
    c = C(0)
    far_hills(c, 10, 1.2, 3.0, 1.0)
    c.poly([(12, 24), (14, 10), (17, 4), (22, 1), (32, 0), (32, 24)], 2)    # the crag
    c.line(12, 24, 14, 10, 3)
    c.line(14, 10, 17, 4, 3)
    c.line(17, 4, 22, 1, 3)
    for pts in (((20, 4), (22, 9), (21, 13), (24, 18)), ((27, 2), (26, 7), (29, 12)), ((17, 12), (19, 16))):
        for a, b in zip(pts, pts[1:]):                             # cracks
            c.line(a[0], a[1], b[0], b[1], 3)
    c.line(13, 17, 31, 10, 0)                                      # the ledge path
    c.line(13, 18, 31, 11, 3)
    c.rect(0, 19, 13, H - 1, 1)
    c.rect(0, 19, 13, 19, 2)
    boulder(c, 5, 23, 3)                                           # fallen boulders
    boulder(c, 10, 23, 2)
    for x, y in ((2, 20), (8, 19), (13, 21), (1, 22)):
        c.rect(x, y, x + 1, y, 3)                                  # rubble
    return c


def rugged_hills():
    """Rugged Hills: layered jagged hills - far light, near dark - a dirt track winding over them, scrubby bushes and
    rocks, a sun."""
    c = C(0)
    c.disc(6, 4, 2.5, 1)
    c.poly([(0, 12), (4, 8), (8, 11), (13, 6), (18, 10), (23, 5), (28, 9), (32, 7), (32, 24), (0, 24)], 1)
    c.poly([(0, 17), (6, 12), (11, 15), (16, 11), (22, 16), (27, 12), (32, 15), (32, 24), (0, 24)], 2)
    for a, b in (((0, 17), (6, 12)), ((6, 12), (11, 15)), ((11, 15), (16, 11)), ((16, 11), (22, 16)),
                 ((22, 16), (27, 12)), ((27, 12), (32, 15))):
        c.line(a[0], a[1], b[0], b[1], 3)
    for k in range(0, 24):                                         # the winding track
        t = k / 23.0
        x = 16 + 7 * math.sin(t * 5.0) * (1 - t * 0.5)
        y = 24 - t * 12
        c.set(x, y, 0)
        c.set(x + 1, y, 0 if t < 0.6 else 1)
    for x, y in ((4, 20), (26, 21), (9, 17), (29, 18)):            # scrub bushes
        c.disc(x, y, 1.6, 3)
        c.set(x - 1, y - 1, 2)
    boulder(c, 21, 23, 2)
    return c


def marsh_wetlands():
    """Marsh Wetlands: murky pools between grassy hummocks, cattails, two dead tree stumps, a plank walkway across
    the marsh, mist on the far side."""
    c = C(0)
    c.dither(0, 6, W - 1, 9, 0, 1, 0.3)                            # mist
    c.rect(0, 10, W - 1, H - 1, 2)                                 # marsh ground
    for x0, y0, x1, y1 in ((2, 12, 12, 15), (17, 11, 30, 14), (5, 18, 16, 21), (21, 17, 31, 20)):
        c.dither(x0, y0, x1, y1, 1, 2, 0.4)                        # murky pools
        c.rect(x0 + 1, y0 + 1, x0 + 4, y0 + 1, 0)                  # a glint
    c.rect(0, 10, W - 1, 10, 3)
    for x in range(7, 27, 2):                                      # plank walkway, rising away
        y = 23 - (x - 7) // 2
        c.rect(x, y, x + 1, y, 0)
        c.set(x, y + 1, 3)
    for x, top in ((3, 6), (14, 8), (28, 5), (24, 9)):             # cattails
        c.rect(x, top, x, 12, 3)
        c.rect(x, top, x, top + 2, 2)
    for x, h in ((9, 6), (20, 8)):                                 # dead stumps
        c.rect(x, 16 - h, x + 1, 16, 3)
        c.set(x - 1, 16 - h + 1, 3)
        c.set(x + 2, 16 - h + 2, 3)
    return c


def iron_mine():
    """Iron Mine: a hillside with a timber-framed mine entrance (dark inside), rails running out of it to the front,
    a loaded mine cart on them, a hanging lantern."""
    c = C(0)
    c.poly([(0, 18), (0, 6), (8, 2), (20, 1), (32, 5), (32, 18)], 2)        # hillside
    for x, y in ((4, 9), (26, 8), (14, 4), (29, 13), (2, 14)):
        c.rect(x, y, x + 1, y, 3)                                  # rocks in the slope
    c.rect(10, 7, 21, 18, 3)                                       # the dark entrance
    c.rect(9, 6, 22, 7, 1)                                         # timber frame: lintel and posts
    c.rect(9, 6, 22, 6, 3)
    for x in (9, 22):
        c.rect(x, 6, x, 18, 1)
        c.rect(x + (1 if x == 9 else -1), 7, x + (1 if x == 9 else -1), 18, 3)
    c.rect(18, 8, 18, 9, 2)                                        # lantern
    c.rect(17, 10, 19, 11, 0)
    c.set(18, 10, 1)
    c.rect(0, 18, W - 1, H - 1, 1)
    c.rect(0, 18, W - 1, 18, 2)
    c.line(13, 18, 6, 24, 3)                                       # rails out of the mine
    c.line(18, 18, 25, 24, 3)
    for y in (19, 21, 23):
        t = (y - 18) / 6.0
        c.rect(int(13 - 7 * t), y, int(18 + 7 * t), y, 2)
    c.rect(11, 14, 20, 18, 2)                                      # mine cart with ore
    c.rect(11, 14, 20, 14, 0)
    c.rect(11, 14, 11, 18, 0)
    c.rect(20, 14, 20, 18, 0)
    for x in (13, 15, 17):
        c.set(x, 13, 0)
        c.set(x + 1, 12, 1)
    c.rect(12, 19, 13, 19, 3)                                      # wheels
    c.rect(18, 19, 19, 19, 3)
    return c


def lighthouse_coast():
    """Lighthouse Coast: a tall striped lighthouse on dark rocks at the left, its lamp sending beams across the sky,
    the sea to the horizon, waves at the rocks."""
    c = C(0)
    c.rect(0, 13, W - 1, 13, 2)                                    # horizon
    c.rect(0, 14, W - 1, H - 1, 1)
    for y, xs in ((16, (18, 26)), (19, (22,)), (22, (16, 28))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    c.poly([(0, 24), (0, 17), (4, 15), (11, 16), (14, 20), (13, 24)], 3)    # rocks
    c.rect(3, 16, 6, 16, 2)
    for y in range(4, 16):                                         # the tower, a little wider at the base
        half = 2 + (y - 4) // 6
        c.rect(7 - half, y, 7 + half, y, 0)
        c.set(7 - half, y, 3)
        c.set(7 + half, y, 3)
        if (y // 2) % 2 == 0:
            c.rect(8 - half, y, 6 + half, y, 2)                    # stripes
    c.rect(5, 2, 9, 3, 3)                                          # lamp room
    c.rect(6, 2, 8, 2, 0)
    c.rect(6, 0, 8, 1, 3)                                          # roof
    c.set(7, 0, 3)
    for dy in (-2, 0, 2):                                          # light beams
        c.line(10, 2, 31, 2 + dy * 3, 1)
    for x, y in ((14, 18), (12, 21)):                              # spray at the rocks
        c.set(x, y, 0)
    return c


def wild_safari():
    """Wild Safari: open tall grass to the horizon, a flat-topped tree, a rope fence on posts in front, a wooden sign
    reading SAFARI."""
    c = C(0)
    c.disc(27, 3, 2.3, 1)
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, H - 1, 1)
    for x in range(0, W, 2):                                       # tall grass tops
        h = 1 + (x * 5) % 3
        c.rect(x, 11 - h, x, 11, 2)
    c.rect(21, 4, 31, 5, 3)                                        # flat-topped tree
    c.rect(23, 3, 29, 3, 3)
    c.rect(26, 6, 26, 11, 3)
    c.line(26, 7, 23, 5, 3)
    c.rect(1, 3, 23, 9, 3)                                         # sign
    text3(c, 2, 4, "SAFARI", 0)
    c.rect(4, 10, 4, 14, 3)
    c.rect(20, 10, 20, 14, 3)
    for x in range(1, 32, 6):                                      # rope fence in front
        c.rect(x, 17, x, 21, 3)
    for x in range(0, W):
        c.set(x, 18 + (1 if (x % 6) in (2, 3, 4) else 0), 3)
    for x, y in ((3, 23), (12, 22), (25, 23), (18, 15), (9, 14)):
        c.set(x, y, 2)
        c.set(x + 1, y - 1, 2)
    return c


DPPT1 = {"lakefront": lakefront, "deep_woods": deep_woods, "blossom_garden": blossom_garden,
         "night_meadow": night_meadow, "crumbling_crag": crumbling_crag, "rugged_hills": rugged_hills,
         "marsh_wetlands": marsh_wetlands, "iron_mine": iron_mine, "lighthouse_coast": lighthouse_coast,
         "wild_safari": wild_safari}
COURSES.update(DPPT1)


def preview_names(png, names):
    """Like preview(), but only for the given names (so a new set gets its own sheet)."""
    global KINDS
    keep = KINDS
    KINDS = {n: keep[n] for n in names}
    try:
        preview(png)
    finally:
        KINDS = keep


def write(out):
    os.makedirs(out, exist_ok=True)
    for name, fn in KINDS.items():
        c = fn()
        path = os.path.join(out, f"{name}.txt")
        if os.path.exists(path) and open(path).read().startswith("; hand-edited"):
            print("kept (hand-edited)", path)
            continue
        with open(path, "w") as f:
            f.write(f"; {name}: 32x24, . white  - light grey  + dark grey  # black (tools/route_art.py)\n")
            for row in c.p:
                f.write("".join(CH[v] for v in row) + "\n")
        print("->", path)


def load(name, d=OUT):
    rows = [l.rstrip("\n") for l in open(os.path.join(d, f"{name}.txt")) if not l.startswith(";")]
    assert len(rows) == H and all(len(r) == W for r in rows), name
    return [[CH.index(ch) for ch in r] for r in rows]


def preview(png):
    sys.path.insert(0, HERE)
    import pwimg
    k, pad = 8, 6
    lcd = [(196, 206, 168), (140, 152, 116), (82, 92, 66), (30, 36, 24)]
    names = list(KINDS)
    cols = 4
    cw, chh = W * k + pad, H * k + pad + 14
    w, h = cols * cw + pad, ((len(names) + cols - 1) // cols) * chh + pad
    rows = [[(44, 44, 50)] * w for _ in range(h)]
    for i, n in enumerate(names):
        px = load(n)
        ox, oy = pad + (i % cols) * cw, pad + (i // cols) * chh
        for y in range(H):
            for x in range(W):
                for dy in range(k):
                    for dx in range(k):
                        g = (dx == k - 1 or dy == k - 1)
                        c = lcd[px[y][x]]
                        rows[oy + y * k + dy][ox + x * k + dx] = tuple(max(0, v - 18) for v in c) if g else c
    pwimg.write_png(png, w, h, [b"".join(bytes(c) for c in r) for r in rows])
    print("->", png, "(order:", ", ".join(names) + ")")


if __name__ == "__main__":
    write(OUT)
    preview(os.path.join(ROOT, "art_src", "route_pictures.png"))
