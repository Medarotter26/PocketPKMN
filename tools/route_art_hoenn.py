#!/usr/bin/env python3
"""route_art_hoenn.py - course pictures for the Hoenn courses shared by Omega Ruby / Alpha Sapphire and
Ruby / Sapphire / Emerald (15 pictures). Same style as tools/route_art.py's DPPT1 and SM1 sets: 32x24, the walker's
4 shades, far (light) to near (dark) layers, one clear subject, places only (no creatures).

Running it writes art/routes/<slug>.txt for these 15 slugs only and renders art_src/route_pictures_hoenn.png.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route_art as ra  # noqa: E402

C, W, H = ra.C, ra.W, ra.H

ra.FONT3.update({"V": ["#.#", "#.#", "#.#", "#.#", ".#."], "N": ["#.#", "###", "###", "###", "#.#"],
                 "K": ["#.#", "#.#", "##.", "#.#", "#.#"], "B": ["##.", "#.#", "##.", "#.#", "##."],
                 "U": ["#.#", "#.#", "#.#", "#.#", "###"], "Y": ["#.#", "#.#", ".#.", ".#.", ".#."]})


def arch(c, cx, top, base, hw, v, rv=None):
    """Filled arch: straight sides from `base` up, a half-ellipse top (half-width hw, height rv) reaching `top`."""
    rv = hw if rv is None else rv
    cy = top + rv
    for y in range(max(0, top), min(H, base + 1)):
        for x in range(W):
            dx = (x + 0.5 - (cx + 0.5)) / (hw + 0.5)
            if abs(dx) > 1:
                continue
            if y >= cy or (dx * dx + ((y + 0.5 - cy) / rv) ** 2) <= 1:
                c.p[y][x] = v


def sparkle(c, x, y, v=0, arm=1):
    c.set(x, y, v)
    for k in range(1, arm + 1):
        for dx, dy in ((k, 0), (-k, 0), (0, k), (0, -k)):
            c.set(x + dx, y + dy, v)


def gull(c, x, y):
    """Small flying gull: a flat 'm' of black with grey wing tips."""
    c.set(x - 2, y, 2)
    c.set(x - 1, y - 1, 3)
    c.set(x, y, 3)
    c.set(x + 1, y - 1, 3)
    c.set(x + 2, y, 2)


# ---------------------------------------------------------------------------------------------------------------

def dim_tunnel():
    """Dim Tunnel: looking down a long rough tunnel bored through rock - rings of rock getting lighter toward a
    small bright exit, the floor converging on it, rubble at the sides, a few glinting gems in the walls."""
    c = C(3)
    arch(c, 15.5, 1, 23, 14, 2, rv=9)                              # nearest ring (dark rock)
    arch(c, 15.5, 5, 20, 10, 3, rv=7)                              # a rib of black rock
    arch(c, 15.5, 6, 20, 9, 2, rv=6)
    arch(c, 15.5, 9, 18, 6, 1, rv=4)                               # further in: lighter
    arch(c, 15.5, 10, 18, 5, 3, rv=3)
    arch(c, 15.5, 11, 17, 4, 1, rv=3)
    arch(c, 15.5, 12, 17, 2, 0, rv=2)                              # the bright exit
    c.poly([(0, 24), (32, 24), (19, 17), (13, 17)], 1)             # floor converging on the exit
    c.poly([(5, 24), (27, 24), (17.5, 17), (14.5, 17)], 2)
    c.poly([(12, 24), (20, 24), (16.5, 17), (15.5, 17)], 1)        # worn middle of the floor
    c.line(0, 23, 13, 17, 3)
    c.line(31, 23, 19, 17, 3)
    for pts in (((3, 4), (5, 8), (4, 12)), ((27, 3), (26, 7), (28, 11)), ((9, 3), (11, 5))):
        for a, b in zip(pts, pts[1:]):                             # cracks in the near rock
            c.line(a[0], a[1], b[0], b[1], 3)
    for x, y in ((6, 15), (24, 5), (21, 3), (8, 9), (29, 15), (2, 18)):
        c.rect(x, y, x + 1, y, 1)                                  # lit rock bumps
    for x, y in ((5, 6), (26, 14), (11, 2)):
        sparkle(c, x, y, 0)                                        # gems in the walls
    for x, y, w in ((1, 22, 3), (26, 21, 3), (8, 20, 1), (22, 19, 1)):
        c.rect(x, y, x + w, y + 1, 3)                              # rubble
        c.set(x, y, 2)
    return c


def beachside():
    """Beachside: a bright sun with rays, the sea to the horizon with rolling surf, a wide sandy beach curving
    away, a striped beach umbrella over a towel, a tall palm on the left, gulls, a shell in the sand."""
    c = C(0)
    c.disc(28, 2, 2.2, 1)                                          # sun with rays
    for dx, dy in ((-4, 0), (4, 0), (0, 4), (-3, 3), (3, 3), (-3, -3), (3, -3)):
        c.set(28 + dx, 2 + dy, 1)
    c.rect(0, 9, W - 1, 9, 2)                                      # horizon
    c.rect(0, 10, W - 1, 15, 1)                                    # sea
    for y, xs in ((11, (14, 22)), (13, (17, 26))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    c.poly([(0, 14), (10, 15), (20, 16), (32, 17), (32, 24), (0, 24)], 0)   # the beach
    for x in range(0, W):                                          # wet sand edge + surf foam scallops
        y = int(14 + (x / 32.0) * 3)
        c.set(x, y, 2)
        if x % 4 == 1:
            c.rect(x, y - 1, x + 1, y - 1, 0)
    for x in range(W):                                             # wet sand band just below the foam line
        y = int(14 + (x / 32.0) * 3)
        c.set(x, y + 1, 1)
        if x % 2:
            c.set(x, y + 2, 1)
    for x, y in ((3, 19), (9, 21), (14, 19), (28, 21), (25, 23), (5, 23), (17, 22), (30, 19)):
        c.set(x, y, 1)                                             # sand grains
    # umbrella over a towel
    cx, top = 24, 7
    for y in range(top, top + 5):                                  # dome canopy with wedge stripes
        for x in range(cx - 7, cx + 8):
            dx, dy = (x - cx) / 7.5, (y - (top + 4.6)) / 4.6
            if dx * dx + dy * dy <= 1:
                u = dx / math.sqrt(max(0.05, 1 - dy * dy))        # -1..1 across the row: panels bulge
                c.set(x, y, 2 if int((u + 1) * 2.5 - 1e-6) % 2 == 0 else 0)
    for y in range(top, top + 5):                                  # black outline of the dome
        for x in range(cx - 8, cx + 9):
            dx, dy = (x - cx) / 7.5, (y - (top + 4.6)) / 4.6
            if dx * dx + dy * dy > 1 and any(((x + a - cx) / 7.5) ** 2 + ((y + b - (top + 4.6)) / 4.6) ** 2 <= 1
                                             for a, b in ((1, 0), (-1, 0), (0, 1))):
                c.set(x, y, 3)
    for x in range(cx - 7, cx + 8):                                # scalloped hem
        c.set(x, top + 5, 3 if x % 3 != 1 else 0)
    c.set(cx, top - 1, 3)
    c.rect(cx, top + 5, cx, 21, 3)                                 # pole
    c.poly([(17, 20), (28, 20), (30, 23), (15, 23)], 2)            # striped towel
    for x in (19, 23, 27):
        c.line(x - 1, 20, x, 23, 1)
    c.rect(cx, 19, cx, 22, 3)
    ra.palm(c, 4, 21, 16, lean=-1)
    c.poly([(5, 21), (7, 19), (9, 21), (7, 22)], 2)                # a shell
    c.set(7, 20, 1)
    c.set(30, 21, 2)
    c.set(29, 22, 2)
    gull(c, 14, 2)
    gull(c, 19, 4)
    return c


def pylon(c, x, top, ground, v=3, arm=4):
    """Lattice power pylon: two legs splaying out, cross bracing, two cross arms with hanging insulators."""
    h = ground - top
    for y in range(top, ground):
        t = (y - top) / float(h)
        half = 0.5 + t * 2.5
        c.set(x - half, y, v)
        c.set(x + half, y, v)
    for y in range(top + 3, ground - 1, 3):                        # bracing X's
        t = (y - top) / float(h)
        t2 = (y + 3 - top) / float(h)
        c.line(x - 0.5 - t * 2.5, y, x + 0.5 + t2 * 2.5, min(ground - 1, y + 3), v)
        c.line(x + 0.5 + t * 2.5, y, x - 0.5 - t2 * 2.5, min(ground - 1, y + 3), v)
    for y, a in ((top + 1, arm), (top + 4, arm - 1)):
        c.rect(x - a, y, x + a, y, v)
        c.set(x - a, y + 1, v)
        c.set(x + a, y + 1, v)
    c.set(x, top - 1, v)


def generator_yard():
    """Generator Yard: a power plant hall with a gable roof and a big lightning-bolt sign, a lattice pylon carrying
    sagging wires, a far cooling tower letting off steam, a transformer box with insulators, a chain-link fence
    with a warning sign across the front."""
    c = C(0)
    for y in range(4, 15):                                         # far cooling tower (waisted), light
        t = (y - 4) / 10.0
        half = 3.2 - 1.6 * math.sin(t * math.pi * 0.8)
        c.rect(int(round(28 - half)), y, int(round(28 + half)), y, 1)
    for k, (x, y, r) in enumerate(((28, 3, 1.6), (26, 1, 1.8), (29, 0, 1.4))):   # steam
        c.disc(x, y, r, 1)
        c.set(x - 1, y - 1, 0)
    c.rect(0, 15, W - 1, 15, 2)                                    # ground
    c.rect(0, 16, W - 1, H - 1, 1)
    c.rect(12, 7, 25, 15, 2)                                       # the plant hall
    c.poly([(11, 7), (18.5, 3), (26, 7)], 2)                       # gable roof
    c.line(11, 7, 18.5, 3, 3)
    c.line(18.5, 3, 26, 7, 3)
    c.line(13, 6, 18, 3.5, 1)
    c.rect(11, 7, 26, 7, 3)
    c.rect(25, 7, 25, 15, 3)
    c.rect(15, 8, 22, 14, 0)                                       # white sign with a black lightning bolt
    c.rect(15, 8, 22, 8, 3)
    c.rect(22, 8, 22, 14, 3)
    ra.stamp(c, ["...###.",
                 "..###..",
                 ".#####.",
                 "...##..",
                 "..##...",
                 ".#....."], 15, 9)
    c.rect(13, 13, 13, 14, 1)                                      # small windows
    c.rect(24, 13, 24, 14, 1)
    pylon(c, 5, 1, 18, v=3, arm=5)                                 # near pylon
    for y0, ax in ((2, 5), (5, 4)):                                # wires: off to the left, and to the hall roof
        for x in range(0, 5 - ax + 1):
            c.set(x, y0 + 1 + 0.4 * (5 - ax - x), 3)
        for x in range(5 + ax, 32):
            t = (x - 5 - ax) / float(32 - 5 - ax)
            yy = y0 + 1 + 6 * t * (1 - t)
            if c.get(x, int(round(yy))) == 0:
                c.set(x, yy, 3 if y0 == 2 else 2)
    c.rect(26, 11, 31, 17, 2)                                      # transformer box with insulator stacks
    c.rect(26, 11, 31, 11, 3)
    c.rect(26, 11, 26, 17, 3)
    for x in (28, 30):
        c.rect(x, 12, x, 16, 1)                                    # fins
    for x in (27, 30):
        for y in (8, 10):
            c.set(x, y, 3)
            c.set(x, y - 1, 1)
        c.set(x, 6, 3)
    c.dither(0, 19, W - 1, 22, 1, 2, 0.5)                          # chain-link fence across the front
    c.rect(0, 18, W - 1, 18, 3)
    for x in (0, 11, 21, 31):
        c.rect(x, 18, x, H - 1, 3)
    c.poly([(14, 22), (16.5, 18.5), (19, 22)], 0)                  # warning sign on the fence
    c.line(14, 22, 16.5, 18, 3)
    c.line(16.5, 18, 19, 22, 3)
    c.rect(14, 22, 19, 22, 3)
    c.rect(16, 20, 16, 20, 3)
    c.rect(0, 23, W - 1, 23, 2)
    return c


def busy_city():
    """Busy City: a skyline of tall towers - far ones pale, near ones dark with rows of lit windows - a big street
    crossing in front with zebra stripes, a bus, a traffic light, street lamps."""
    c = C(0)
    for x0, x1, top in ((0, 4, 6), (9, 12, 3), (17, 21, 5), (26, 31, 2)):   # far towers
        c.rect(x0, top, x1, 15, 1)
    c.rect(10, 1, 10, 2, 1)
    for x0, x1, top in ((3, 9, 4), (13, 18, 8), (22, 27, 1)):      # near towers
        c.rect(x0, top, x1, 15, 2)
        c.rect(x0, top, x1, top, 3)
        c.rect(x1, top, x1, 15, 3)
        for y in range(top + 2, 15, 2):                            # window rows
            for x in range(x0 + 1, x1 - 1, 2):
                c.set(x, y, 0 if (x * 3 + y) % 5 else 1)
    c.rect(24, 0, 24, 0, 3)                                        # antenna
    c.rect(0, 15, W - 1, 15, 3)
    c.rect(0, 16, W - 1, 16, 1)                                    # pavement
    c.rect(0, 17, W - 1, H - 1, 2)                                 # the street
    for x in range(1, W, 3):                                       # zebra crossing in perspective
        c.poly([(x, 18), (x + 1.5, 18), (x + 2.2 + (x - 16) * 0.25, 24), (x + 0.3 + (x - 16) * 0.25, 24)], 0)
    c.rect(0, 17, W - 1, 17, 3)
    c.rect(1, 13, 14, 19, 0)                                       # the bus, side on, at the kerb
    c.rect(1, 12, 13, 12, 3)
    c.rect(1, 12, 1, 19, 3)
    c.rect(14, 13, 14, 19, 3)
    c.set(14, 12, 2)
    c.rect(1, 19, 14, 19, 3)
    for x in range(3, 12, 3):
        c.rect(x, 14, x + 1, 15, 2)                                # windows
    c.rect(12, 14, 13, 18, 2)                                      # door
    c.rect(2, 17, 11, 17, 1)                                       # stripe
    for x in (4, 10):                                              # wheels
        c.rect(x - 1, 19, x + 1, 20, 3)
        c.set(x, 19, 1)
    c.rect(28, 7, 28, 17, 3)                                       # traffic light
    c.rect(27, 7, 29, 13, 3)
    c.set(28, 8, 0)
    c.set(28, 10, 1)
    c.set(28, 12, 1)
    ra.lamp_post(c, 20, 9, 17)
    return c


def sunbaked_ruins():
    """Sunbaked Ruins: a blazing sun, an old stone temple half swallowed by a dune - stepped roof, a dark doorway,
    rows of carved dots on the wall - broken pillars, a small cactus and sand ripples in front."""
    c = C(0)
    c.disc(5, 4, 3, 1)                                             # sun with heat rings
    for k in range(12):
        a = k * math.pi / 6
        c.set(5 + 4.6 * math.cos(a), 4 + 4.6 * math.sin(a), 1)
    c.poly([(0, 12), (7, 10), (14, 12), (22, 9), (32, 11), (32, 13), (0, 13)], 1)   # far dunes
    c.rect(0, 13, W - 1, H - 1, 1)
    # a stepped stone temple: four tiers of blocks, each narrower, light tops, black right ends, block joints
    tiers = ((11, 30, 15, 20), (13, 28, 11, 14), (15, 26, 8, 10), (17, 24, 5, 7))
    for x0, x1, y0, y1 in tiers:
        c.rect(x0, y0, x1, y1, 2)
        c.rect(x0, y0, x1, y0, 1)
        c.rect(x1, y0, x1, y1, 3)
        c.rect(x0, y0, x0, y1, 3)
        for y in range(y0 + 1, y1 + 1, 2):                         # block joints, staggered
            for x in range(x0 + 1 + (y % 4 == 0) * 2, x1, 4):
                c.set(x, y, 3)
    c.rect(11, 20, 30, 20, 3)
    c.rect(18, 15, 22, 20, 3)                                      # dark doorway with a lintel
    c.rect(17, 14, 23, 14, 3)
    c.rect(18, 14, 22, 14, 0)
    for y in (17, 19):                                             # carved dots beside the door
        for x in (13, 15, 25, 27):
            c.set(x, y, 0)
    c.poly([(0, 15), (6, 13), (13, 16), (19, 22), (24, 24), (0, 24)], 0)   # a dune swallowing its left side
    c.line(0, 15, 6, 13, 2)
    c.line(6, 13, 13, 16, 2)
    c.line(13, 16, 19, 22, 2)
    c.line(19, 22, 23, 24, 2)
    c.poly([(24, 24), (28, 22), (32, 21), (32, 24)], 0)            # sand drift at the right
    c.line(24, 23, 28, 22, 2)
    c.line(28, 22, 32, 21, 2)
    ra.ruin_column(c, 2, 20, 5, cut=2)                             # a broken pillar in the sand
    c.rect(9, 17, 9, 22, 3)                                        # small cactus
    c.rect(7, 18, 7, 19, 3)
    c.set(8, 19, 3)
    c.rect(11, 17, 11, 19, 3)
    c.set(10, 19, 3)
    c.set(9, 17, 2)
    for y, x0, x1 in ((22, 2, 5), (20, 13, 15), (23, 12, 17)):    # ripples
        c.rect(x0, y, x1, y, 1)
    return c


def kelp(c, x, ground, h, phase, v=3, wide=True):
    """Kelp: a swaying stalk (2 px when wide) with long leaf blades drooping off alternate sides."""
    for i in range(h):
        y = ground - 1 - i
        xx = x + round(1.3 * math.sin(i / 2.6 + phase))
        c.set(xx, y, v)
        if wide:
            c.set(xx + 1, y, v)
        if i % 3 == 2 and i < h - 1:
            s = 1 if (i // 3) % 2 else -1
            base = xx + (2 if s > 0 and wide else 1 if s > 0 else -1)
            c.set(base, y, v)
            c.set(base + s, y + 1, v)
            c.set(base + 2 * s, y + 2, 2 if v == 3 else 1)


def underwater():
    """Underwater: the rippling surface far above with shafts of light slanting down, deep water darkening toward
    the bottom, kelp swaying on both sides, rising bubbles, a big open clam with a pearl on the pale sand."""
    c = C(1)
    for x in range(W):                                             # the rippling surface, seen from below
        c.set(x, 1 + (1 if (x // 3) % 2 else 0), 0)
        c.set(x, 0, 0)
    c.dither(0, 11, W - 1, 14, 1, 2, 0.5)                          # water darkening with depth
    c.rect(0, 15, W - 1, 19, 2)
    for x0, w in ((3, 3), (12, 2), (20, 3)):                       # slanting shafts of light, fading out
        for y in range(3, 17):
            for d in range(w):
                xx = int(x0 + y * 0.45 + d)
                if y < 10:
                    c.set(xx, y, 0)
                elif (xx + y) % 2 == 0:
                    c.set(xx, y, 0 if y < 13 else 1)
    c.poly([(0, 20), (9, 19), (18, 20), (26, 18), (32, 19), (32, 24), (0, 24)], 0)   # pale sandy floor
    c.line(0, 20, 9, 19, 1)
    c.line(9, 19, 18, 20, 1)
    c.line(18, 20, 26, 18, 1)
    c.line(26, 18, 32, 19, 1)
    for y, x0, x1 in ((22, 2, 5), (23, 22, 26), (21, 27, 29)):
        c.rect(x0, y, x1, y, 1)                                    # sand ripples
    kelp(c, 9, 19, 9, 1.0, 2, wide=False)                          # far kelp, dark grey
    kelp(c, 23, 19, 11, 2.0, 2, wide=False)
    kelp(c, 2, 22, 21, 0.0, 3)                                     # near kelp, black
    kelp(c, 28, 22, 20, 2.5, 3)

    def shell(cx, cy, rx, ry, up):
        m = {(x, y) for y in range(H) for x in range(W)
             if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1 and ((y <= cy) if up else (y >= cy))}
        for x, y in m:
            edge = any((x + a, y + b) not in m for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)))
            rib = (x - int(cx)) % 2 == 0
            c.set(x, y, 3 if edge else (1 if rib else 2))
    shell(15.5, 17, 5.6, 4.2, True)                                # big open clam: lid raised
    shell(15.5, 20, 5.6, 3.2, False)                               # bowl
    c.rect(11, 18, 20, 19, 3)                                      # dark inside between lid and bowl
    c.rect(14, 18, 17, 20, 0)                                      # the pearl
    c.rect(15, 17, 16, 17, 0)
    c.set(17, 20, 1)
    c.set(14, 20, 1)
    for x, y in ((17, 11), (18, 8), (16, 5), (11, 10), (12, 7), (21, 12), (22, 9)):   # bubbles rising
        c.set(x, y, 0)
    c.rect(18, 3, 18, 3, 0)
    return c


def cliffside():
    """Cliffside: a towering cliff seen side-on, rising in great rock steps from the lower left to the top right -
    pale ledge tops, sunlit risers, dark cracked rock, a rope ladder and shrubs on the ledges - with a smoking
    volcano far off and the valley floor far below."""
    c = C(0)
    c.poly([(0, 15), (5, 8), (8, 8), (14, 15)], 1)                 # far volcano
    c.rect(5, 8, 8, 8, 2)
    for k, (x, y) in enumerate(((6, 6), (8, 4), (7, 2), (10, 1))):     # smoke plume
        c.disc(x, y, 1.0 + k * 0.3, 1)
    c.rect(0, 15, W - 1, 15, 2)
    c.dither(0, 16, W - 1, H - 1, 0, 1, 0.5)                       # the valley far below
    steps = ((7, 19), (13, 14), (19, 9), (25, 4))                  # (left x, top y) of each rock step
    for x0, top in steps:
        c.rect(x0, top, W - 1, H - 1, 2)                           # the rock mass
    for i, (x0, top) in enumerate(steps):
        bottom = steps[i - 1][1] if i else H - 1                   # the riser runs down to the step below
        right = steps[i + 1][0] - 2 if i + 1 < len(steps) else W - 1
        c.rect(x0, top + 1, x0 + 1, bottom, 1)                     # sunlit riser
        c.rect(x0 - 1, top, x0 - 1, bottom, 3)                     # black outline of the profile
        c.rect(x0 - 1, top, right + 1, top, 3)
        c.rect(x0, top + 1, right, top + 1, 0)                     # pale ledge top
        c.rect(x0 + 2, top + 2, right, top + 2, 1)
        for x in range(x0 + 3, right, 3):
            c.set(x, top + 3, 3)                                   # broken shadow under the lip
    for pts in (((10, 21), (11, 23)), ((16, 16), (15, 19), (17, 22)), ((22, 11), (23, 15), (21, 18)),
                ((28, 6), (27, 10), (29, 13), (28, 17)), ((25, 20), (26, 23))):
        for a, b in zip(pts, pts[1:]):                             # cracks
            c.line(a[0], a[1], b[0], b[1], 3)
    for x, y in ((18, 21), (24, 13), (30, 9), (12, 22), (30, 20), (20, 17)):
        c.rect(x, y, x + 1, y, 1)                                  # lit knobs of rock
    for y in range(10, 14):                                        # rope ladder down the third riser
        c.set(21, y, 3)
        c.set(23, y, 3)
        if y % 2 == 0:
            c.set(22, y, 0)
    for x, y in ((10, 19), (16, 14), (29, 4)):                     # shrubs on the ledge tops
        c.disc(x, y - 1, 1.1, 3)
        c.set(x - 1, y - 2, 2)
    return c


def gravestone(c, x, ground, w, h, v=2):
    """Round-topped grave stone: body v, black outline on the right/bottom, light top-left edge."""
    c.rect(x, ground - h + 1, x + w - 1, ground - 1, v)
    c.rect(x + 1, ground - h, x + w - 2, ground - h, v)
    c.rect(x + w - 1, ground - h + 1, x + w - 1, ground - 1, 3)
    c.set(x + w - 2, ground - h, 3)
    if w >= 3:
        c.set(x, ground - h + 1, 1 if v == 2 else 0)
        c.set(x + 1, ground - h, 1 if v == 2 else 0)


def misty_mountain():
    """Misty Mountain: a tall lone mountain with a shrine on its peak, bands of white mist drifting across it, rows
    of round-topped grave stones climbing the slope in front, a lantern by the near graves."""
    c = C(0)
    peak = [(0, 16), (6, 11), (12, 4), (15, 2), (18, 2), (21, 5), (26, 10), (32, 13)]
    c.poly(peak + [(32, 24), (0, 24)], 1)                          # the mountain, pale in the mist
    c.poly([(15, 2), (18, 2), (21, 5), (26, 10), (32, 13), (32, 16), (22, 9), (17, 4)], 2)   # shaded right face
    for a, b in zip(peak, peak[1:]):
        c.line(a[0], a[1], b[0], b[1], 2)
    c.rect(15, 0, 18, 1, 3)                                        # shrine on the peak
    c.rect(14, 0, 19, 0, 3)
    c.set(16, 1, 0)
    for x in range(8, 26, 3):                                      # far graves on the slope (tiny dark bumps)
        y = 9 + abs(x - 16) // 3
        c.rect(x, y, x + 1, y + 1, 2)
        c.set(x + 1, y, 3)
    for x in range(W):                                             # two drifting bands of white mist
        for yc, amp, th in ((7.5, 1.0, 1), (13.5, 1.0, 2)):
            yy = int(round(yc + amp * math.sin(x / 3.2 + yc)))
            for d in range(th):
                c.set(x, yy + d, 0)
            for d in (-2, -1, th, th + 1):                         # thin, see-through edges
                if (x + yy + d) % 2 and c.get(x, yy + d) != 3:
                    c.set(x, yy + d, 0 if abs(d - th / 2.0) < 2 else c.get(x, yy + d) and 1)
    for x in range(1, 31, 5):                                      # middle row of graves, below the mist
        gravestone(c, x, 19, 3, 3, 2)
    c.rect(0, 19, W - 1, H - 1, 1)                                 # near ground
    c.rect(0, 19, W - 1, 19, 2)
    for x, w, h in ((1, 5, 6), (24, 6, 7)):                        # near graves: pale stone, black outline
        gravestone(c, x, 23, w, h, 0)
        c.rect(x, 23 - h + 1, x, 22, 3)
        c.set(x + 1, 23 - h, 3)
        c.rect(x + 2, 23 - h - 1, x + w - 3, 23 - h - 1, 3)
        c.rect(x + w - 2, 23 - h + 1, x + w - 2, 22, 1)
        c.rect(x + 1, 23 - h + 3, x + w - 3, 23 - h + 3, 2)       # carved lines
        c.rect(x + 1, 23 - h + 5, x + w - 3, 23 - h + 5, 2)
        c.rect(x - 1, 23, x + w, 23, 3)                            # plinth
    c.rect(13, 14, 19, 14, 3)                                      # stone lantern with a lit window
    c.rect(14, 15, 18, 16, 3)
    c.rect(15, 15, 17, 16, 0)
    c.rect(14, 17, 18, 17, 3)
    c.rect(15, 18, 17, 22, 3)
    c.rect(13, 23, 19, 23, 3)
    c.set(16, 13, 3)
    for x, y in ((9, 21), (21, 22), (10, 23)):                     # grass tufts
        c.set(x, y, 2)
        c.set(x + 1, y - 1, 2)
    return c


def icicle(c, x, top, ln, w=1):
    """Icicle hanging from `top`: 2 px wide (3 at the root when w > 1) narrowing to a 1-px tip, white with a grey
    right side (all white where it hangs in front of the grey sky)."""
    px = []
    for i in range(ln):
        wide = w + 1 if i < 2 and w > 1 else (2 if i < ln * 0.6 else 1)
        for dx in range(wide):
            px.append((x + dx, top + i, 1 if dx == wide - 1 and wide > 1 else 0))
    for a, b, v in px:
        c.set(a, b, v if c.get(a, b) != 1 else 0)


def frozen_shoal():
    """Frozen Shoal: inside the mouth of an ice cave looking out to the sea - pale icicles hanging from the black
    ceiling, ice spikes rising from the frozen floor, a tide pool with cracks around it, the grey sea and a far ice
    floe outside."""
    c = C(3)
    c.poly([(0, 24), (1, 10), (5, 4), (12, 2), (22, 2), (28, 5), (31, 11), (32, 24)], 2)   # cave walls
    c.poly([(4, 15), (5, 11), (9, 8), (23, 8), (27, 11), (28, 15)], 1)                     # the mouth: grey sky
    c.rect(4, 13, 28, 13, 3)                                       # sea horizon
    c.rect(4, 14, 28, 16, 2)
    c.rect(7, 14, 10, 14, 1)
    c.rect(19, 15, 22, 15, 1)
    c.poly([(13, 13), (15, 10), (19, 10), (22, 13)], 0)            # far ice floe
    c.rect(19, 11, 21, 12, 1)
    c.poly([(0, 24), (0, 17), (8, 15), (24, 15), (32, 17), (32, 24)], 0)   # the ice floor
    c.line(0, 17, 8, 15, 2)
    c.rect(8, 15, 24, 15, 2)
    c.line(24, 15, 32, 17, 2)
    c.poly([(9, 22), (12, 18), (21, 18), (25, 21), (18, 23)], 1)   # tide pool
    c.rect(13, 19, 17, 19, 0)
    c.rect(19, 21, 21, 21, 0)
    for pts in (((2, 20), (5, 22), (4, 24)), ((27, 19), (29, 21)), ((7, 17), (9, 18))):
        for a, b in zip(pts, pts[1:]):                             # cracks in the ice
            c.line(a[0], a[1], b[0], b[1], 2)
    for x, top, ln, w in ((6, 5, 5, 1), (10, 3, 9, 2), (15, 3, 4, 1), (19, 3, 7, 2), (24, 4, 5, 1),
                          (27, 6, 6, 1)):
        icicle(c, x, top, ln, w)
    for x, base, h, w in ((3, 23, 12, 3), (8, 23, 6, 2), (28, 23, 11, 3), (23, 23, 5, 2)):
        ice_spike(c, x, base, h, w)                                # ice spikes rising from the floor
    return c


def ice_spike(c, x, base, h, w):
    """Ice spike growing up from `base`: lit left half white, right half light grey; grey outline on the lit side,
    black on the shaded side."""
    m = {}
    for i in range(h):
        half = w * (1 - i / float(h))
        for xx in range(int(math.floor(x - half)), int(math.ceil(x + half)) + 1):
            if abs(xx - x) <= half + 0.01:
                m[(xx, base - i)] = 0 if xx <= x else 1
    for (xx, y), v in m.items():
        edge = any((xx + a, y + b) not in m for a, b in ((1, 0), (-1, 0), (0, -1)))
        c.set(xx, y, (3 if xx > x else 2) if edge else v)
    c.set(x, base - h, 2)


def beginners_path():
    """Beginner's Path: a little town (two houses and a lab) in the distance, a pale path winding from it toward us
    through the grass, a wooden route sign, flowers, patches of tall grass and a small tree."""
    c = C(0)
    ra.cloud(c, 4, 3, 7)
    ra.cloud(c, 21, 2, 6)
    ra.far_hills(c, 8, 0.8, 4.0, 0.5)
    for x, w in ((6, 5), (21, 5)):                                 # the little town: two houses...
        y = 11
        c.rect(x, y - 3, x + w - 1, y - 1, 0)
        c.rect(x, y - 3, x, y - 1, 3)
        c.rect(x + w - 1, y - 3, x + w - 1, y - 1, 3)
        for i in range((w + 1) // 2 + 1):
            c.rect(x - 1 + i, y - 4 - i, x + w - i, y - 4 - i, 2)
        c.rect(x - 1, y - 4, x + w, y - 4, 3)
        c.set(x + 1, y - 2, 2)
        c.set(x + w - 2, y - 2, 3)
    c.rect(12, 6, 19, 10, 0)                                       # ...and a bigger lab with a flat roof
    c.rect(12, 5, 19, 5, 3)
    c.rect(12, 5, 12, 10, 3)
    c.rect(19, 5, 19, 10, 3)
    for x in (14, 17):
        c.set(x, 7, 2)
    c.rect(15, 9, 16, 10, 3)
    c.rect(17, 3, 17, 4, 3)                                        # antenna on the lab
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, H - 1, 1)
    pts = []
    for k in range(26):                                            # the winding path, widening toward us
        t = k / 25.0
        y = 12 + t * 12
        x = 16 + 5 * math.sin(t * 3.3 + 0.3) * t
        half = 0.8 + t * 3.6
        c.rect(int(round(x - half)), int(y), int(round(x + half)), int(y), 0)
        c.set(x - half - 1, y, 2)
        c.set(x + half + 1, y, 2)
    for x, y in ((24, 13), (7, 14)):                               # small far tufts
        ra.stamp(c, ra.TUFT_SMALL, x, y)
    ra.stamp(c, ra.TUFT_BIG, 23, 16)                               # tall grass patches
    ra.stamp(c, ra.TUFT_BIG, 0, 16)
    c.rect(10, 16, 10, 20, 3)                                      # little wooden route sign
    c.rect(8, 14, 13, 16, 2)
    c.rect(8, 14, 13, 14, 3)
    c.rect(13, 14, 13, 16, 3)
    c.rect(9, 15, 11, 15, 1)
    for x, y in ((14, 19), (20, 14), (18, 22)):                    # pebbles on the path
        c.set(x, y, 1)
    ra.tree2(c, 30, 15, 2, 3)
    flower = ra.flower
    for x, y in ((6, 22), (27, 22), (12, 13)):
        flower(c, x, y, 0)
    return c


def crate(c, x, ground, w, h):
    c.rect(x, ground - h, x + w - 1, ground - 1, 1)
    c.rect(x, ground - h, x + w - 1, ground - h, 3)
    c.rect(x, ground - 1, x + w - 1, ground - 1, 3)
    c.rect(x, ground - h, x, ground - 1, 3)
    c.rect(x + w - 1, ground - h, x + w - 1, ground - 1, 3)
    c.line(x, ground - h, x + w - 1, ground - 1, 2)                # cross brace
    c.line(x + w - 1, ground - h, x, ground - 1, 2)


def scavengers_trail():
    """Scavenger's Trail: a dirt trail into the hills littered with finds - a stack of wooden crates, a barrel, an
    old tyre, a broken signpost, scraps - and an open treasure chest sparkling in the front."""
    c = C(0)
    c.poly([(0, 11), (6, 7), (12, 10), (19, 6), (26, 9), (32, 7), (32, 13), (0, 13)], 1)   # far hills
    c.rect(0, 13, W - 1, 13, 2)
    c.rect(0, 14, W - 1, H - 1, 1)
    c.poly([(14, 14), (17, 14), (25, 24), (5, 24)], 0)             # trail
    c.line(14, 14, 5, 23, 2)
    c.line(17, 14, 25, 23, 2)
    for x, y in ((12, 19), (19, 21), (15, 16)):                    # ruts / footprints
        c.rect(x, y, x + 1, y, 1)
    crate(c, 0, 22, 7, 7)                                          # crate stack, left
    crate(c, 6, 22, 5, 5)
    crate(c, 2, 15, 5, 5)
    c.rect(19, 11, 21, 15, 2)                                      # a barrel by the trail
    c.rect(19, 11, 21, 11, 1)
    c.rect(19, 12, 21, 12, 3)
    c.rect(19, 14, 21, 14, 3)
    c.rect(21, 11, 21, 15, 3)
    c.rect(19, 11, 19, 15, 3)
    for x in range(24, 30):                                        # an old tyre lying on its side
        c.set(x, 14, 3)
        c.set(x, 16, 3)
    c.set(23, 15, 3)
    c.set(30, 15, 3)
    c.rect(24, 15, 29, 15, 2)
    c.rect(26, 15, 27, 15, 1)
    c.line(9, 15, 12, 11, 3)                                       # a broken signpost
    c.rect(9, 11, 13, 12, 2)
    c.rect(9, 11, 13, 11, 3)
    for x, y in ((17, 18), (13, 21)):                              # tin cans / scraps on the trail
        c.rect(x, y, x + 1, y, 2)
        c.set(x + 1, y - 1, 3)
    # treasure chest, open, front right
    c.rect(22, 19, 30, 23, 2)
    c.rect(22, 19, 30, 19, 3)
    c.rect(22, 19, 22, 23, 3)
    c.rect(30, 19, 30, 23, 3)
    c.rect(22, 23, 30, 23, 3)
    c.rect(25, 20, 27, 21, 3)                                      # lock
    c.set(26, 20, 0)
    c.poly([(22, 19), (23, 15), (30, 15), (31, 19)], 3)            # open lid
    c.rect(24, 16, 29, 17, 2)
    c.rect(23, 18, 29, 18, 0)                                      # gleaming contents
    c.set(25, 17, 0)
    c.set(28, 17, 1)
    sparkle(c, 20, 17, 0)
    c.set(20, 17, 1)
    for x, y in ((31, 13), (21, 21)):
        c.set(x, y, 0)
    return c


def traders_hill():
    """Trader's Hill: a round grassy hill with a big striped trading tent on its top, strings of pennants running
    down from its pole, market stalls on the slopes, a path climbing up, crates of goods at the foot."""
    c = C(0)
    ra.cloud(c, 2, 3, 6)
    ra.cloud(c, 25, 5, 6)
    c.poly([(0, 24), (0, 20), (6, 15), (12, 12), (20, 12), (26, 15), (32, 19), (32, 24)], 1)   # the hill
    c.dither(0, 21, W - 1, H - 1, 1, 2, 0.25)
    c.line(0, 20, 6, 15, 3)
    c.line(6, 15, 12, 12, 3)
    c.rect(12, 12, 20, 12, 3)
    c.line(20, 12, 26, 15, 3)
    c.line(26, 15, 32, 19, 3)
    c.poly([(10, 12), (16, 4), (22, 12)], 0)                       # the tent
    for k in range(-5, 6, 2):                                      # stripes
        c.line(16, 4, 16 + k, 12, 2)
    c.line(10, 12, 16, 4, 3)
    c.line(16, 4, 22, 12, 3)
    c.rect(15, 9, 17, 12, 3)                                       # entrance
    c.rect(16, 1, 16, 4, 3)                                        # pole + flag
    c.rect(17, 1, 19, 2, 3)
    for side in (-1, 1):                                           # pennant strings
        for k in range(1, 14):
            x = 16 + side * k
            y = 2 + k * 0.75 + 0.06 * k * k
            c.set(x, y, 3 if k % 3 else 2)
            if k % 3 == 0:
                c.set(x, y + 1, 3)
    ra.stall(c, 3, 21, 6)                                          # stalls on the slopes
    ra.stall(c, 24, 21, 6)
    for k in range(10):                                            # path up the hill
        y = 23 - k
        x = 15 + round(1.5 * math.sin(k / 2.0))
        c.rect(x - (3 - k // 4), y, x + (3 - k // 4), y, 1 if k > 5 else 0)
    crate(c, 11, 24, 3, 3)
    crate(c, 19, 24, 3, 3)
    return c


def champions_road():
    """Champion's Road: a straight stone road between rows of torch-topped pillars, getting smaller toward a great
    gate with a bright arched opening, a tall crowned tower rising behind it, jagged far mountains either side."""
    c = C(0)
    c.poly([(0, 12), (3, 6), (6, 9), (9, 4), (12, 12)], 1)         # far jagged mountains
    c.poly([(20, 12), (23, 5), (26, 8), (29, 3), (32, 7), (32, 12)], 1)
    c.rect(13, 2, 18, 9, 2)                                        # the champion's tower behind the gate
    c.rect(13, 2, 13, 9, 1)
    c.rect(18, 2, 18, 9, 3)
    for x in (12, 14, 16, 18, 19):                                 # crown battlements
        c.set(x if x != 19 else 19, 1, 3)
    c.rect(12, 2, 19, 2, 3)
    c.rect(15, 0, 16, 0, 3)                                        # spire tip
    c.rect(15, 4, 16, 5, 0)                                        # lit window
    c.rect(9, 8, 22, 12, 3)                                        # the gate
    c.rect(8, 7, 23, 7, 3)
    c.rect(9, 8, 22, 8, 2)
    arch(c, 15.5, 9, 12, 2, 1, rv=1)                               # bright arched opening
    c.rect(15, 10, 16, 12, 0)
    c.rect(0, 12, W - 1, 12, 2)
    c.rect(0, 13, W - 1, H - 1, 1)
    c.poly([(13.5, 12), (18.5, 12), (30, 24), (2, 24)], 0)         # road in perspective
    c.line(13.5, 12, 2, 23.5, 2)
    c.line(18.5, 12, 30, 23.5, 2)
    for y in (14, 17, 21):                                         # paving joins
        t = (y - 12) / 12.0
        c.rect(int(13.5 - 11.5 * t) + 2, y, int(18.5 + 11.5 * t) - 2, y, 1)
    for y, h, w in ((14, 3, 1), (17, 5, 2), (22, 9, 3)):           # torch pillars lining both sides
        t = (y - 12) / 12.0
        for side in (-1, 1):
            xe = (13.5 - 11.5 * t) if side < 0 else (18.5 + 11.5 * t)
            x0 = int(round(xe)) - w - 1 if side < 0 else int(round(xe)) + 1
            x1 = x0 + w - 1
            top = y - h
            c.rect(x0, top, x1, y, 2 if w > 1 else 3)
            c.rect(x1, top, x1, y, 3)
            if w > 1:
                c.rect(x0, top, x0, y, 3)
                c.rect(x0 - 1, top, x1 + 1, top, 3)                # bowl
                c.rect(x0 - 1, top - 1, x1 + 1, top - 1, 1)        # flame
                c.rect(x0, top - 2, x1, top - 2, 1)
                c.set(x0 + w // 2, top - 1, 0)
                c.set(x0 + w // 2, top - 3, 1)
            else:
                c.set(x0, top - 1, 1)
    return c


def starfall_crater():
    """Starfall Crater: a dark night sky with stars and three falling stars streaking down, a wide crater bowl
    with a raised rocky rim, a glowing meteorite in its middle, boulders thrown out around it."""
    c = C(2)
    for x, y in ((3, 2), (9, 6), (14, 1), (20, 4), (27, 2), (30, 7), (6, 10), (24, 9), (17, 8)):
        c.set(x, y, 0)
    for x0, y0, ln in ((22, 0, 7), (8, 1, 5), (29, 4, 4)):         # falling stars with fading tails
        for k in range(ln):
            c.set(x0 - k, y0 + k * 0.6, 1 if k > 0 else 0)
        c.set(x0 - ln, y0 + ln * 0.6, 0)
        c.set(x0 - ln - 1, y0 + ln * 0.6, 0)
        c.set(x0 - ln, y0 + ln * 0.6 - 1, 0)
        c.set(x0 - ln, y0 + ln * 0.6 + 1, 0)
    c.poly([(0, 13), (6, 11), (12, 12), (20, 11), (26, 12), (32, 11), (32, 14), (0, 14)], 3)   # far ridge
    c.rect(0, 14, W - 1, H - 1, 1)                                 # plain
    cx, cy, rx, ry = 15.5, 18.5, 14.0, 4.6

    def d(x, y):
        return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
    for y in range(12, 24):
        for x in range(W):
            if d(x, y) <= 1:
                c.p[y][x] = 3                                     # the bowl, deep in shadow
            elif d(x, y) <= 1.45 and y < cy:
                c.p[y][x] = 0 if d(x, y) > 1.15 else 2            # raised rim: pale crest, dark inner slope
            elif d(x, y) <= 1.3:
                c.p[y][x] = 2                                     # near rim lip
    for y in range(12, 24):                                        # glow round the meteorite
        for x in range(W):
            g = ((x - 15.5) / 7.0) ** 2 + ((y - 19) / 2.6) ** 2
            if d(x, y) <= 0.9 and g <= 1:
                c.p[y][x] = 1 if g > 0.45 else 0
    ra.boulder(c, 15.5, 21, 2)                                     # the meteorite
    c.set(15, 18, 0)
    for k in (1, 2):                                               # glow rays
        c.set(15 - 4 - k, 19, 1)
        c.set(16 + 4 + k, 19, 1)
    for x, g, r in ((1, 16, 1), (30, 17, 1), (3, 23, 2), (28, 23, 2)):
        ra.boulder(c, x, g, r)                                     # rocks thrown out of the crater
    return c


def far_shores():
    """Far Shores: a long low coastline with hills far across the water at the horizon, a setting sun half sunk
    behind it with a path of light on the sea, tall dark sea stacks, wet rocks and tide pools in front."""
    c = C(0)
    c.disc(9, 6, 3.2, 1)                                           # setting sun, half sunk behind the far shore
    c.disc(9, 6, 1.6, 0)
    c.poly([(0, 9), (0, 7), (5, 6), (11, 8), (17, 5), (23, 7), (32, 6), (32, 9)], 1)   # far shore
    c.rect(0, 9, W - 1, 9, 2)
    c.rect(0, 10, W - 1, H - 1, 1)                                 # the sea
    for y in range(10, 18, 2):                                     # path of light under the sun
        t = (y - 10) / 8.0
        c.rect(int(9 - 1 - t * 2), y, int(9 + 1 + t * 2), y, 0)
    for y, xs in ((12, (17, 26)), (15, (20,)), (11, (28,))):
        for x in xs:
            c.rect(x, y, x + 2, y, 0)
    c.poly([(20, 17), (21, 7), (23, 5), (26, 6), (27, 17)], 2)     # sea stack
    c.line(21, 7, 20, 17, 3)
    c.line(26, 6, 27, 17, 3)
    c.poly([(26, 6), (27, 17), (25, 17), (25, 7)], 3)
    c.rect(22, 5, 25, 5, 3)
    c.line(22, 9, 25, 10, 3)
    c.line(21, 13, 25, 14, 3)
    c.poly([(28, 15), (29, 10), (31, 11), (32, 15)], 2)            # smaller stack
    c.rect(19, 17, 28, 17, 0)
    c.poly([(0, 24), (0, 17), (5, 16), (10, 18), (15, 19), (22, 21), (32, 20), (32, 24)], 3)   # near rocks
    c.rect(4, 17, 6, 17, 2)
    c.rect(14, 20, 16, 20, 2)
    c.poly([(5, 21), (9, 20), (14, 22), (8, 23)], 1)               # tide pools
    c.rect(7, 21, 9, 21, 0)
    c.poly([(22, 23), (26, 22), (30, 23)], 1)
    for x, y in ((11, 18), (18, 19), (29, 19), (2, 16)):           # spray
        c.set(x, y - 1, 0)
    return c


HOENN = {"dim_tunnel": dim_tunnel, "beachside": beachside, "generator_yard": generator_yard,
         "busy_city": busy_city, "sunbaked_ruins": sunbaked_ruins, "underwater": underwater,
         "cliffside": cliffside, "misty_mountain": misty_mountain, "frozen_shoal": frozen_shoal,
         "beginners_path": beginners_path, "scavengers_trail": scavengers_trail, "traders_hill": traders_hill,
         "champions_road": champions_road, "starfall_crater": starfall_crater, "far_shores": far_shores}
ra.COURSES.update(HOENN)


def write_hoenn():
    os.makedirs(ra.OUT, exist_ok=True)
    for name, fn in HOENN.items():
        c = fn()
        path = os.path.join(ra.OUT, f"{name}.txt")
        with open(path, "w") as f:
            f.write(f"; {name}: 32x24, . white  - light grey  + dark grey  # black (tools/route_art_hoenn.py)\n")
            for row in c.p:
                f.write("".join(ra.CH[v] for v in row) + "\n")
        print("->", path)


if __name__ == "__main__":
    write_hoenn()
    ra.preview_names(os.path.join(ra.ROOT, "art_src", "route_pictures_hoenn.png"), list(HOENN))
