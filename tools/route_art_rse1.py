#!/usr/bin/env python3
"""route_art_rse1.py - course pictures for the Ruby / Sapphire / Emerald post-game courses, first half (13).

Same style as route_art.py's DPPT1 / SM1 sets: far (light) to near (dark) layers, one clear subject, places only.
Run: python3 tools/route_art_rse1.py  -> art/routes/<slug>.txt for these 13 only + art_src/route_pictures_rse1.png
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route_art as ra  # noqa: E402

C, W, H = ra.C, ra.W, ra.H


def hill_band(c, f, v, edge=None):
    """Fill below y = f(x) with v; an optional edge shade on the top pixel row."""
    for x in range(W):
        top = int(round(f(x)))
        for y in range(max(0, top), H):
            c.p[y][x] = v
        if edge is not None and 0 <= top < H:
            c.p[top][x] = edge


def rolling_meadow():
    """Rolling Meadow: three bands of smooth round hills (far light, middle mottled, near dark with grass blades),
    a lone round tree on the middle crest, a flock of birds in a V, a cloud."""
    c = C(0)
    ra.cloud(c, 2, 3, 7)
    far = lambda x: 10 + 1.5 * math.sin(x / 3.5 + 0.5)
    hill_band(c, far, 1)                                           # far hills, light
    mid = lambda x: 13.5 + 2.8 * math.sin(x / 6.0 + 2.6)
    hill_band(c, mid, 0, edge=3)                                   # middle hill: sunlit, outlined
    for x in range(W):
        top = int(round(mid(x)))
        if x % 4 == 1:
            c.set(x, top + 2, 1)
        if x % 6 == 3:
            c.set(x, top + 4, 1)
            c.set(x + 1, top + 4, 1)
    near = lambda x: 19 + 2.3 * math.sin(x / 5.0 + 0.9)
    hill_band(c, near, 2, edge=3)                                  # near hill, dark
    for x in range(W):
        top = int(round(near(x)))
        if x % 3 != 2:
            c.set(x, top + 1, 1)                                   # light along its crest
    for x, y in ((3, 23), (10, 22), (18, 23), (25, 22), (29, 23), (14, 23)):
        c.set(x, y, 3)                                             # grass blades
        c.set(x - 1, y - 1, 3)
        c.set(x + 1, y - 1, 3)
    cx = 22                                                        # lone tree on the middle crest
    ground = int(round(mid(cx))) + 1
    ra.tree2(c, cx, ground, 3, 3)
    for x, y in ((9, 7), (14, 5), (19, 3)):                        # birds
        ra.gsc_bird(c, x, y)
    return c


def buzzing_grove():
    """Buzzing Grove: two thick trees whose canopy closes over the top, a striped beehive hanging from a branch,
    three cocoons on silk threads, bugs zipping about on dotted flight trails, light far trees between."""
    c = C(0)
    for x in (9, 15, 21):                                          # far trees, light, low
        c.rect(x, 13, x, 17, 1)
        c.disc(x, 14, 2.5, 1)
    c.rect(0, 18, W - 1, 18, 2)
    c.rect(0, 19, W - 1, H - 1, 1)
    for x in range(W):                                             # the canopy over the top
        bot = 3 + int(round(1.5 * math.sin(x / 2.2))) + (2 if x in range(5, 9) or x in range(24, 28) else 0)
        c.rect(x, 0, x, bot, 2)
        c.set(x, bot, 3)
        if (x * 3) % 7 == 1:
            c.set(x, bot - 2, 1)
    for x0 in (2, 26):                                             # thick trunks
        c.rect(x0, 3, x0 + 3, H - 3, 3)
        c.rect(x0 + 1, 6, x0 + 1, H - 4, 2)
        c.set(x0 - 1, H - 3, 3)
        c.set(x0 + 4, H - 3, 3)
    c.line(6, 7, 15, 6, 3)                                         # branch with the hive
    c.line(6, 8, 10, 7, 3)
    c.rect(11, 7, 11, 7, 3)
    for y, half in ((8, 2), (9, 3), (10, 4), (11, 4), (12, 4), (13, 4), (14, 3), (15, 2)):
        c.rect(11 - half, y, 11 + half, y, 0 if y % 2 else 2)      # striped hive
        c.set(11 - half, y, 3)
        c.set(11 + half, y, 3)
    c.rect(10, 16, 12, 16, 3)
    c.rect(8, 8, 14, 8, 3)
    c.rect(10, 12, 12, 13, 3)                                      # hive hole
    c.set(9, 11, 1)
    for x, top, ln in ((18, 5, 2), (23, 6, 5)):                    # cocoons on silk threads
        c.rect(x, top, x, top + ln, 3)
        y = top + ln + 1
        ra.stamp(c, [".##.",
                     "#--#",
                     "#.-#",
                     "#++#",
                     "#.-#",
                     "#--#",
                     ".##."], x - 1, y)
    for (bx, by), trail in (((16, 11), ((16, 13), (17, 15), (19, 16), (21, 15))),
                            ((20, 19), ((22, 20), (24, 21), (26, 20))),
                            ((12, 20), ((10, 21), (8, 20), (7, 18), (8, 16)))):
        c.rect(bx, by, bx + 1, by, 3)                              # bug
        c.set(bx, by - 1, 1)
        c.set(bx + 1, by - 1, 1)
        for tx, ty in trail:
            c.set(tx, ty, 2)
    for x, y in ((9, 21), (17, 22), (23, 21), (14, 23)):
        c.set(x, y, 2)
        c.set(x + 1, y - 1, 2)
    return c


def old_home_path():
    """Old Home Path: a stone path widening toward you, leading up to a small old cottage with a chimney puffing
    smoke, an old big tree beside it, a mailbox and a low stone wall at the front."""
    c = C(0)
    ra.far_hills(c, 11, 1.0, 4.0, 0.3)
    c.rect(0, 14, W - 1, 14, 2)
    c.rect(0, 15, W - 1, H - 1, 1)
    for y in range(11, 14):                                        # far hills stop at the field line
        for x in range(W):
            if c.p[y][x] == 0:
                pass
    for y in range(14, 18):
        for x in range(W):
            if c.p[y][x] == 0:
                c.p[y][x] = 1
    # cottage
    x0, x1, top = 11, 21, 9
    c.rect(x0, top, x1, 14, 0)
    c.rect(x0, top, x0, 14, 3)
    c.rect(x1, top, x1, 14, 3)
    for i in range(6):                                             # thatched roof
        c.rect(x0 - 1 + i, top - 1 - i, x1 + 1 - i, top - 1 - i, 2)
        c.set(x0 - 1 + i, top - 1 - i, 3)
        c.set(x1 + 1 - i, top - 1 - i, 3)
    c.rect(x0 - 1, top - 1, x1 + 1, top - 1, 3)
    for x in range(x0 + 1, x1, 2):
        c.set(x, top - 2, 1)
    c.rect(19, 2, 20, 5, 3)                                        # chimney + smoke
    for x, y in ((20, 1), (21, 0), (23, 0), (19, 0)):
        c.set(x, y, 1)
    c.rect(22, 1, 23, 1, 1)
    c.rect(15, 11, 17, 14, 3)                                      # door
    c.rect(12, 10, 13, 11, 2)                                      # windows
    c.rect(19, 10, 20, 11, 2)
    c.set(12, 10, 1)
    c.set(19, 10, 1)
    # path widening toward the viewer, with stepping stones
    c.poly([(15, 15), (18, 15), (25, 24), (7, 24)], 0)
    c.line(15, 15, 7, 23, 2)
    c.line(18, 15, 25, 23, 2)
    for y, xs in ((17, (15, 17)), (19, (13, 17)), (21, (11, 15, 20)), (23, (9, 14, 19))):
        for x in xs:
            w = 1 if y < 20 else 2
            c.rect(x, y, x + w, y, 2)
    # big old tree at the left
    ra.tree2(c, 4, 17, 4, 5)
    c.rect(3, 13, 5, 16, 3)
    # mailbox
    c.rect(26, 16, 26, 20, 3)
    c.rect(24, 13, 28, 15, 3)
    c.rect(25, 14, 27, 14, 0)
    c.set(28, 12, 2)
    c.rect(28, 11, 29, 11, 2)
    # low stone wall at the front corners
    for xa, xb in ((0, 6), (26, 31)):
        c.rect(xa, 21, xb, 23, 2)
        c.rect(xa, 21, xb, 21, 3)
        for x in range(xa, xb + 1, 3):
            c.set(x, 22, 3)
        for x in range(xa + 1, xb + 1, 3):
            c.set(x, 23, 3)
    return c


def thorn_line(c, pts, v=3, every=3):
    """A bramble stem through pts with thorns sticking out on alternate sides."""
    k = 0
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        n = int(max(abs(xb - xa), abs(yb - ya))) + 1
        for i in range(n):
            t = i / n
            x, y = xa + (xb - xa) * t, ya + (yb - ya) * t
            c.set(x, y, v)
            if k % (every * 2) == 0:
                c.set(x - 1, y - 1, v)
                c.set(x - 2, y - 2, v)
            elif k % (every * 2) == every:
                c.set(x + 1, y - 1, v)
                c.set(x + 2, y - 2, v)
            k += 1


def thorn_thicket():
    """Thorn Thicket: a hot sun, far light bramble mounds, middle dark bramble bushes bristling with thorns, near
    black thorny stems arching over both sides, leaving a narrow trampled gap with a few berries."""
    c = C(0)
    c.disc(25, 4, 2.6, 1)
    for x, y in ((21, 4), (29, 4), (25, 0), (25, 8)):
        c.set(x, y, 1)
    for cx, r in ((3, 4), (10, 3), (17, 4), (24, 3), (31, 4)):     # far mounds, pale, with spiky tops
        c.disc(cx, 13, r, 1)
        for dx in range(-r + 1, r, 2):
            c.set(cx + dx, 13 - int((r * r - dx * dx) ** 0.5) - 1, 1)
    c.rect(0, 13, W - 1, H - 1, 1)
    # a wall of bramble across the middle, split by a gap where a path goes through
    top = lambda x: 10 + 1.5 * math.sin(x / 1.9) + (0 if abs(x - 16) > 6 else (6 - abs(x - 16)) * 0.9)
    m = {(x, y) for x in range(W) for y in range(H) if top(x) <= y <= 19 and not (13 <= x <= 18 and y >= 14)}
    for (x, y) in m:
        edge = any((x + dx, y + dy) not in m for dx, dy in ((1, 0), (-1, 0), (0, -1)))
        c.set(x, y, 3 if edge else 2)
    for x in range(0, W, 2):                                       # thorns along the top edge
        y = int(math.ceil(top(x))) - 1
        if not (13 <= x <= 18):
            c.set(x, y, 3)
            c.set(x + (1 if x % 4 else -1), y - 1, 3)
    for pts in (((1, 18), (3, 14), (7, 13), (10, 16)), ((5, 12), (9, 15), (12, 14)),
                ((20, 15), (24, 12), (28, 13), (30, 17)), ((23, 18), (26, 15), (30, 14))):
        thorn_line(c, pts, v=3, every=2)                           # thorny stems curling inside the hedge
    for x, y in ((4, 16), (9, 18), (21, 17), (27, 16), (11, 12), (26, 11)):
        c.set(x, y, 1)                                             # pale berries
    c.rect(0, 20, W - 1, H - 1, 1)
    c.rect(0, 20, W - 1, 20, 2)
    c.poly([(14, 19), (18, 19), (21, 24), (11, 24)], 0)            # trampled path through the gap
    c.line(14, 19, 11, 23, 2)
    c.line(18, 19, 21, 23, 2)
    # near brambles in the bottom corners
    c.poly([(0, 18), (5, 19), (9, 22), (10, 24), (0, 24)], 3)
    c.poly([(31, 18), (26, 19), (22, 22), (21, 24), (31, 24)], 3)
    thorn_line(c, ((0, 17), (4, 16), (8, 18), (10, 21)))
    thorn_line(c, ((31, 17), (27, 16), (23, 18), (21, 21)))
    for x, y in ((3, 21), (6, 22), (26, 21), (29, 22), (24, 23)):  # berries
        c.set(x, y, 0)
    return c


def mushroom(c, cx, ground, cap_r, stem_h, spots=True, cap=2):
    """Big mushroom: a dark dome cap with light spots and a black rim, light gills, a white stem outlined black."""
    top = ground - stem_h
    c.rect(cx - 1, top, cx + 1, ground - 1, 0)
    c.rect(cx - 1, top, cx - 1, ground - 1, 3)
    c.rect(cx + 1, top, cx + 1, ground - 1, 3)
    c.set(cx, ground - 2, 1)
    cy = top
    m = {(x, y) for y in range(int(cy - cap_r) - 1, int(cy) + 1) for x in range(int(cx - cap_r - 1), int(cx + cap_r + 2))
         if ((x - cx) / (cap_r + 0.5)) ** 2 + ((y - cy) / (cap_r + 0.3)) ** 2 <= 1}
    for (x, y) in m:
        edge = any((x + dx, y + dy) not in m for dx, dy in ((1, 0), (-1, 0), (0, -1)))
        c.set(x, y, 3 if (edge and cap != 2) else cap)
    for x in range(int(cx - cap_r), int(cx + cap_r) + 1):
        c.set(x, cy, 3)
    c.rect(int(cx - cap_r) + 1, cy + 1, int(cx + cap_r) - 1, cy + 1, 1)    # gills
    c.rect(cx - 1, cy + 1, cx + 1, cy + 1, 3)
    if spots:
        for dx, dy in ((-2, -2), (1, -3), (3, -1), (-cap_r + 1, -1)):
            if abs(dx) < cap_r and dy > -cap_r:
                c.set(cx + dx, cy + dy, 2 if cap == 0 else 0)


def spore_marsh():
    """Spore Marsh: hazy air full of drifting spores, a huge spotted mushroom on the left and a cluster of smaller
    ones on a hummock to the right, dark damp ground with still pools reflecting light."""
    c = C(0)
    for x0, y0, x1 in ((2, 4, 9), (13, 2, 20), (12, 9, 18)):       # drifting mist bands
        c.rect(x0, y0, x1, y0, 1)
    for x in range(W):                                             # far misty trees
        top = 9 + int(round(1.2 * math.sin(x / 1.6))) + (x % 5 == 0)
        for y in range(top, 15):
            c.p[y][x] = 1 if (x + y) % 2 == 0 or y > 12 else 0
    c.rect(0, 15, W - 1, H - 1, 2)
    c.rect(0, 15, W - 1, 15, 3)
    for x0, y0, x1 in ((13, 18, 23), (1, 22, 8), (18, 22, 29)):    # pools with a glint
        c.rect(x0, y0, x1, y0 + 1, 1)
        c.rect(x0 + 1, y0, x0 + 3, y0, 0)
        c.rect(x0, y0 - 1, x1, y0 - 1, 3)
    c.poly([(19, 17), (22, 14), (31, 13), (32, 13), (32, 18), (19, 18)], 2)    # hummock on the right
    c.line(19, 17, 22, 14, 3)
    c.line(22, 14, 31, 13, 3)
    mushroom(c, 9, 21, 6, 10)                                      # the giant
    mushroom(c, 25, 14, 3, 5, cap=1)
    mushroom(c, 30, 14, 2, 3, spots=False, cap=1)
    mushroom(c, 20, 18, 2, 3, spots=False, cap=0)
    for x, y in ((15, 2), (19, 5), (23, 3), (27, 7), (17, 9), (3, 3), (13, 7), (30, 2), (21, 10), (26, 11),
                 (6, 1)):                                          # spores
        c.set(x, y, 2)
    for x, y in ((16, 4), (25, 5), (12, 4), (29, 9)):
        c.set(x, y, 1)
        c.set(x + 1, y, 1)
        c.set(x, y + 1, 1)
        c.set(x + 1, y + 1, 1)
    return c


def vine_garden():
    """Vine Garden: a wooden pergola across the top with leafy vines hanging down at different lengths, bell
    flowers at their tips, puffs of fluff floating through, a far hedge and garden beds below."""
    c = C(0)
    for x in range(W):                                             # clipped hedge behind, rounded top
        c.rect(x, 10 + (1 if x % 4 in (0, 3) else 0), x, 15, 1)
    c.rect(0, 15, W - 1, 15, 2)
    c.rect(0, 16, W - 1, H - 1, 1)
    c.poly([(13, 16), (19, 16), (23, 24), (9, 24)], 0)             # garden path
    c.line(13, 16, 9, 23, 2)
    c.line(19, 16, 23, 23, 2)
    for x, y in ((4, 19), (27, 19), (2, 22), (29, 22), (6, 22), (25, 22)):   # low shrubs
        c.rect(x - 1, y, x + 1, y, 2)
        c.set(x, y - 1, 2)
        c.set(x + 1, y, 3)
    # pergola: posts at the sides, beam and rafters across the top
    for x in (1, 29):
        c.rect(x, 1, x + 1, H - 2, 3)
        c.rect(x + 1, 4, x + 1, H - 3, 2)
    c.rect(0, 1, W - 1, 2, 3)
    c.rect(1, 1, W - 2, 1, 2)
    for x in range(2, W - 1, 4):
        c.rect(x, 0, x + 1, 0, 3)
    # hanging vines: a stem swaying down, a leaf every few pixels, a bell flower at the tip
    sway = lambda i, ph: int(round(0.7 * math.sin((i + ph) / 2.5)))
    for x, ln, ph in ((5, 9, 0), (11, 13, 2), (19, 7, 1), (25, 11, 3)):
        for i in range(ln):
            c.set(x + sway(i, ph), 3 + i, 3)
        for n, i in enumerate(range(1 + ph % 2, ln - 1, 3)):
            y, xx = 3 + i, x + sway(i, ph)
            s = 1 if n % 2 else -1
            c.set(xx + s, y, 2)                                    # leaf: two dark pixels and a black tip
            c.set(xx + 2 * s, y, 2)
            c.set(xx + 2 * s, y - 1, 2)
            c.set(xx + 3 * s, y - 1, 3)
        bx, y = x + sway(ln, ph), 3 + ln
        ra.stamp(c, [".#.",                                       # bell flower at the tip
                     "###",
                     "#.#",
                     "#.#",
                     "#.#"], bx - 1, y)
        c.rect(bx, y + 2, bx, y + 4, 0)
        c.set(bx - 2, y + 4, 3)
        c.set(bx + 2, y + 4, 3)
    for cx, cy in ((15, 8), (22, 3), (29, 17), (8, 19)):            # floating fluff puffs
        c.disc(cx, cy, 1.6, 3)
        c.rect(cx - 1, cy - 1, cx + 1, cy + 1, 0)
        c.set(cx + 1, cy + 1, 1)
        c.set(cx - 2, cy - 2, 0)
        c.set(cx + 2, cy - 2, 0)
    return c


def tangled_grove():
    """Tangled Grove: a dense dark jungle wall behind, two leaning coconut palms, thick vines looping and sagging
    between the trunks, big jungle leaves at the bottom corners, a huge spotted flower in the middle."""
    c = C(0)
    for x in range(W):                                             # jungle wall behind, bumpy top
        top = 9 + int(round(1.4 * math.sin(x / 1.5) + math.sin(x / 3.7)))
        c.rect(x, top, x, 17, 1)
    c.dither(0, 13, W - 1, 17, 1, 2, 0.35)
    c.rect(0, 18, W - 1, H - 1, 2)
    c.rect(0, 18, W - 1, 18, 3)
    ra.palm(c, 5, 19, 16, lean=1)
    ra.palm(c, 26, 19, 14, lean=-1)
    for x, y in ((7, 4), (9, 4), (8, 5), (23, 6), (25, 6), (24, 7)):   # coconut bunches
        c.set(x, y, 3)
    arcs = (((6, 5), (25, 7), 3), ((7, 9), (24, 11), 4))
    for n, ((xa, ya), (xb, yb), sag) in enumerate(arcs):           # vines draped between the palms
        pts = []
        for k in range(41):
            t = k / 40.0
            x = xa + (xb - xa) * t
            y = ya + (yb - ya) * t + sag * 4 * t * (1 - t)
            c.set(x, y, 3)
            pts.append((x, y))
            if k % 6 == 3:                                         # leaves along the vine
                c.set(x, y - 1, 2)
                c.set(x + 1, y - 1, 2)
        for k, ln in ((10 + n * 4, 3 + n), (24 - n * 3, 2 + 2 * (n % 2)), (32, 2)):
            x, y = pts[k]                                          # tendrils hanging off, curled at the end
            c.rect(int(round(x)), int(round(y)), int(round(x)), int(round(y)) + ln, 3)
            c.set(round(x) + 1, round(y) + ln + 1, 3)
            c.set(round(x) + 2, round(y) + ln, 3)
    for k in range(14):                                            # a vine spiralling up the left palm
        y = 18 - k
        c.set(6 + (k * 1.0 / 14) ** 2 * 2 + (1 if k % 4 < 2 else -1), y, 3)
    # big flower on a stalk, seen from the side: a wide flat bloom with spots
    cx, d = 16, 3                                                  # d: shifted down to the ground
    c.rect(cx, 19 + d, cx, 23, 3)
    c.rect(cx - 3, 22, cx - 1, 22, 1)
    c.rect(cx + 1, 23, cx + 3, 23, 1)
    c.rect(cx - 4, 16 + d, cx + 4, 18 + d, 0)
    c.rect(cx - 4, 18 + d, cx + 4, 18 + d, 3)
    c.set(cx - 5, 17 + d, 3)
    c.set(cx + 5, 17 + d, 3)
    c.rect(cx - 3, 15 + d, cx + 3, 15 + d, 3)
    c.set(cx - 4, 16 + d, 3)
    c.set(cx + 4, 16 + d, 3)
    for x in (cx - 2, cx + 2):
        c.set(x, 17 + d, 2)
    c.rect(cx - 1, 16 + d, cx + 1, 16 + d, 1)
    # big leaves at the corners
    for sx, x0 in ((1, 0), (-1, 31)):
        c.poly([(x0, 24), (x0, 17), (x0 + sx * 5, 19), (x0 + sx * 9, 24)], 3)
        c.line(x0, 24, x0 + sx * 5, 19, 2)
        c.line(x0 + sx * 2, 21, x0 + sx * 3, 23, 2)
    return c


def mole_hole(c, x, y, w):
    """A dirt mound with a black hole in it."""
    c.rect(x - w, y, x + w, y, 2)
    c.rect(x - w + 1, y - 1, x + w - 1, y - 1, 2)
    c.rect(x - w + 2, y, x + w - 2, y, 3)
    if w >= 3:
        c.rect(x - w + 2, y - 2, x + w - 2, y - 2, 2)
        c.rect(x - w + 3, y - 1, x + w - 3, y - 1, 3)
    c.set(x - w + 1, y - 1, 1)


def rumbling_canyon():
    """Rumbling Canyon: two steep striped canyon walls closing toward a narrow far end, a thin strip of sky above,
    a dusty floor dotted with mole holes, boulders and falling pebbles."""
    c = C(0)
    c.rect(0, 0, W - 1, H - 1, 1)
    c.rect(12, 0, 20, 12, 0)                                       # sky between the rims
    c.poly([(13, 13), (19, 13), (32, 24), (0, 24)], 1)             # the floor
    left = [(0, 0), (12, 0), (11, 4), (13, 8), (12, 11), (14, 13), (0, 24)]
    right = [(32, 0), (20, 0), (21, 3), (19, 7), (20, 10), (18, 13), (32, 24)]
    c.poly(left, 2)
    c.poly(right, 2)
    c.rect(13, 12, 19, 13, 1)                                      # far end, light
    c.poly([(13, 13), (15, 10), (17, 11), (19, 13)], 1)
    for pts in (left, right):
        for a, b in zip(pts[1:-1], pts[2:-1]):
            c.line(a[0], a[1], b[0], b[1], 3)
    c.line(14, 13, 0, 24, 3)
    c.line(18, 13, 31, 24, 3)
    for y in range(H):                                             # rock strata: wide bands, sloping down
        for x in range(W):                                         # toward you, a dark line on each band top
            if c.get(x, y) != 2:
                continue
            yy = y - int(abs(x - 16) * 0.35)
            row = yy % 6
            if row == 0:
                c.set(x, y, 3)                                     # dark seam
            elif row == 1 and (x * 5 + y) % 7 != 0:
                c.set(x, y, 1)                                     # lit ledge under it
    for y in range(14, 24):                                        # floor dust
        for x in range(W):
            if c.get(x, y) == 1 and (x * 7 + y * 3) % 11 == 0:
                c.set(x, y, 0)
    mole_hole(c, 16, 15, 1)
    mole_hole(c, 13, 18, 2)
    mole_hole(c, 20, 19, 2)
    mole_hole(c, 15, 23, 4)
    ra.boulder(c, 6, 23, 2)
    ra.boulder(c, 26, 23, 3)
    for x, y in ((9, 6), (22, 8), (10, 12)):                       # falling pebbles
        c.set(x, y, 3)
        c.set(x, y - 2, 1)
    return c


def wanderers_prairie():
    """Wanderer's Prairie: a huge sky over a low flat horizon, a far farm windmill with its wheel, tall prairie grass,
    a long post-and-wire fence running away from you in perspective, a tall sunflower by the near post."""
    c = C(0)
    ra.cloud(c, 1, 3, 7)
    ra.cloud(c, 11, 6, 8)
    ra.cloud(c, 27, 9, 5)
    c.rect(0, 13, W - 1, 13, 2)
    c.rect(0, 14, W - 1, H - 1, 1)
    for x in range(0, W, 2):                                       # far grass line
        c.set(x, 12, 1)
    # windmill at the right in the distance
    wx = 22
    c.line(wx - 2, 13, wx, 6, 3)
    c.line(wx + 2, 13, wx, 6, 3)
    c.line(wx - 1, 9, wx + 1, 9, 3)
    c.line(wx - 1.5, 11, wx + 1.5, 11, 3)
    c.disc(wx, 5, 2.6, 1)
    for a in range(0, 360, 45):
        c.line(wx, 5, wx + 2.6 * math.cos(math.radians(a)), 5 + 2.6 * math.sin(math.radians(a)), 3)
    c.set(wx, 5, 2)
    c.rect(wx + 1, 4, wx + 4, 4, 3)                                # tail vane
    c.rect(wx + 3, 3, wx + 4, 5, 2)
    for x, y in ((3, 16), (9, 15), (14, 17), (21, 16), (27, 15), (6, 19), (18, 20), (25, 19)):
        c.set(x, y, 2)                                             # grass tufts, bigger nearer
        c.set(x - 1, y - (1 if y > 17 else 0), 2)
        c.set(x + 1, y - (1 if y > 17 else 0), 2)
    # rail fence running away from the near right toward the far left
    posts = []
    for k in range(8):
        s = (1 - k / 7.0) ** 1.5                                   # perspective scale, 1 = nearest
        posts.append((4 + 27 * s, 14 + 8.5 * s, 1 + 6.5 * s))
    for frac in (0.3, 0.7):                                        # two rails
        for (xa, ya, ha), (xb, yb, hb) in zip(posts, posts[1:]):
            c.line(xa, ya - ha * frac, xb, yb - hb * frac, 3 if ha > 3 else 2)
    for x, y, h in posts:
        x, y, h = int(round(x)), int(round(y)), int(round(h))
        c.rect(x, y - h, x, y, 3)
        if h >= 4:
            c.rect(x - 1, y - h, x - 1, y, 2)
            c.set(x - 1, y - h, 3)
    # sunflower in the near left
    c.rect(5, 14, 5, 23, 3)
    c.rect(3, 19, 4, 19, 2)
    c.rect(6, 17, 7, 17, 2)
    c.set(3, 18, 2)
    c.set(7, 16, 2)
    for a in range(0, 360, 30):
        c.set(5 + 2.7 * math.cos(math.radians(a)), 11 + 2.7 * math.sin(math.radians(a)), 2)
    c.disc(5, 11, 1.8, 3)
    c.set(4, 10, 2)
    for x, y in ((1, 22), (9, 23), (13, 21), (18, 23), (23, 23), (11, 18)):   # near grass tufts
        c.rect(x, y - 2, x, y, 2)
        c.set(x - 1, y - 1, 2)
        c.set(x + 1, y - 1, 2)
    return c


def dojo_hill():
    """Dojo Hill: a steep hill with a long stone stairway climbing straight up to a small dojo with a curved roof
    on top, a gate at the foot of the stairs, wind-bent pines on the slopes, far mountains."""
    c = C(0)
    c.poly([(0, 12), (5, 7), (9, 10), (24, 9), (28, 5), (32, 9), (32, 14), (0, 14)], 1)   # far mountains
    c.poly([(0, 24), (0, 17), (8, 12), (13, 8), (19, 8), (24, 12), (32, 17), (32, 24)], 2)   # the hill
    c.line(0, 17, 8, 12, 3)
    c.line(8, 12, 13, 8, 3)
    c.line(19, 8, 24, 12, 3)
    c.line(24, 12, 32, 17, 3)
    # dojo on top
    c.rect(13, 5, 19, 8, 0)
    c.rect(13, 5, 13, 8, 3)
    c.rect(19, 5, 19, 8, 3)
    c.rect(15, 6, 17, 8, 3)
    c.rect(11, 3, 21, 4, 3)                                        # curved roof
    c.rect(12, 2, 20, 2, 3)
    c.rect(13, 3, 19, 3, 2)
    c.set(10, 2, 3)
    c.set(22, 2, 3)
    c.rect(14, 1, 18, 1, 3)
    # stairs: narrow at the top, wide at the bottom
    for y in range(9, 24):
        t = (y - 9) / 14.0
        x0, x1 = 15 - 4 * t, 17 + 4 * t
        c.rect(int(round(x0)), y, int(round(x1)), y, 0 if (y % 2 or t < 0.25) else 1)
        c.set(int(round(x0)) - 1, y, 3)
        c.set(int(round(x1)) + 1, y, 3)
    for y in range(10, 24, 2):
        t = (y - 9) / 14.0
        c.rect(int(round(15 - 4 * t)), y, int(round(17 + 4 * t)), y, 2)
    # stone lanterns either side of the foot of the stairs
    for x in (8, 24):
        c.rect(x - 1, 23, x + 1, 23, 3)
        c.rect(x, 20, x, 22, 3)
        c.rect(x - 1, 18, x + 1, 19, 0)
        c.rect(x - 1, 18, x - 1, 19, 3)
        c.rect(x + 1, 18, x + 1, 19, 3)
        c.set(x, 19, 1)
        c.rect(x - 2, 17, x + 2, 17, 3)
        c.set(x, 16, 3)
    # wind-bent pines on the slopes (flat layered crowns)
    for x, g in ((3, 19), (28, 19)):
        c.rect(x, g - 4, x, g, 3)
        c.rect(x - 2, g - 5, x + 2, g - 5, 3)
        c.rect(x - 1, g - 6, x + 3, g - 6, 2)
        c.rect(x - 3, g - 3, x + 1, g - 3, 2)
        c.rect(x - 2, g - 2, x, g - 2, 3)
    for x, y in ((5, 22), (27, 22), (2, 23), (30, 23)):
        c.set(x, y, 3)
    return c


def ember_ranch():
    """Ember Ranch: seen through a ranch gate with a horseshoe on its crossbar - a big hot sun with rays, a corral
    fence and a stable behind, smoke rising from a campfire burning in the yard, hoofprints in the dirt."""
    c = C(1)                                                       # hot hazy sky
    sx, sy = 9, 7
    c.disc(sx, sy, 3.3, 0)                                         # big white sun
    for a in range(0, 360, 45):
        ang = math.radians(a)
        c.line(sx + 4.5 * math.cos(ang), sy + 4.5 * math.sin(ang), sx + 5.5 * math.cos(ang), sy + 5.5 * math.sin(ang), 0)
    for x in (sx - 2, sx + 1):                                     # heat shimmer below it
        c.set(x, 11, 0)
        c.set(x + 1, 12, 0)
    c.rect(0, 13, W - 1, 13, 2)
    c.rect(0, 14, W - 1, H - 1, 0)                                 # sun-baked dusty yard
    for x, y in ((3, 16), (25, 15), (14, 23), (28, 22), (8, 15)):
        c.rect(x, y, x + 1, y, 1)
    # stable at the right
    c.rect(22, 10, 29, 13, 2)
    c.rect(22, 10, 29, 10, 3)
    c.poly([(21, 10.5), (25.5, 7.5), (30, 10.5)], 3)
    c.rect(24, 11, 26, 13, 3)
    c.rect(24, 11, 26, 11, 1)
    # corral fence across the back
    for y in (11, 13):
        c.rect(0, y, 21, y, 3)
    for x in range(1, 22, 4):
        c.rect(x, 10, x, 13, 3)
    # campfire with rising smoke
    fx, fy = 16, 22
    stamp_fire = ["....#....",
                  "...#+#...",
                  ".#.#+#.#.",
                  "#+##-##+#",
                  "#+#---#+#",
                  "#+-----+#",
                  ".#+---+#.",
                  "..#####..",
                  "#+#+#+#+#",
                  ".#.....#."]
    ra.stamp(c, stamp_fire, fx - 4, fy - 8)
    for x, y in ((fx, fy - 3), (fx - 1, fy - 2), (fx + 1, fy - 2), (fx, fy - 2)):
        c.set(x, y, 0)                                             # white-hot core
    for (px, py, r) in ((fx + 1, fy - 10, 1.3), (fx + 3, fy - 13, 1.7), (fx + 2, fy - 17, 2.1)):
        c.disc(px, py, r + 0.8, 3)                                 # smoke puffs, growing as they rise
        c.disc(px, py, r, 2)
        c.set(px - 1, py - 1, 1)
    for x, y in ((5, 16), (8, 18), (5, 20), (8, 22), (24, 18), (26, 21)):   # hoofprints
        c.rect(x, y, x + 1, y, 2)
        c.set(x, y - 1, 2)
    # the gate frame in front
    for x in (1, 29):
        c.rect(x, 2, x + 1, H - 1, 3)
        c.rect(x + 1, 3, x + 1, H - 1, 2)
    c.rect(0, 1, W - 1, 3, 3)
    c.rect(1, 2, 30, 2, 2)
    # horseshoe hanging from the crossbar
    hx = 25
    c.set(hx, 4, 3)
    c.rect(hx - 2, 5, hx + 2, 5, 3)
    c.rect(hx - 3, 6, hx - 3, 7, 3)
    c.rect(hx + 3, 6, hx + 3, 7, 3)
    c.set(hx - 2, 6, 2)
    c.set(hx + 2, 6, 2)
    c.set(hx, 5, 2)
    return c


def bolt(c, pts, core=0, edge=3):
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        c.line(xa + 1, ya, xb + 1, yb, edge)
        c.line(xa - 1, ya, xb - 1, yb, edge)
    for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
        c.line(xa, ya, xb, yb, core)


def static_works():
    """Static Works: inside a power lab - a wall of computer cabinets with screens and blinking lights on the left,
    two tall coil towers on the right with a jagged spark arcing between them, thick cables over a tiled floor."""
    c = C(1)                                                       # back wall
    c.rect(0, 0, W - 1, 0, 3)                                      # ceiling with strip lights
    for x in range(1, W, 8):
        c.rect(x, 1, x + 4, 1, 0)
    # floor tiles in perspective
    c.rect(0, 16, W - 1, H - 1, 0)
    c.rect(0, 16, W - 1, 16, 3)
    for y in (18, 21):
        c.rect(0, y, W - 1, y, 1)
    for k in range(-2, 8):
        c.line(16 + (k - 2.5) * 3, 17, 16 + (k - 2.5) * 8, 23, 1)
    # big computer console on the left: a wide screen showing a wave, rows of buttons below
    c.rect(1, 3, 13, 15, 2)
    c.rect(1, 3, 13, 3, 3)
    c.rect(1, 3, 1, 15, 3)
    c.rect(13, 3, 13, 15, 3)
    c.rect(2, 4, 12, 9, 3)
    c.rect(3, 5, 11, 8, 0)                                         # screen
    for x in range(3, 12):
        c.set(x, 6 + round(1.2 * math.sin(x * 1.3)), 3)            # the wave on screen
    for y in (11, 13):
        for x in range(3, 12, 2):
            c.set(x, y, 0 if (x + y) % 3 else 3)                   # buttons and lamps
    c.rect(4, 14, 10, 14, 1)
    # tall tesla coil on the right: ringed column on a base, a big ball on top throwing sparks
    cx = 24
    c.rect(cx - 3, 13, cx + 3, 15, 3)
    c.rect(cx - 2, 13, cx + 2, 13, 2)
    c.rect(cx - 1, 7, cx + 1, 12, 2)
    for y in range(7, 13, 2):
        c.rect(cx - 2, y, cx + 2, y, 3)                            # coil rings
    c.disc(cx, 5, 3.2, 3)
    c.disc(cx, 5, 2.2, 0)
    c.set(cx + 1, 6, 1)
    c.set(cx + 2, 5, 1)
    for pts in (((cx - 4, 4), (19, 2), (18, 6), (15, 4)),          # sparks jumping off the ball
                ((cx + 4, 3), (28, 1), (29, 5), (31, 3)),
                ((cx + 3, 8), (27, 10), (29, 9), (31, 11))):
        for (xa, ya), (xb, yb) in zip(pts, pts[1:]):
            c.line(xa, ya, xb, yb, 3)
        c.set(pts[-1][0], pts[-1][1], 0)
    # a cable over the floor from the console to the coil
    for (xa, ya), (xb, yb) in zip(((8, 16), (10, 20), (19, 20)), ((10, 20), (19, 20), (22, 16))):
        c.line(xa, ya, xb, yb, 3)
        c.line(xa, ya + 1, xb, yb + 1, 2)
    return c


def twilight_woods():
    """Twilight Woods: a dusk sky fading from dark at the top to light at the horizon with the sun half set, black
    tree silhouettes framing it, an owl with glowing eyes on a branch, a spider web strung between two trees."""
    c = C(0)
    c.rect(0, 0, W - 1, 6, 2)                                      # dusk sky, dark at the top
    c.dither(0, 7, W - 1, 9, 1, 2, 0.4)
    c.rect(0, 10, W - 1, 12, 1)
    c.dither(0, 13, W - 1, 16, 0, 1, 0.5)
    c.disc(16, 16, 4.2, 0)                                         # sun half set
    for x, y in ((7, 1), (13, 3), (17, 1), (11, 6)):
        c.set(x, y, 0)
    for x in range(W):                                             # far tree line
        top = 16 - (x * 5 % 4 == 0) - ((x * 3) % 5 == 1)
        c.rect(x, top, x, 17, 2)
    c.rect(0, 18, W - 1, H - 1, 3)
    # big framing trees
    c.rect(0, 0, 3, H - 1, 3)
    c.rect(4, 19, 5, 20, 3)
    c.line(3, 9, 12, 7, 3)                                         # owl's branch
    c.line(3, 10, 11, 8, 3)
    c.line(9, 8, 12, 5, 3)
    c.rect(28, 0, 31, H - 1, 3)
    c.line(28, 1, 20, 0, 3)                                        # branches framing the web
    c.line(28, 11, 21, 9, 3)
    c.line(21, 9, 19, 10, 3)
    c.set(26, 19, 3)
    # owl perched on the branch
    c.rect(6, 3, 9, 7, 3)
    c.set(5, 5, 3)
    c.set(6, 2, 3)
    c.set(9, 2, 3)
    c.set(7, 4, 0)
    c.set(8, 4, 0)
    c.set(7, 8, 3)
    c.set(8, 8, 3)
    # spider web strung between the branches and the trunk
    hub = (24, 5)
    for ex, ey in ((21, 0), (26, 0), (27, 3), (27, 8), (23, 10), (20, 6)):
        c.line(hub[0], hub[1], ex, ey, 0)
    for r in (1.5, 3.0):
        for a in range(0, 360, 18):
            x, y = hub[0] + r * math.cos(math.radians(a)), hub[1] + r * math.sin(math.radians(a))
            if c.get(int(round(x)), int(round(y))) not in (3,):
                c.set(x, y, 0)
    c.set(24, 5, 3)
    c.set(24, 6, 3)
    c.poly([(14, 18), (18, 18), (22, 24), (10, 24)], 2)            # a dim path toward the sunset
    for x, y in ((6, 13), (21, 12), (9, 20), (25, 21), (13, 11)):  # fireflies
        c.set(x, y, 0)
    # grass blades against the light
    for x in range(4, 28, 2):
        c.rect(x, 17 - (x * 7) % 3, x, 17, 3)
    return c


RSE1 = {"rolling_meadow": rolling_meadow, "buzzing_grove": buzzing_grove, "old_home_path": old_home_path,
        "thorn_thicket": thorn_thicket, "spore_marsh": spore_marsh, "vine_garden": vine_garden,
        "tangled_grove": tangled_grove, "rumbling_canyon": rumbling_canyon, "wanderers_prairie": wanderers_prairie,
        "dojo_hill": dojo_hill, "ember_ranch": ember_ranch, "static_works": static_works,
        "twilight_woods": twilight_woods}
ra.COURSES.update(RSE1)


def write_rse1():
    os.makedirs(ra.OUT, exist_ok=True)
    for name, fn in RSE1.items():
        c = fn()
        path = os.path.join(ra.OUT, f"{name}.txt")
        with open(path, "w") as f:
            f.write(f"; {name}: 32x24, . white  - light grey  + dark grey  # black (tools/route_art_rse1.py)\n")
            for row in c.p:
                f.write("".join(ra.CH[v] for v in row) + "\n")
        print("->", path)


if __name__ == "__main__":
    write_rse1()
    ra.preview_names(os.path.join(ra.ROOT, "art_src", "route_pictures_rse1.png"), list(RSE1))
