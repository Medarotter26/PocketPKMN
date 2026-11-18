#!/usr/bin/env python3
"""route_art_frlg.py - PocketPKMN's own course pictures for the FireRed / LeafGreen (Kanto) courses.

Same canvas, shades and helpers as route_art.py (imported, not changed). 14 pictures, one function per course slug,
each built far (light) to near (dark) with a clear subject. Running it writes art/routes/<slug>.txt for these 14
only and renders art_src/route_pictures_frlg.png.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route_art as ra  # noqa: E402

C, W, H = ra.C, ra.W, ra.H


# --- small local helpers ---------------------------------------------------------------------------------------

def tall_grass(c, x0, y0, x1, y1):
    """A patch of Kanto-style tall grass: dark block, a zigzag top edge, rows of black 'v' blades."""
    c.rect(x0, y0, x1, y1, 2)
    for x in range(x0, x1 + 1, 2):
        c.set(x, y0 - 1, 2)
    for r, y in enumerate(range(y0, y1 + 1, 2)):
        for x in range(x0 + (r % 2) * 2, x1 + 1, 4):
            c.set(x, y, 3)
            c.set(x + 1, y + 1, 3)
            c.set(x + 2, y, 3)


def facade(c, x, ground, w, h, roof, wall=0, edge=3, roofv=2, win=2, door=True):
    """House front with a gable roof, in chosen shades (lighter ones read as farther away)."""
    c.rect(x, ground - h, x + w - 1, ground - 1, wall)
    c.rect(x, ground - h, x, ground - 1, edge)
    c.rect(x + w - 1, ground - h, x + w - 1, ground - 1, edge)
    mid = x + (w - 1) / 2.0
    for i in range(roof + 1):
        half = (w + 1) / 2.0 * (1 - i / float(roof + 1)) + 0.5
        c.rect(int(round(mid - half)), ground - h - i, int(round(mid + half)), ground - h - i, roofv if i else edge)
    if w >= 6:
        c.rect(x + 1, ground - h + 1, x + 2, ground - h + 2, win)
    if door:
        c.rect(x + w - 3, ground - 3, x + w - 2, ground - 1, edge)


def container(c, x0, y0, x1, y1, v):
    """Freight container: ribbed side (alternating shade stripes), black outline, door bars at the right end."""
    c.rect(x0, y0, x1, y1, v)
    for x in range(x0 + 1, x1, 2):
        c.rect(x, y0 + 1, x, y1 - 1, max(0, v - 1))
    c.rect(x0, y0, x1, y0, 3)
    c.rect(x0, y1, x1, y1, 3)
    c.rect(x0, y0, x0, y1, 3)
    c.rect(x1, y0, x1, y1, 3)


def pylon(c, x, top, ground):
    """Lattice power pylon: tapering legs with cross-bracing, two cross-arms."""
    for y in range(top, ground):
        t = (y - top) / float(ground - top)
        half = 1 + int(round(3 * t))
        c.set(x - half, y, 3)
        c.set(x + half, y, 3)
        if (y - top) % 4 == 2 and y > top + 4:
            c.rect(x - half, y, x + half, y, 3)                    # struts
        elif (y - top) % 4 == 0 and y > top + 4:
            c.set(x, y, 3)                                         # middle of the cross-bracing
    c.rect(x - 4, top + 1, x + 4, top + 1, 3)                      # cross-arms with insulators
    c.rect(x - 3, top + 4, x + 3, top + 4, 3)
    for dx in (-4, 4):
        c.set(x + dx, top + 2, 2)
    for dx in (-3, 3):
        c.set(x + dx, top + 5, 2)
    c.set(x, top - 1, 3)


def birch(c, x, top, ground, w=2):
    """Birch trunk: white bark, black outline, black bark marks, a forked bare branch."""
    for y in range(top, ground):
        c.rect(x, y, x + w - 1, y, 0)
        c.set(x - 1, y, 3)
        c.set(x + w, y, 3)
        if (y * 7 + x * 3) % 5 == 0:
            c.rect(x, y, x + w // 2, y, 3)


# --- the 14 pictures -------------------------------------------------------------------------------------------

def field():
    """Field: an open Kanto route - a sandy path running up to a far tree line, patches of tall grass on both
    sides, a one-way ledge, a route sign post, clouds and a bird."""
    c = C(0)
    ra.cloud(c, 2, 3, 8)
    ra.cloud(c, 22, 2, 6)
    ra.gsc_bird(c, 14, 4)
    for x in range(0, W + 2, 3):                                   # far tree line
        c.disc(x, 10, 2.2, 1)
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, H - 1, 1)
    c.poly([(10, 24), (21, 24), (17, 12), (15, 12)], 0)            # path
    c.line(10, 23, 15, 12, 2)
    c.line(21, 23, 17, 12, 2)
    c.rect(0, 15, 11, 15, 3)                                       # ledge
    c.rect(0, 16, 11, 16, 2)
    tall_grass(c, 21, 13, 31, 15)                                  # far grass
    tall_grass(c, 0, 19, 9, 23)                                    # near grass
    tall_grass(c, 23, 19, 31, 23)
    c.rect(22, 8, 27, 11, 3)                                       # route sign post
    c.rect(23, 9, 26, 10, 0)
    c.rect(24, 9, 25, 9, 2)
    c.rect(24, 12, 25, 13, 3)
    for x, y in ((13, 21), (19, 17), (16, 15)):                    # pebbles
        c.set(x, y, 1)
    return c


def forest():
    """Forest: inside a dense broad-leaf wood - a leafy canopy closing overhead, two big leafy trees framing a
    bright gap with paler trees deep inside, a path through tall grass, a cocoon hanging on a thread."""
    c = C(1)
    c.disc(16, 10, 5, 0)                                           # light deep in the wood
    c.rect(11, 10, 21, 14, 0)
    for x, cy in ((12, 8), (20, 9)):                               # pale trees deep inside
        c.rect(x, cy, x, 14, 1)
        c.disc(x, cy - 1, 2, 1)
    for x in (2, 29):                                              # darker trees behind the big ones
        c.rect(x, 6, x, 15, 2)
        c.disc(x, 6, 3, 2)
    c.rect(0, 15, W - 1, H - 1, 2)                                 # forest floor
    c.rect(0, 15, W - 1, 15, 3)
    c.poly([(10, 24), (21, 24), (17, 15), (15, 15)], 1)            # path
    c.poly([(14, 24), (18, 24), (16, 16), (15, 16)], 0)
    ra.tree2(c, 6, 18, 5, 8)                                       # big leafy trees
    ra.tree2(c, 26, 18, 5, 8)
    for x in (6, 26):
        c.rect(x - 1, 10, x - 1, 17, 3)
        c.set(x - 2, 17, 3)
        c.set(x + 1, 17, 3)
    for x in range(-2, W + 3, 5):                                  # canopy closing overhead
        c.disc(x, -1, 3.2, 3)
    for x, y in ((3, 0), (13, 1), (23, 0), (18, 1)):
        c.set(x, y, 2)
    c.rect(11, 2, 11, 5, 3)                                        # a cocoon hanging on a thread
    c.rect(10, 6, 12, 9, 2)
    c.rect(10, 6, 10, 9, 3)
    c.rect(12, 6, 12, 9, 3)
    c.rect(10, 6, 12, 6, 3)
    c.set(11, 10, 3)
    c.set(11, 7, 1)
    tall_grass(c, 0, 20, 9, 23)                                    # tall grass by the path
    tall_grass(c, 22, 20, 31, 23)
    return c


def rocky_foothills():
    """Rocky Foothills: a big mountain behind (lit face light, shadow face dark), a cave mouth at its foot, rocky
    terraces stepping down to the front, boulders and a path climbing to the cave."""
    c = C(0)
    ra.cloud(c, 2, 2, 6)
    c.poly([(0, 13), (5, 8), (10, 9), (17, 1), (25, 5), (32, 9), (32, 13)], 1)        # mountain
    c.poly([(17, 1), (25, 5), (32, 9), (32, 13), (22, 13), (19, 6)], 2)                 # shadow face
    for a, b in (((5, 8), (10, 9)), ((10, 9), (17, 1)), ((17, 1), (25, 5)), ((25, 5), (32, 9)), ((0, 13), (5, 8))):
        c.line(a[0], a[1], b[0], b[1], 3)
    c.line(13, 6, 15, 10, 2)
    c.line(26, 8, 28, 12, 3)
    c.disc(12, 14, 3.2, 3)                                         # cave mouth
    c.rect(8, 14, 16, 14, 3)
    c.rect(0, 14, W - 1, 14, 2)
    c.rect(0, 15, W - 1, H - 1, 1)
    c.rect(9, 15, 15, 15, 3)
    for x0, x1, y in ((18, 31, 16), (0, 7, 18), (22, 31, 19)):     # rock terraces
        c.rect(x0, y, x1, y + 2, 2)
        c.rect(x0, y + 2, x1, y + 2, 3)
        c.rect(x0, y, x1, y, 1)
        c.set(x0, y + 1, 3)
    c.poly([(9, 24), (16, 24), (13, 15), (11, 15)], 0)             # path to the cave
    ra.boulder(c, 25, 24, 3)
    ra.boulder(c, 4, 24, 2)
    ra.boulder(c, 19, 16, 1)
    for x, y in ((8, 22), (17, 21), (29, 22), (21, 17)):
        c.rect(x, y, x + 1, y, 3)
    return c


def riverside():
    """Riverside: a river running out of the distance and widening towards you, under a far arched bridge,
    grassy banks with reeds, a willow, stepping stones across."""
    c = C(0)
    ra.cloud(c, 21, 2, 7)
    ra.far_hills(c, 8, 1, 3.0)
    for y in range(8, 11):
        c.rect(0, y, W - 1, y, 1)
    c.rect(0, 10, W - 1, 10, 2)
    c.rect(0, 11, W - 1, H - 1, 2)                                 # banks
    c.poly([(13, 11), (19, 11), (32, 21), (32, 24), (2, 24)], 1)   # river
    c.line(13, 11, 2, 24, 3)
    c.line(19, 11, 32, 21, 3)
    for y, x0, x1 in ((13, 15, 17), (16, 12, 15), (17, 19, 22), (20, 9, 12), (22, 22, 27)):
        c.rect(x0, y, x1, y, 0)                                    # ripples
    for k in range(13):                                            # far arched bridge with a railing
        x = 10 + k
        y = 8 - round(2 * math.sin(math.pi * k / 12))
        c.set(x, y, 2)
        c.set(x, y + 1, 1)
        if k % 3 == 0:
            c.set(x, y - 1, 2)
    c.rect(10, 8, 10, 10, 2)
    c.rect(22, 8, 22, 10, 2)
    for x, y in ((12, 22), (17, 20), (21, 17), (24, 19)):          # stepping stones
        c.rect(x - 1, y, x + 1, y, 2)
        c.rect(x - 1, y + 1, x + 1, y + 1, 3)
        c.set(x - 1, y, 0)
    for x, top in ((1, 15), (3, 17), (5, 16), (29, 13), (27, 14), (30, 16)):   # reeds
        c.rect(x, top, x, top + 4, 3)
        c.set(x, top - 1, 2)
    ra.tree2(c, 4, 14, 4, 4)                                       # bankside tree
    return c


def underground():
    """Underground: a tiled underground passage in one-point perspective - checker floor tiles running away, wall
    panels, a row of ceiling lights, a bright stairway up at the far end."""
    c = C(0)
    vx, vy = 15.5, 11.3
    for y in range(H):
        for x in range(W):
            dx, dy = x + 0.5 - vx, y + 0.5 - vy
            if abs(dy) * 1.4 >= abs(dx) and dy < 0:
                c.p[y][x] = 2                                      # ceiling
            elif abs(dy) * 1.4 >= abs(dx):
                u = dx / dy
                v = 1.0 / dy
                c.p[y][x] = 0 if (int(math.floor(u * 2.2)) + int(math.floor(v * 14))) % 2 else 1   # floor
            else:
                d = 1.0 / abs(dx)
                c.p[y][x] = 2 if int(d * 22) % 3 == 0 else 1       # walls with panel seams
    c.rect(12, 9, 19, 14, 0)                                       # far end: stairway up
    for i, y in enumerate((14, 13, 12, 11, 10)):
        c.rect(12 + i // 2, y, 19 - i // 2, y, 1 if i % 2 else 0)
        c.rect(12 + i // 2, y, 19 - i // 2, y, 0)
        c.rect(12, y, 19, y, 2 if i % 2 == 0 else 0)
    c.rect(12, 9, 19, 9, 3)
    c.rect(12, 9, 12, 14, 3)
    c.rect(19, 9, 19, 14, 3)
    c.line(0, 0, 12, 9, 3)                                         # corner edges
    c.line(31, 0, 19, 9, 3)
    c.line(0, 23, 12, 14, 3)
    c.line(31, 23, 19, 14, 3)
    for y, half in ((1, 3), (4, 2), (7, 1)):                       # ceiling lights
        c.rect(int(vx - half), y, int(vx + half) + 1, y, 0)
    c.rect(14, 7, 17, 7, 0)
    return c


def residential_zone():
    """Residential Zone: a big family house close up - shingled roof with a smoking chimney, upstairs windows, a
    door with a path out through a picket-fence gate, a flower bed, a mailbox, a smaller house farther off."""
    c = C(0)
    ra.cloud(c, 22, 2, 7)
    c.rect(0, 15, W - 1, H - 1, 1)                                 # lawn
    c.rect(0, 15, W - 1, 15, 2)
    c.rect(23, 11, 29, 14, 0)                                      # far house, light
    c.rect(23, 11, 23, 14, 2)
    c.rect(29, 11, 29, 14, 2)
    for i in range(4):
        c.rect(22 + i, 10 - i, 30 - i, 10 - i, 1)
    c.rect(25, 12, 26, 12, 1)
    c.rect(27, 12, 28, 14, 2)
    c.disc(31, 10, 2.5, 2)                                         # its tree
    c.rect(31, 12, 31, 14, 3)
    c.disc(5, 1, 1.4, 1)                                           # chimney smoke
    c.disc(7, 0, 1.2, 1)
    c.rect(4, 2, 5, 5, 3)                                          # chimney
    for i in range(6):                                             # shingled roof
        y = 9 - i
        x0, x1 = 0 + i, 21 - i
        c.rect(x0, y, x1, y, 2)
        for x in range(x0 + (i % 2) * 2, x1 + 1, 4):
            c.set(x, y, 3)
        c.set(x0, y, 3)
        c.set(x1, y, 3)
    c.rect(5, 4, 16, 4, 3)
    c.rect(0, 9, 21, 9, 3)                                         # eaves
    c.rect(1, 10, 20, 18, 0)                                       # walls
    c.rect(1, 10, 1, 18, 3)
    c.rect(20, 10, 20, 18, 3)
    c.rect(1, 10, 20, 10, 2)
    for x0, y0 in ((3, 11), (15, 11), (3, 14)):                    # windows
        c.rect(x0, y0, x0 + 3, y0 + 2, 3)
        c.rect(x0 + 1, y0 + 1, x0 + 2, y0 + 1, 1)
    c.rect(9, 12, 12, 18, 3)                                       # door with step
    c.rect(10, 13, 11, 17, 2)
    c.set(11, 15, 0)
    c.rect(8, 19, 13, 19, 2)
    for x in (2, 5, 15, 18):                                       # flower bed along the front
        c.set(x, 18, 2)
        ra.flower(c, x, 17, 0)
    c.rect(15, 14, 18, 16, 3)
    c.rect(16, 15, 17, 15, 1)
    c.poly([(9, 20), (13, 20), (16, 24), (7, 24)], 0)              # path
    for x in list(range(0, 8, 2)) + list(range(15, 24, 2)):       # picket fence with the gate open
        c.rect(x, 20, x, 22, 0)
        c.set(x, 19, 3)
        c.rect(x + 1, 20, x + 1, 22, 3)
    c.rect(0, 21, 7, 21, 3)
    c.rect(15, 21, 24, 21, 3)
    c.line(14, 20, 16, 23, 3)                                      # open gate
    c.rect(26, 16, 29, 18, 3)                                      # mailbox
    c.rect(27, 17, 28, 17, 2)
    c.set(30, 15, 3)
    c.rect(27, 19, 27, 23, 3)
    return c


ra.FONT3.update({"G": ["###", "#..", "#.#", "#.#", "###"], "X": ["#.#", "#.#", ".#.", "#.#", "#.#"],
                 "N": ["#.#", "###", "###", "###", "#.#"], "B": ["##.", "#.#", "##.", "#.#", "##."]})


def hobby_town():
    """Hobby Town: an evening street - a big ARCADE sign ringed with bulbs on a building, lit shop windows on the
    neighbours, stars over the roofs, a street lamp, light spilling on the pavement."""
    c = C(0)
    c.dither(0, 0, W - 1, 9, 1, 2, 0.5)                            # evening sky
    for x, y in ((2, 1), (13, 0), (29, 2), (21, 1)):
        c.set(x, y, 0)
    c.rect(0, 5, 6, 18, 2)                                         # left shop
    c.rect(0, 5, 6, 5, 3)
    for y in (7, 11):
        c.rect(1, y, 2, y + 1, 0)
        c.rect(4, y, 5, y + 1, 0)
    c.rect(25, 3, 31, 18, 2)                                       # right shop
    c.rect(25, 3, 31, 3, 3)
    for y in (5, 9):
        c.rect(26, y, 27, y + 1, 0)
        c.rect(29, y, 30, y + 1, 1)
    ra.awning(c, 25, 31, 13)
    c.rect(7, 9, 24, 18, 3)                                        # the arcade
    c.rect(2, 1, 29, 9, 3)                                         # sign board
    for x in range(3, 29, 2):                                      # bulbs
        c.set(x, 2, 1)
        c.set(x + 1, 8, 1)
    ra.text3(c, 5, 3, "ARCADE", 0)
    c.rect(12, 10, 19, 10, 2)
    c.rect(9, 12, 13, 16, 1)                                       # windows / doors
    c.rect(18, 12, 22, 16, 1)
    c.rect(14, 12, 17, 18, 0)
    c.rect(15, 12, 16, 18, 1)
    c.set(15, 15, 3)
    c.rect(0, 19, W - 1, 19, 3)
    c.rect(0, 20, W - 1, H - 1, 2)                                 # pavement with light spill
    c.poly([(13, 20), (18, 20), (21, 24), (10, 24)], 1)
    ra.lamp_post(c, 4, 13, 23)
    c.set(4, 14, 0)
    c.set(3, 15, 1)
    c.set(5, 15, 1)
    return c


def beach():
    """Beach: a big wave rearing up and curling over with a white foam crest, the open sea to a far rock, wet sand
    at the waterline, a striped beach umbrella and footprints in the sand, a sun."""
    c = C(0)
    c.disc(26, 3, 2.4, 0)
    for t in range(16):
        a = t * math.pi / 8
        c.set(26 + 3.4 * math.cos(a), 3 + 3.4 * math.sin(a), 1)
    c.rect(0, 9, W - 1, 9, 2)                                      # horizon
    c.rect(0, 10, W - 1, 15, 1)
    c.poly([(27, 9), (28, 7), (30, 7), (31, 9)], 2)                # far rock
    for y, x0, x1 in ((11, 22, 26), (13, 25, 30), (14, 19, 22)):
        c.rect(x0, y, x1, y, 0)
    body = [(0, 16), (0, 8), (2, 5), (5, 3), (9, 2), (13, 3), (16, 5), (17.5, 8), (17, 11), (15, 9.5), (13, 8.5),
            (11, 9), (10, 11), (11, 14), (14, 16)]
    c.poly(body, 1)                                                # the wave's body
    for a, b in zip(body[1:9], body[2:10]):
        c.line(a[0], a[1], b[0], b[1], 3)                          # its outer edge
    c.poly([(17, 11), (15, 9.5), (13, 8.5), (11, 9), (10, 11), (11, 14), (14, 16), (19, 16), (19, 12)], 2)   # tube
    c.poly([(14, 11), (12, 11), (12, 14), (15, 15), (16, 13)], 3)
    for pts in (((1, 14), (2, 9), (5, 6)), ((4, 15), (5, 10), (8, 7))):    # streaks on the face
        for a, b in zip(pts, pts[1:]):
            c.line(a[0], a[1], b[0], b[1], 2)
    for a, b in (((3, 6), (9, 4)), ((9, 4), (14, 5)), ((14, 5), (16, 8)), ((16, 8), (16, 10))):
        c.line(a[0], a[1], b[0], b[1], 0)                          # foam along the crest and lip
    for x, y in ((17, 12), (18, 13), (19, 4), (18, 2), (12, 1), (20, 7)):
        c.set(x, y, 0)                                             # spray
    for x in range(0, 22):                                         # foam where it breaks
        c.set(x, 16, 0 if x % 3 else 1)
    c.rect(0, 17, W - 1, 17, 1)                                    # wet sand with foam scallops
    for x in range(22, W, 4):
        c.rect(x, 16, x + 2, 16, 0)
    c.rect(0, 18, W - 1, H - 1, 0)                                 # dry sand
    c.rect(0, 18, W - 1, 18, 1)
    for x, y in ((3, 20), (9, 22), (12, 19), (30, 22), (25, 23)):
        c.set(x, y, 1)
    for k in range(5):                                             # footprints
        c.set(10 + k * 2, 23 - k, 2)
        c.set(11 + k * 2, 22 - k + (k % 2), 2)
    for k in range(11):                                            # umbrella
        c.rect(20 + k, 11 + abs(5 - k) // 2, 20 + k, 13, 3 if (k // 2) % 2 else 0)
        c.set(20 + k, 10 + abs(5 - k) // 2, 3)
    c.rect(19, 14, 31, 14, 3)
    c.line(25, 12, 27, 21, 3)
    c.rect(20, 20, 30, 21, 2)                                      # towel
    c.rect(20, 21, 30, 21, 3)
    return c


def freight_yard():
    """Freight Yard: stacks of ribbed freight containers, two smokestacks trailing smoke, a lattice power pylon
    with its cable, two rail tracks in front with a container wagon on the far one."""
    c = C(0)
    for x in range(0, 27):                                         # power cable
        c.set(x, 3 + 2.5 * math.sin(math.pi * x / 27.0), 3)
    c.line(27, 4, 31, 3, 3)
    for x0, top in ((1, 5), (5, 7)):                               # smokestacks
        c.rect(x0, top, x0 + 2, 13, 1)
        c.rect(x0, top, x0 + 2, top, 2)
        c.set(x0 + 1, top + 3, 2)
    c.disc(3, 3, 1.6, 1)                                           # smoke drifting right
    c.disc(6, 2, 1.8, 1)
    c.disc(10, 1, 2, 1)
    c.disc(8, 5, 1.3, 1)
    pylon(c, 27, 3, 14)
    c.rect(0, 13, W - 1, 13, 2)
    c.rect(0, 14, W - 1, H - 1, 1)
    container(c, 8, 9, 15, 13, 1)                                  # container stacks
    container(c, 15, 9, 22, 13, 2)
    container(c, 11, 5, 18, 9, 2)
    container(c, 18, 5, 22, 9, 1)
    for y in (17, 22):                                             # two tracks
        c.rect(0, y - 1, W - 1, y - 1, 2)
        c.rect(0, y, W - 1, y, 3)
        for x in range(1, W, 3):
            c.set(x, y + 1, 2)
    container(c, 1, 12, 14, 15, 2)                                 # wagon with a container
    c.rect(0, 15, 15, 15, 3)
    for x in (2, 4, 11, 13):
        c.set(x, 16, 3)
    c.rect(16, 15, 17, 15, 3)
    for x, y in ((4, 20), (12, 19), (22, 20), (9, 23)):
        c.rect(x, y, x + 1, y, 2)
    return c


def volcano_peak():
    """Volcano Peak: close under an erupting summit - a fountain of glowing lava blobs from the crater, a dark ash
    plume, lava streams running down black rock, glowing cracks in the jagged rocks in front."""
    c = C(1)                                                       # ash-grey sky
    for x, y, r in ((10, 3, 3.2), (15, 1, 3.6), (21, 2, 3.2), (26, 4, 2.6), (5, 5, 2.2)):   # ash plume
        c.disc(x, y, r, 2)
    for x, y in ((13, 4), (18, 4), (24, 5), (8, 6), (27, 6)):
        c.rect(x, y, x + 1, y, 3)
    for x, y in ((9, 1), (14, 0), (20, 0), (25, 2)):
        c.set(x, y, 1)
    c.poly([(0, 24), (0, 17), (8, 9), (12, 8), (20, 8), (24, 10), (32, 15), (32, 24)], 3)   # the peak
    c.rect(11, 8, 21, 8, 0)                                        # crater glow
    c.rect(12, 7, 20, 7, 1)
    c.rect(15, 5, 17, 6, 0)                                        # lava fountain
    for x, y in ((16, 3), (13, 4), (19, 4), (11, 6), (21, 6), (16, 1), (23, 8), (9, 7), (14, 2), (18, 2)):
        c.set(x, y, 0)
    for pts in (((13, 9), (11, 12), (12, 15), (8, 19), (7, 24)), ((19, 9), (21, 13), (19, 17), (22, 21), (21, 24))):
        for a, b in zip(pts, pts[1:]):                             # lava streams
            c.line(a[0], a[1], b[0], b[1], 0)
            c.line(a[0] + 1, a[1], b[0] + 1, b[1], 1)
    for a, b in (((3, 15), (6, 18)), ((26, 13), (29, 17)), ((16, 12), (16, 15))):
        c.line(a[0], a[1], b[0], b[1], 2)
    c.poly([(0, 24), (0, 19), (3, 17), (6, 21), (9, 20), (12, 24)], 2)     # jagged rocks in front
    c.poly([(23, 24), (25, 19), (28, 18), (32, 20), (32, 24)], 2)
    c.line(0, 19, 3, 17, 3)
    c.line(3, 17, 6, 21, 3)
    c.line(25, 19, 28, 18, 3)
    c.line(2, 20, 4, 23, 0)                                        # glowing cracks
    c.line(27, 20, 29, 23, 0)
    return c


def safari_grounds():
    """Safari Grounds: a thatched rest house under a REST sign, a pond with reeds, savanna grass running to a far
    line of trees, a big boulder and tall grass in front."""
    c = C(0)
    ra.cloud(c, 20, 2, 8)
    for x in range(0, W + 2, 4):                                   # far tree line
        c.rect(x, 9, x + 2, 9, 1)
        c.rect(x - 1, 10, x + 3, 10, 1)
    c.rect(0, 11, W - 1, 11, 2)
    c.rect(0, 12, W - 1, H - 1, 1)
    c.rect(1, 10, 12, 17, 0)                                       # rest house
    c.rect(1, 10, 1, 17, 3)
    c.rect(12, 10, 12, 17, 3)
    for i in range(3):                                             # thatched roof
        c.rect(-1 + i, 9 - i, 14 - i, 9 - i, 2)
        for x in range(-1 + i + (i % 2), 15 - i, 2):
            c.set(x, 9 - i, 3)
    c.rect(-1, 9, 14, 9, 3)
    c.rect(-1, 0, 16, 6, 3)                                        # REST sign
    ra.text3(c, 0, 1, "REST", 0)
    c.rect(8, 12, 10, 17, 3)                                       # door
    c.rect(3, 12, 5, 13, 2)                                        # window
    c.poly([(16, 15), (21, 13), (29, 13), (32, 15), (32, 18), (24, 19), (17, 18)], 2)    # pond
    c.rect(20, 15, 24, 15, 0)
    c.rect(26, 16, 29, 16, 1)
    c.line(17, 18, 24, 19, 3)
    for x, top in ((16, 11), (18, 12), (31, 10), (29, 11)):        # reeds
        c.rect(x, top, x, top + 4, 3)
        c.rect(x, top, x, top + 1, 2)
    ra.boulder(c, 13, 22, 3)
    tall_grass(c, 0, 20, 8, 23)
    tall_grass(c, 18, 21, 31, 23)
    return c


def mountains():
    """Mountains: a high range - one great central peak with a snow cap, lit and shadowed faces split by a ridge,
    rock strata, a wisp of cloud, hazy peaks behind, dark lower peaks and a black rocky ridge in front, a bird."""
    c = C(0)
    c.poly([(0, 15), (4, 7), (8, 11), (11, 8), (14, 15)], 9)       # hazy peaks behind (dithered)
    c.poly([(18, 15), (24, 6), (27, 9), (31, 5), (32, 6), (32, 15)], 9)
    for y in range(H):
        for x in range(W):
            if c.p[y][x] == 9:
                c.p[y][x] = 1 if (x + y) % 2 else 0
    c.poly([(2, 22), (16, 1), (30, 22)], 1)                        # great peak, lit face
    c.poly([(16, 1), (30, 22), (20, 22), (17, 15), (19, 8)], 2)    # shadow face
    c.poly([(16, 1), (20, 7), (19, 8), (18, 7), (17, 8), (15, 6), (13, 8), (11, 7), (15.5, 1)], 0)  # snow cap
    c.poly([(16.5, 1), (20, 7), (21, 7.5), (18, 2)], 1)
    for a, b in (((16, 1), (19, 8)), ((19, 8), (17, 15)), ((17, 15), (20, 22)),     # ridge
                 ((16, 1), (2, 22)), ((16, 1), (30, 22))):                           # outline
        c.line(a[0], a[1], b[0], b[1], 3)
    for a, b, v in (((10, 10), (8, 13), 2), ((13, 10), (12, 12), 2), ((23, 11), (25, 15), 3),
                    ((22, 16), (24, 19), 3), ((26, 16), (27, 18), 3), ((14, 15), (13, 18), 2)):
        c.line(a[0], a[1], b[0], b[1], v)                          # rock strata
    for x0, x1, y in ((1, 9, 12), (3, 11, 13)):                    # cloud wisp
        c.rect(x0, y, x1, y, 0)
    c.rect(4, 14, 10, 14, 1)
    c.poly([(0, 24), (0, 17), (5, 14), (11, 20), (13, 24)], 2)     # lower dark peaks
    c.poly([(20, 24), (25, 17), (29, 14), (32, 17), (32, 24)], 2)
    for a, b in (((0, 17), (5, 14)), ((5, 14), (11, 20)), ((25, 17), (29, 14)), ((29, 14), (32, 17))):
        c.line(a[0], a[1], b[0], b[1], 3)
    c.line(5, 15, 3, 19, 1)
    c.line(29, 15, 27, 18, 1)
    c.poly([(0, 24), (0, 21), (6, 20), (12, 22), (18, 20), (24, 22), (32, 20), (32, 24)], 3)   # near ridge
    c.line(12, 23, 18, 21, 1)                                      # path over the ridge
    ra.gsc_bird(c, 25, 3)
    return c


def island_shores():
    """Island Shores: a sea dotted with small islands - a far volcanic isle, a middle island with a palm and a hut,
    a ferry sailing between - seen from a rocky shore with a strip of sand, gulls overhead."""
    c = C(0)
    c.rect(0, 9, W - 1, 9, 2)                                      # horizon
    c.rect(0, 10, W - 1, H - 1, 1)
    c.poly([(19, 9), (23, 4), (25, 4), (30, 9)], 1)                # far volcanic isle
    c.rect(23, 3, 24, 3, 1)
    c.poly([(0, 9), (2, 7), (6, 8), (8, 9)], 1)
    c.poly([(2, 15), (6, 12), (14, 11), (18, 15)], 2)              # middle island
    c.line(2, 15, 18, 15, 3)
    ra.palm(c, 7, 12, 6, 1)
    c.rect(12, 9, 15, 11, 0)                                       # hut
    c.rect(11, 8, 16, 8, 3)
    c.rect(12, 7, 15, 7, 3)
    c.rect(14, 10, 14, 11, 3)
    c.rect(21, 13, 28, 13, 0)                                      # ferry
    c.rect(21, 14, 29, 15, 3)
    c.rect(23, 11, 26, 12, 0)
    c.rect(23, 11, 26, 11, 3)
    c.rect(24, 10, 24, 10, 3)
    c.set(23, 14, 0)
    c.set(26, 14, 0)
    for y, x0, x1 in ((17, 3, 7), (18, 20, 24), (20, 11, 14), (11, 28, 30)):
        c.rect(x0, y, x1, y, 0)
    c.poly([(14, 24), (18, 21), (25, 19), (32, 18), (32, 24)], 0)  # sand
    c.poly([(24, 24), (25, 20), (28, 18), (32, 17), (32, 24)], 3)  # shore rocks
    c.rect(26, 20, 27, 20, 2)
    c.set(29, 18, 2)
    c.line(14, 24, 18, 21, 2)
    c.line(18, 21, 25, 19, 2)
    ra.gsc_bird(c, 9, 3)
    ra.gsc_bird(c, 16, 5)
    return c


def twilight_grove():
    """Twilight Grove: a cold grove under a big full moon - white birch trunks against a dark sky, bare branches,
    snow falling, snowy ground with long shadows."""
    c = C(0)
    c.rect(0, 0, W - 1, 17, 2)                                     # dusk sky, darker above
    c.dither(0, 0, W - 1, 4, 2, 3, 0.5)
    c.dither(0, 13, W - 1, 17, 2, 1, 0.4)
    c.disc(16, 8, 4.5, 0)                                          # full moon
    c.disc(14.5, 7, 1, 1)
    c.disc(18, 9.5, 1.2, 1)
    c.set(16, 5, 1)
    for x in (3, 12, 25):                                          # far trunks
        c.rect(x, 2, x, 17, 1)
    c.rect(0, 18, W - 1, H - 1, 0)                                 # snowy ground
    c.dither(0, 18, W - 1, 18, 0, 1, 0.5)
    for x0, x1, y in ((9, 15, 21), (24, 31, 20), (0, 6, 22)):      # long shadows
        c.rect(x0, y, x1, y, 1)
    for x, top, bot, w in ((6, 0, 21, 3), (21, 0, 20, 3), (29, 5, 19, 2)):   # near birches
        for y in range(top, bot):
            c.rect(x, y, x + w - 1, y, 0)
            c.set(x + w - 1, y, 1)                                 # shaded side
            c.set(x - 1, y, 3)
            c.set(x + w, y, 3)
        for i, y in enumerate(range(top + 2, bot - 1, 4)):         # black bark dashes, alternating sides
            if i % 2:
                c.rect(x + w - 2, y, x + w - 1, y, 3)
            else:
                c.rect(x, y, x + (1 if w > 2 else 0), y, 3)
                c.set(x, y + 1, 3)
        c.set(x - 2, bot - 1, 3)
        c.set(x + w + 1, bot - 1, 3)
    c.line(9, 6, 13, 2, 3)                                         # bare branches
    c.line(5, 9, 2, 6, 3)
    c.line(20, 5, 17, 1, 3)
    c.line(24, 9, 27, 5, 3)
    c.line(11, 4, 12, 5, 3)
    c.line(2, 6, 1, 4, 3)
    for x, y in ((2, 2), (11, 10), (26, 2), (31, 11), (1, 13), (14, 15), (24, 14), (4, 16), (19, 16), (10, 1)):
        c.set(x, y, 0)                                             # snowflakes
    return c


FRLG = {"field": field, "forest": forest, "rocky_foothills": rocky_foothills, "riverside": riverside,
        "underground": underground, "residential_zone": residential_zone, "hobby_town": hobby_town, "beach": beach,
        "freight_yard": freight_yard, "volcano_peak": volcano_peak, "safari_grounds": safari_grounds,
        "mountains": mountains, "island_shores": island_shores, "twilight_grove": twilight_grove}
ra.COURSES.update(FRLG)


def write_frlg(out=ra.OUT):
    os.makedirs(out, exist_ok=True)
    for name, fn in FRLG.items():
        c = fn()
        path = os.path.join(out, f"{name}.txt")
        if os.path.exists(path) and open(path).read().startswith("; hand-edited"):
            print("kept (hand-edited)", path)
            continue
        with open(path, "w") as f:
            f.write(f"; {name}: 32x24, . white  - light grey  + dark grey  # black (tools/route_art_frlg.py)\n")
            for row in c.p:
                f.write("".join(ra.CH[v] for v in row) + "\n")
        print("->", path)


if __name__ == "__main__":
    write_frlg()
    ra.preview_names(os.path.join(ra.ROOT, "art_src", "route_pictures_frlg.png"), list(FRLG))
