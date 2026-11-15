#!/usr/bin/env python3
"""draw_icons.py - draws the WalkerBox menu icons and writes them as ASCII
art into art/icons/. The ASCII files stay the source of truth for
tools/gen_art.py; rerun this script to regenerate the files it owns
(every drawn icon except more_message, more_messages_mask and contrast).

  python3 tools/draw_icons.py            # write the files
  python3 tools/draw_icons.py --print    # print them instead

Style rules for the whole set (all WalkerBox original art):
  * one light source, top-left;
  * 1-px black outline;
  * round things are shaded like spheres: white specular spot top-left,
    light-grey body, dark-grey crescent along the bottom-right inside edge;
  * boxy things get a bevel (light top/left inner edge, dark bottom/right)
    or a 3/4 view with a light top face and a dark side face;
  * objects that sit on the screen cast a 1-px dark-grey drop shadow to the
    bottom-right.
Shades: '.' 0 white, '-' 1 light grey, '+' 2 dark grey, '#' 3 black.

Geometry: shapes are sets of pixels whose CENTRES lie inside the ideal
ellipse / rounded rectangle / polygon (pixel-edge coordinates, so a 16-px
slot is centred on 8.0 and shapes stay symmetric). The outline is the
boundary of the shape, thinned to an 8-connected 1-px line; the pixels
removed at the step corners become light-grey anti-alias pixels.

Sprite letters (hand-placed parts): '.', '-', '+', '#' are literal shades;
'o' = bevelled light surface (light grey, white top-left edge, dark
bottom-right edge); 'O' = bevelled dark surface (dark grey, light grey
top-left edge); 'w' = bevelled white surface (white, light grey
bottom-right edge); ' ' = leave the pixel as it is.
"""
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICONS = os.path.join(ROOT, "art", "icons")
CH = ".-+#"
BEVEL = {"o": (1, 0, 2), "O": (2, 1, 2), "w": (0, 0, 1)}   # mid, hi, lo

# ------------------------------------------------------------- shape tests

def ellipse(cx, cy, rx, ry=None):
    ry = rx if ry is None else ry
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0


def rrect(x0, y0, x1, y1, r):
    """Rounded rectangle covering pixel columns x0..x1 and rows y0..y1."""
    X0, Y0, X1, Y1 = x0, y0, x1 + 1, y1 + 1
    def f(x, y):
        if not (X0 <= x <= X1 and Y0 <= y <= Y1):
            return False
        cx = min(max(x, X0 + r), X1 - r)
        cy = min(max(y, Y0 + r), Y1 - r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    return f


def rect(x0, y0, x1, y1):
    return lambda x, y: x0 <= x <= x1 + 1 and y0 <= y <= y1 + 1


def polygon(pts):
    def f(x, y):
        inside = False
        n = len(pts)
        for i in range(n):
            (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
            if (ay > y) != (by > y):
                if x < ax + (y - ay) * (bx - ax) / (by - ay):
                    inside = not inside
        return inside
    return f


def union(*fs):
    return lambda x, y: any(f(x, y) for f in fs)


def minus(a, b):
    return lambda x, y: a(x, y) and not b(x, y)

# ------------------------------------------------------------------ canvas

class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.p = [[0] * w for _ in range(h)]

    def inb(self, x, y):
        return 0 <= x < self.w and 0 <= y < self.h

    def mask(self, f):
        return {(x, y) for y in range(self.h) for x in range(self.w) if f(x + 0.5, y + 0.5)}

    def coverage(self, f, x, y, n=4):
        c = 0
        for j in range(n):
            for i in range(n):
                c += f(x + (i + 0.5) / n, y + (j + 0.5) / n)
        return c / (n * n)

    def shape(self, f, fill=0, outline=3, aa=True, thin=True):
        """Fill shape f, draw its 1-px outline; returns (mask, edge)."""
        m = self.mask(f)
        edge = {(x, y) for (x, y) in m
                if any((x + dx, y + dy) not in m for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))}
        corners = set()
        if thin:
            for (x, y) in edge:
                for dx in (-1, 1):
                    for dy in (-1, 1):
                        if ((x + dx, y) in edge and (x, y + dy) in edge
                                and (x - dx, y - dy) not in m and (x - dx, y) not in m
                                and (x, y - dy) not in m):
                            corners.add((x, y))
        m = m - corners
        edge = edge - corners
        if fill is not None:
            for (x, y) in m - edge:
                self.p[y][x] = fill
        for (x, y) in edge:
            self.p[y][x] = outline
        for (x, y) in corners:      # keep what is underneath; AA only on white
            if aa and self.p[y][x] == 0 and self.coverage(f, x, y) >= 0.3:
                self.p[y][x] = 1
        return m, edge

    def sphere(self, m, edge, cx, cy, rx, ry=None, mid=1, hi=0, lo=2, spec=0.24, crescent=0.32):
        """Shade the inside of a round shape like a ball lit from top-left."""
        ry = rx if ry is None else ry
        for (x, y) in m - edge:
            nx = (x + 0.5 - cx) / rx
            ny = (y + 0.5 - cy) / ry
            t = (nx + ny) / math.sqrt(2)          # + towards bottom-right
            r = math.hypot(nx, ny)
            sx, sy = nx + 0.42, ny + 0.42         # specular spot top-left
            if math.hypot(sx, sy) < spec:
                v = hi
            elif t > crescent and r > 0.55:
                v = lo
            else:
                v = mid
            self.p[y][x] = v

    def bevel(self, m, edge, mid=1, hi=0, lo=2):
        """Shade the inside of any shape: light top-left rim, dark bottom-right rim."""
        inner = m - edge
        for (x, y) in inner:
            if ((x + 1, y) not in inner or (x, y + 1) not in inner or (x + 1, y + 1) not in inner):
                v = lo
            elif (x - 1, y) not in inner or (x, y - 1) not in inner:
                v = hi
            else:
                v = mid
            self.p[y][x] = v

    def fill(self, f, shade):
        for (x, y) in self.mask(f):
            self.p[y][x] = shade

    def line(self, x0, y0, x1, y1, shade=3):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            if self.inb(x0, y0):
                self.p[y0][x0] = shade
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def sprite(self, rows, x0, y0):
        cls = {}
        for j, r in enumerate(rows):
            for i, ch in enumerate(r):
                if ch == " ":
                    continue
                x, y = x0 + i, y0 + j
                if ch in BEVEL:
                    cls[(x, y)] = ch
                else:
                    self.p[y][x] = CH.index(ch)
        for (x, y), ch in cls.items():
            mid, hi, lo = BEVEL[ch]
            same = lambda a, b: cls.get((a, b)) == ch
            if not same(x + 1, y) or not same(x, y + 1) or not same(x + 1, y + 1):
                v = lo
            elif not same(x - 1, y) or not same(x, y - 1):
                v = hi
            else:
                v = mid
            self.p[y][x] = v

    def exterior(self):
        """White pixels 4-connected to the canvas border (outside the objects)."""
        out = set()
        stack = [(x, y) for x in range(self.w) for y in (0, self.h - 1)]
        stack += [(x, y) for y in range(self.h) for x in (0, self.w - 1)]
        while stack:
            x, y = stack.pop()
            if not self.inb(x, y) or (x, y) in out or self.p[y][x] != 0:
                continue
            out.add((x, y))
            stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
        return out

    def drop_shadow(self, shade=2):
        """1-px shadow to the bottom-right, only on the outside white."""
        ext = self.exterior()
        snap = [row[:] for row in self.p]
        for y in range(self.h):
            for x in range(self.w):
                if snap[y][x] != 0 and (x + 1, y + 1) in ext and snap[y + 1][x + 1] == 0:
                    self.p[y + 1][x + 1] = shade

    def mirror(self):
        for row in self.p:
            row.reverse()

    def rows(self):
        return ["".join(CH[v] for v in row) for row in self.p]


def sprite_icon(w, h, rows, x0=0, y0=0, shadow=True):
    c = Canvas(w, h)
    c.sprite(rows, x0, y0)
    if shadow:
        c.drop_shadow()
    return c

# ------------------------------------------------------------- 8x8 icons

def ball():
    return sprite_icon(8, 8, [
        "..####..",
        ".#-+++#.",
        "#-+##++#",
        "###..###",
        "#..##..#",
        "#.....-#",
        ".#..--#.",
        "..####..",
    ], shadow=False)


def ball_light():
    return sprite_icon(8, 8, [
        "..++++..",
        ".+.---+.",
        "+.-++--+",
        "+++..+++",
        "+..++..+",
        "+......+",
        ".+...-+.",
        "..++++..",
    ], shadow=False)


def item():
    return sprite_icon(8, 8, [
        "...##...",
        "..#..#..",
        ".######.",
        "#OOOOOO#",
        "#OO##OO#",
        "#OOOOOO#",
        "#OOOOOO#",
        ".######.",
    ], shadow=False)


def item_light():
    return sprite_icon(8, 8, [
        "...++...",
        "..+..+..",
        ".++++++.",
        "+oooooo+",
        "+oo++oo+",
        "+oooooo+",
        "+oooooo+",
        ".++++++.",
    ], shadow=False)


def map_small():
    return sprite_icon(8, 8, [
        ".#.##.#.",
        "#-#..#-#",
        "#-#.+#-#",
        "#-#++#-#",
        "#+#..#-#",
        "#-#..#+#",
        "#-#..#-#",
        ".#.##.#.",
    ], shadow=False)


SUITS = {
    "suit_heart": ["........", ".##.##..", "#-#####.", "#######.", ".#####..", "..###...", "...#....", "........"],
    "suit_spade": ["...#....", "..###...", ".#-###..", "#######.", "#######.", ".#.#.#..", "...#....", "..###..."],
    "suit_diamond": ["...#....", "..#-#...", ".#####..", "#######.", ".#####..", "..###...", "...#....", "........"],
    "suit_club": ["..###...", "..#-#...", "##.#.##.", "#######.", "##.#.##.", "...#....", "..###...", "........"],
}

ARROW_UP = ["........", "...##...", "..####..", ".######.", "...##...", "...##...", "...##...", "........"]


def rot90(rows):
    """Rotate a square sprite 90 degrees counter-clockwise (up -> left)."""
    n = len(rows)
    return ["".join(rows[y][n - 1 - x] for y in range(n)) for x in range(n)]


def flipv(rows):
    return rows[::-1]


def fliph(rows):
    return [r[::-1] for r in rows]


def shift(rows, dx, dy):
    n = len(rows)
    out = []
    for y in range(n):
        r = ""
        for x in range(n):
            sx, sy = x - dx, y - dy
            r += rows[sy][sx] if 0 <= sx < n and 0 <= sy < n else "."
        out.append(r)
    return out


def invert(rows):
    return ["".join("#" if ch == "." else "." for ch in r) for r in rows]


def arrows():
    base = {"up": ARROW_UP, "down": flipv(ARROW_UP), "left": rot90(ARROW_UP)}
    base["right"] = fliph(base["left"])
    step = {"up": (0, -1), "down": (0, 1), "left": (-1, 0), "right": (1, 0)}
    out = {}
    for d, rows in base.items():
        out["arrow_" + d] = sprite_icon(8, 8, rows)
        out["arrow_" + d + "_offset"] = sprite_icon(8, 8, shift(rows, *step[d]))
        out["arrow_" + d + "_invert"] = sprite_icon(8, 8, invert(rows), shadow=False)
    return out


def vial():
    return sprite_icon(8, 8, [
        "..####..",
        "...##...",
        "..#..#..",
        ".#.-..#.",
        ".#---+#.",
        ".#-.-+#.",
        ".#--++#.",
        "..####..",
    ], shadow=False)


def low_battery():
    return sprite_icon(8, 8, [
        "........",
        "#######.",
        "##....##",
        "##...-##",
        "##...-##",
        "#-----#.",
        "#######.",
        "........",
    ], shadow=False)


def hp_blip():
    return sprite_icon(8, 8, [
        "........",
        "######..",
        "#OOOO#..",
        "#OOOO#..",
        "#OOOO#..",
        "#OOOO#..",
        "######..",
        "........",
    ])


def catch_star():
    return sprite_icon(8, 8, [
        "...#....",
        "..###...",
        "##-###..",
        ".#####..",
        "..###...",
        ".##.##..",
        "#.....#.",
        "........",
    ])


def music_note():
    return sprite_icon(8, 8, [
        "...##...",
        "...#.#..",
        "...#..#.",
        "...#....",
        "...#....",
        ".###....",
        "#-##....",
        ".##.....",
    ], shadow=False)

# ------------------------------------------------------------ 8x16 icons

def menu_arrow(right):
    rows = ["........", "........", "........", ".....#..", "....##..", "...###..", "..####..",
            ".#####..", "..####..", "...###..", "....##..", ".....#..", "........", "........",
            "........", "........"]
    if right:
        rows = [r[::-1] for r in rows]
        rows = [r[1:] + "." for r in rows]       # keep one column free for the shadow
    return sprite_icon(8, 16, rows)


def menu_return():
    return sprite_icon(8, 16, [
        "........", "........", "........", "........",
        "..#####.", ".......#", ".......#", "..#....#",
        ".##....#", "#######.", ".##.....", "..#.....",
        "........", "........", "........", "........"])


def ir_arcs():
    c = Canvas(8, 16)
    c.fill(rect(0, 7, 1, 8), 3)
    for r in (3.6, 6.2):
        ring = minus(ellipse(1, 8, r), ellipse(1, 8, r - 1.15))
        for (x, y) in c.mask(ring):
            if 2 <= x <= 6:
                c.p[y][x] = 3
    return c                  # thin lines: no shadow (it would double them)

# ----------------------------------------------------------- 16x16 icons

def watt():
    """A coin with a lightning bolt: dark-grey disc, one small light-grey
    highlight cluster top-left on the rim, a white bolt with a black outline."""
    # disc on columns/rows 1..14 (centre 7.5 in pixel indices); the 6x10
    # bolt below sits on columns 5..10 and rows 3..12, the same centre
    c = Canvas(16, 16)
    m, e = c.shape(ellipse(8, 8, 7.0), fill=2, aa=False)
    # one highlight cluster: the 3 inside-rim pixels nearest the top-left
    inner = m - e
    rim = [(x, y) for (x, y) in inner
           if any((x + dx, y + dy) in e for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))]
    rim.sort(key=lambda p: (p[0] + p[1], abs(p[0] - p[1])))
    for (x, y) in rim[:3]:
        c.p[y][x] = 1
    c.sprite([                                   # ' ' = keep the disc
        "    ####",
        "   #..# ",
        "   #.#  ",
        "  #..###",
        "  #....#",
        "  ###.# ",
        "    #.# ",
        "   #.#  ",
        "   ##   ",
        "   #    ",
    ], 3, 3)
    c.drop_shadow()
    return c


# ---- radar / dowsing menu icons: three candidates each (R1-R3, D1-D3);
# ---- the slots use R1 and D1, tools/draw_icons.py --options renders all six

def _blades(c, blades, base):
    """Soft curled grass blades drawn back to front: (bx, w0, tx, ty, bend, fill)."""
    for (bx, w0, tx, ty, bend, fill) in blades:
        mm, ee = c.shape(soft_blade(bx, base, w0, tx, ty, bend, 0.9), fill=fill, aa=False)
        inner = mm - ee
        for (x, y) in inner:
            if fill > 0 and (x - 1, y) not in inner and (x + 1, y) in inner and (x + 2, y) in inner:
                c.p[y][x] = fill - 1
    for x in range(c.w):
        if c.p[base - 1][x] != 0:
            c.p[base - 1][x] = 3


def _critter_head(c, cx, cy, rx, ry, ear_h):
    """Generic round critter head with two pointed ears and two eyes."""
    for s in (-1, 1):
        ex = cx + s * rx * 0.62
        c.shape(polygon([(ex - s * 1.6, cy - ry * 0.55), (ex + s * 0.6, cy - ry - ear_h),
                         (ex + s * 2.0, cy - ry * 0.25)]), fill=1, aa=False)
    m, e = c.shape(ellipse(cx, cy, rx, ry), fill=0)
    c.sphere(m, e, cx, cy, rx, ry, mid=0, hi=0, lo=1)
    for s in (-1, 1):                                  # eyes: 1x2 black
        x = int(math.floor(cx + s * rx * 0.42))
        if s > 0:
            x = int(math.floor(cx + s * rx * 0.42 - 0.01))
        c.p[int(cy - 1)][x] = 3
        c.p[int(cy)][x] = 3


def radar_r1():
    """R1: a friendly critter face (pointed ears, eyes, cheeks, mouth)
    peeking over the top of a tuft of tall grass in front of it."""
    return sprite_icon(16, 16, [
        "...#........#...",
        "...##......##...",
        "...#.######.#...",
        "..#..........#..",
        "..#..#....#..#..",
        "..#..#....#..#..",
        "..#..........#..",
        "..#-...##...-#..",
        "..#..#.##.#..#..",
        ".#-##-#--#-##-#.",
        "#--#--#--#--#--#",
        "#-------------+#",
        "#-------------+#",
        "#++++++++++++++#",
        "################",
        "................",
    ])


def radar_r2():
    """R2: round badge: white sky, the critter's head (light) rising out of
    dark tall grass that fills the bottom of the badge."""
    c = Canvas(16, 16)
    m, e = c.shape(ellipse(8, 8, 7.0), fill=0, aa=False)
    inner = m - e
    spr = [
        "    #  #    ",
        "   #-##-#   ",
        "  #------#  ",
        "  #-#--#-#  ",
        "  #-#--#-#  ",
        "# #  ##  # #",
        "#+#+#++#+#+#",
        "#++#++++#++#",
        "#++++++++++#",
        "#++++++++++#",
        "############",
    ]
    for j, r in enumerate(spr):
        for i, ch in enumerate(r):
            x, y = 2 + i, 3 + j
            if ch != " " and (x, y) in inner:
                c.p[y][x] = CH.index(ch)
    c.drop_shadow()
    return c

def radar_r3():
    """R3: a dense tuft of dark tall grass with a dark gap in the middle and
    two white eyes looking out of it."""
    c = Canvas(16, 16)
    _blades(c, [(2.0, 5.0, 0.8, 4.0, -0.8, 2), (14.0, 5.0, 15.2, 4.0, 0.8, 2),
                (5.0, 5.5, 4.2, 1.5, -0.6, 2), (11.0, 5.5, 11.8, 1.5, 0.6, 2),
                (8.0, 5.5, 8.0, 3.0, 0.0, 2)], 15)
    c.fill(rrect(4, 6, 11, 10, 2.0), 3)            # the dark gap
    for (x, y) in ((5, 7), (6, 7), (5, 8), (6, 8), (9, 7), (10, 7), (9, 8), (10, 8)):
        c.p[y][x] = 0                              # eyes
    c.p[8][6] = 1
    c.p[8][10] = 1
    c.drop_shadow()
    return c


def _arcs(c, cx, cy, radii_shades, keep):
    """Crisp 1-px arcs: the outline pixels of circles, filtered by keep(x, y)."""
    for r, shade in radii_shades:
        m = c.mask(ellipse(cx, cy, r))
        for (x, y) in m:
            if any((x + dx, y + dy) not in m for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                if keep(x, y) and c.p[y][x] == 0:
                    c.p[y][x] = shade


def dowsing_d1():
    """D1: a round handheld scanner (bottom left, with a dark screen)
    sending three crisp pulse arcs up and to the right."""
    c = Canvas(16, 16)
    m, e = c.shape(ellipse(4.0, 12.0, 3.6), fill=1)
    c.sphere(m, e, 4.0, 12.0, 3.6)
    c.fill(rect(3, 11, 4, 12), 3)                  # screen
    c.drop_shadow()                                # solid parts only
    _arcs(c, 4.0, 12.0, ((6.6, 3), (9.4, 2), (12.2, 1)), lambda x, y: x >= 4 and y <= 10)
    return c


def dowsing_d2():
    """D2: a forked dowsing rod held by its two arms, the stem pointing up,
    pulse arcs rising from its tip."""
    c = Canvas(16, 16)
    c.fill(rect(7, 6, 8, 10), 3)                   # stem
    c.line(7, 10, 3, 14)
    c.line(7, 11, 4, 14)
    c.line(8, 10, 12, 14)
    c.line(8, 11, 11, 14)
    c.drop_shadow()                                # solid parts only
    _arcs(c, 8.0, 6.5, ((2.6, 3), (4.5, 2), (6.4, 1)), lambda x, y: y + 0.5 < 5.6)
    return c


def dowsing_d3():
    """D3: a metal-detector wand: grip at the top left, shaft, a flat coil
    over the ground, pulse rings on the ground around it."""
    c = Canvas(16, 16)
    c.line(3, 3, 8, 8)                             # shaft
    c.line(4, 3, 9, 8)
    c.sprite(["###", "#-#", "###", " ##"], 0, 0)    # grip
    c.shape(ellipse(9.5, 10.0, 3.6, 1.6), fill=1, aa=False)   # coil
    c.drop_shadow()                                # solid parts only
    for (rx, ry, shade) in ((6.8, 2.3, 1), (5.0, 1.6, 2)):     # rings on the ground
        m = c.mask(ellipse(9.5, 13.4, rx, ry))
        for (x, y) in m:
            if (any((x + dx, y + dy) not in m for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
                    and c.p[y][x] == 0):
                c.p[y][x] = shade
    c.fill(rect(9, 13, 10, 13), 3)                 # the find
    return c

RADAR_OPTIONS = {"R1": radar_r1, "R2": radar_r2, "R3": radar_r3}
DOWSING_OPTIONS = {"D1": dowsing_d1, "D2": dowsing_d2, "D3": dowsing_d3}


def icon_radar():
    return radar_r1()


def icon_dowsing():
    return dowsing_d1()


def options_sheet(path):
    """Review sheet: every radar/dowsing option at 1x and 4x with its label."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import pwimg
    opts = list(RADAR_OPTIONS.items()) + list(DOWSING_OPTIONS.items())
    cell_w, cell_h = 16 * 4 + 16 + 30, 16 * 4 + 24
    W, H = cell_w * 3 + 10, cell_h * 2 + 10
    bg = (196, 206, 222)
    px = bytearray(bytes(bg) * (W * H))

    def put(x, y, g):
        o = (y * W + x) * 3
        px[o:o + 3] = bytes((g, g, g)) if isinstance(g, int) else bytes(g)

    for i, (label, fn) in enumerate(opts):
        rows = fn().rows()
        ox, oy = 10 + (i % 3) * cell_w, 10 + (i // 3) * cell_h
        for y in range(16):
            for x in range(16):
                g = pwimg.GREY[CH.index(rows[y][x])]
                put(ox + x, oy + 20 + y, g)              # 1x
                for dy in range(4):
                    for dx in range(4):
                        put(ox + 24 + x * 4 + dx, oy + y * 4 + dy, g)   # 4x
        tmp = bytearray(40 * 16)
        pwimg.draw_text_line(tmp, 40, 16, label, 0, 0)
        for y in range(16):
            for x in range(40):
                if tmp[y * 40 + x]:
                    put(ox + x, oy + 40 + y, (20, 30, 60))
    pwimg.write_png(path, W, H, [px[r * W * 3:(r + 1) * W * 3] for r in range(H)])
    print("wrote", path)


def icon_connect():
    return sprite_icon(16, 16, [
        "................",
        "................",
        "..........#.....",
        "..........##....",
        "..###########...",
        "..........##....",
        "..........#.....",
        "................",
        "................",
        ".....#..........",
        "....##..........",
        "...###########..",
        "....##..........",
        ".....#..........",
        "................",
        "................",
    ])


def icon_trainer_card():
    # white card face, black outline, photo box + black/grey text lines
    return sprite_icon(16, 16, [
        ".##############.",
        "#..............#",
        "#.####.........#",
        "#.#--#.#######.#",
        "#.#++#.........#",
        "#.####.#####...#",
        "#..............#",
        "#.++++++++++++.#",
        "#..............#",
        ".##############.",
    ], 0, 3)


def icon_inventory():
    c = Canvas(16, 16)
    c.sprite(ball().rows(), 0, 4)
    c.sprite([
        "######",
        "#oooo#",
        "######",
        "#oooo#",
        "#oooo#",
        "######",
    ], 9, 6)
    c.drop_shadow()
    return c


def icon_settings():
    return sprite_icon(16, 16, [
        "................",
        "......####......",
        "...##.#OO#.##...",
        "..#OO##OO##OO#..",
        "..#OOOOOOOOOO#..",
        "...#OOO##OOO#...",
        ".###OO#..#OO###.",
        ".#OOO#....#OOO#.",
        ".#OOO#....#OOO#.",
        ".###OO#..#OO###.",
        "...#OOO##OOO#...",
        "..#OOOOOOOOOO#..",
        "..#OO##OO##OO#..",
        "...##.#OO#.##...",
        "......####......",
        "................",
    ], shadow=False)


def person():
    c = Canvas(16, 16)
    m, e = c.shape(ellipse(8, 4.5, 3.4), fill=1)
    c.sphere(m, e, 8, 4.5, 3.4)
    body = lambda x, y: ellipse(8, 15.5, 6.6, 7.5)(x, y) and rect(0, 9, 15, 13)(x, y)
    m, e = c.shape(body, fill=1)
    c.bevel(m, e)
    c.drop_shadow()
    return c


def route_small():
    tree = [".###.", "#.-+#", "#-++#", ".###.", "..#..", "..#.."]
    c = Canvas(16, 16)
    c.sprite(tree, 0, 0)
    c.sprite(tree, 10, 0)
    c.sprite([
        "...-++....",
        "..-++.....",
        "..-++.....",
        "...-++....",
        "....-++...",
        ".....-++..",
        "....-++...",
        "...-++....",
        "..-++.....",
        ".-++......",
    ], 3, 5)
    c.drop_shadow()
    return c


def speaker(waves):
    rows = [
        "........................",
        "........................",
        "........................",
        "........................",
        "......##................",
        ".....#O#................",
        ".####OO#................",
        ".#oo#OO#................",
        ".#oo#OO#................",
        ".####OO#................",
        ".....#O#................",
        "......##................",
        "........................",
        "........................",
        "........................",
        "........................",
    ]
    c = Canvas(24, 16)
    c.sprite(rows, 0, 0)
    if waves == 0:
        c.sprite(["#...#", ".#.#.", "..#..", ".#.#.", "#...#"], 11, 5)
    else:
        for (x, y) in ((10, 5), (11, 6), (11, 7), (11, 8), (11, 9), (10, 10)):
            c.p[y][x] = 3
        if waves == 2:
            for (x, y) in ((13, 3), (14, 4), (15, 5), (15, 6), (15, 7), (15, 8), (15, 9),
                           (15, 10), (14, 11), (13, 12)):
                c.p[y][x] = 3
    c.drop_shadow()
    return c


def bubble_radar(n):
    c = Canvas(16, 16)
    body = rrect(1, 1, 14, 11, 3)
    tail = polygon([(3.5, 11), (7.5, 11), (2.2, 15.2)])
    c.shape(union(body, tail), fill=0)
    cols = {1: [7], 2: [5, 9], 3: [3, 7, 11]}[n]
    for x in cols:
        c.fill(rect(x, 3, x + 1, 7), 3)
        c.fill(rect(x, 9, x + 1, 9), 3)
    c.drop_shadow()
    return c


def radar_click():
    return sprite_icon(16, 16, [
        "................",
        "................",
        "................",
        "....#...........",
        "....#......#....",
        "....#.....#.....",
        "....#....#......",
        "....#...#.......",
        ".......#........",
        "................",
        "................",
        "........######..",
        "................",
        "................",
        "................",
        "................",
    ], shadow=False)


def grass(w, h, back, front, base, x0, x1, back_fill, front_fill):
    """Tall grass tuft: pointed blades (bx0, bx1, tip_x, tip_y) rising from a
    common base, back row drawn first in the darker fill, front row over it
    in the lighter fill, each blade with its own black outline and a light
    left edge; black ground line."""
    c = Canvas(w, h)
    for blades, fill in ((back, back_fill), (front, front_fill)):
        for (bx0, bx1, tx, ty) in blades:
            f = polygon([(bx0, base), (tx, ty), (bx1, base)])
            m, e = c.shape(f, fill=fill, aa=False)
            inner = m - e
            for (x, y) in inner:              # lit left edge where the blade is wide
                if ((x - 1, y) not in inner and (x + 1, y) in inner and (x + 2, y) in inner
                        and fill > 0):
                    c.p[y][x] = fill - 1
    for x in range(x0, x1 + 1):
        c.p[base - 1][x] = 3
    return c


GRASS_SMALL = [                 # dark back blade, two lighter front blades
    ".......##.......",         # leaning outwards; white = lit left edges
    "......#++#......",
    "......#-+#......",
    ".....#-+++#.....",
    ".#...#-+++#...#.",
    ".#.#.#-+++#.#.#.",
    ".#.-##-+++##.-#.",
    "..#.-#++++#.-#..",
    "..#.--#++#.--#..",
    "..#.---##.---#..",
    "..#.---##.---#..",
    "..#.---##.---#..",
    ".##############.",
]


def grass_rows(w, h, back_pts, front_pts, base, back_fill, front_fill):
    """Tall grass: a back row of pointed blades (zigzag top) in the darker
    fill, a front row offset by half a blade in the lighter fill drawn over
    it; each blade's left inside edge is lit, black ground line."""
    c = Canvas(w, h)
    for pts, fill in ((back_pts, back_fill), (front_pts, front_fill)):
        m, e = c.shape(polygon(pts), fill=fill, aa=False)
        inner = m - e
        for (x, y) in inner:          # lit left edge, only where the blade is wide
            if ((x - 1, y) not in inner and (x + 1, y) in inner and (x + 2, y) in inner
                    and fill > 0):
                c.p[y][x] = fill - 1
    for x in range(w):
        if c.p[base - 1][x] != 0:
            c.p[base - 1][x] = 3
    return c


def soft_blade(bx, base, w0, tx, ty, bend, rt=1.5):
    """A soft grass blade: curved centre line from (bx, base) up to the tip,
    bending sideways near the top (bend > 0 = to the right), width tapering
    from w0 to a rounded tip."""
    left, right = [], []
    n = 16
    for i in range(n + 1):
        s = i / n
        cx = bx + (tx - bx) * s + bend * s * s
        cy = base + (ty - base) * s
        hw = (w0 / 2) * (1 - s) ** 0.6 + 0.8 * rt * s
        left.append((cx - hw, cy))
        right.append((cx + hw, cy))
    body = polygon(left + right[::-1])
    tip = ellipse(tx + bend, ty + rt * 0.85, rt, rt * 1.05)   # rounded, leaf-like tip
    return union(body, tip)


def soft_grass(w, h, back, front, base, back_fill, front_fill):
    """Soft tall grass: wide curved blades with rounded tips, darker ones
    behind and lighter ones in front. Only the outer silhouette is black;
    the edges between blades inside the tuft are dark grey."""
    c = Canvas(w, h)
    inner_edges = set()
    total = set()
    for blades, fill in ((back, back_fill), (front, front_fill)):
        for b in blades:
            rt = b[5] if len(b) > 5 else (1.5 if w > 16 else 0.9)
            f = soft_blade(b[0], base, b[1], b[2], b[3], b[4], rt)
            m, e = c.shape(f, fill=fill, aa=False)
            inner = m - e
            for (x, y) in inner:      # lit left edge where the blade is wide
                if ((x - 1, y) not in inner and (x + 1, y) in inner and (x + 2, y) in inner
                        and fill > 0):
                    c.p[y][x] = fill - 1
            inner_edges |= e
            total |= m
    for (x, y) in inner_edges:        # edges inside the tuft: dark grey
        if c.p[y][x] == 3 and all((x + dx, y + dy) in total
                                  for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            c.p[y][x] = 2
    for x in range(w):                # ground line under the tuft
        if c.p[base - 1][x] != 0:
            c.p[base - 1][x] = 3
    return c


def grass_small(light):
    # (base_x, base_width, tip_x, tip_y, bend): rounded tuft, middle tallest
    back = [(6.5, 6.0, 6.0, 1.2, -1.2), (9.5, 6.0, 10.0, 1.2, 1.2)]
    front = [(4.0, 6.5, 3.0, 5.0, -1.6), (12.0, 6.5, 13.0, 5.0, 1.6), (8.0, 6.0, 8.0, 3.6, 0.0)]
    if light:
        return soft_grass(16, 16, back, front, 14, 1, 0)
    return soft_grass(16, 16, back, front, 14, 2, 1)


def radar_grass():
    # the tall many-blade tuft of the first version, with the curled tips of
    # the 16x16 ones; mirror-symmetric except the curl of the middle blade
    back = [(8.0, 8.0, 7.0, 1.5, -1.5, 1.0), (24.0, 8.0, 25.0, 1.5, 1.5, 1.0),
            (16.0, 8.0, 16.5, 0.5, -1.0, 1.0)]
    front = [(4.0, 7.0, 2.0, 8.5, -1.2, 1.0), (28.0, 7.0, 30.0, 8.5, 1.2, 1.0),
             (12.0, 8.0, 10.5, 5.0, -1.5, 1.0), (20.0, 8.0, 21.5, 5.0, 1.5, 1.0)]
    return soft_grass(32, 24, back, front, 22, 2, 1)

# ------------------------------------------------------- 24x16 bubbles

TALK = {
    "exclaim": ["##", "##", "##", "##", "##", "..", "##"],
    "heart": [".##..##.", "#-######", "########", ".######.", "..####..", "...##..."],
    "music": ["..######", "..#....#", "..#....#", ".##...##", "#-#..#-#", ".#....#."],
    "smile": [".#....#.", ".#....#.", "........", "#......#", ".#....#.", "..####.."],
    "neutral": [".#....#.", ".#....#.", "........", "........", ".######.", "........"],
    "ellipsis": ["##..##..##", "##..##..##"],
}


def talk(kind, left=False):
    c = Canvas(24, 16)
    body = rrect(0, 0, 21, 10, 3.5)
    tail = polygon([(13.5, 10.5), (18.0, 10.5), (22.6, 15.0)])
    c.shape(union(body, tail), fill=0)
    spr = TALK[kind]
    w, h = len(spr[0]), len(spr)
    x0 = 11 - w // 2
    y0 = int(6 - h / 2)                 # centred on the inside rows 1..9
    c.sprite(spr, x0, y0)
    if left:
        c.mirror()
    c.drop_shadow()                     # after mirroring: shadow stays bottom-right
    return c

# ------------------------------------------------------ 32x24 pictures

def box3d(c, x0, y0, x1, y1, d, top, front, side):
    """3/4-view box: front face x0..x1 / y0..y1, top and right side faces
    receding d px up-right. Light top, mid front, dark side, black edges."""
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            c.p[y][x] = front
    for k in range(1, d + 1):
        for x in range(x0 + k, x1 + k + 1):
            c.p[y0 - k][x] = top
        for y in range(y0 - k, y1 - k + 1):
            c.p[y][x1 + k] = side
    c.line(x0, y0, x1, y0)
    c.line(x0, y1, x1, y1)
    c.line(x0, y0, x0, y1)
    c.line(x1, y0, x1, y1)
    c.line(x0, y0, x0 + d, y0 - d)
    c.line(x1, y0, x1 + d, y0 - d)
    c.line(x0 + d, y0 - d, x1 + d, y0 - d)
    c.line(x1 + d, y0 - d, x1 + d, y1 - d)
    c.line(x1, y1, x1 + d, y1 - d)


def chest():
    """Treasure chest, 3/4 view lit from the top-left: domed lid shaded as a
    cylinder, lid lip with a dark shadow line under it, wooden body with
    plank grain lines, two metal bands with white highlight edges, corner
    fittings, a lock plate with a keyhole, dark right side."""
    c = Canvas(32, 24)
    # front face columns 3..24 (centre 13.5), lid rows ~3..10, body 10..20,
    # right side receding 3 px up-right (columns 25..28)
    X0, X1, SEAM, BOT = 3, 24, 10, 20
    CX = (X0 + X1 + 1) / 2.0                      # 14.0 in pixel-edge units

    def dome_top(x):                              # continuous dome height
        u = (x - CX) / ((X1 + 1 - X0) / 2.0)
        return SEAM - 6.8 * math.sqrt(max(0.0, 1 - u * u))

    lid = lambda x, y: X0 <= x <= X1 + 1 and dome_top(x) <= y <= SEAM + 1
    body = rect(X0, SEAM, X1, BOT)
    front = union(lid, body)
    # right side: the front silhouette extruded 3 px up-right, dark grey
    extr = lambda x, y: any(front(x - k * 0.5, y + k * 0.5) for k in range(1, 7))
    c.shape(extr, fill=2, aa=False, thin=False)
    c.shape(body, fill=1, aa=False, thin=False)
    m, e = c.shape(lid, fill=1, aa=False)
    inner = m - e
    for (x, y) in inner:                          # lid: light, lit just under the dome
        c.p[y][x] = 0 if (x, y - 1) not in inner and y < SEAM - 2 else 1
    for x in range(X0 + 1, X1):                   # trim band along the lid edge
        c.p[SEAM - 1][x] = 2
    for x in (5, 9, 18, 22):                      # studs on the trim (mirror pairs)
        c.p[SEAM - 1][x] = 0
    for x in range(X0 + 1, X1):                   # shadow under the lid lip
        c.p[SEAM + 1][x] = 2
    # vertical bands (mirror pair: 7-8 and 19-20), white lit edge, rivets
    for bx in (7, 19):
        for y in range(int(math.ceil(dome_top(bx + 1))) + 1, BOT):
            if y in (SEAM, SEAM - 1):
                continue
            c.p[y][bx] = 2
            c.p[y][bx + 1] = 2
        for y in (13, 18):                         # rivets with a lit pixel
            c.p[y][bx] = 0
            c.p[y][bx + 1] = 3
    # corner brackets on the bottom corners (dark L with a white rivet)
    for (x, y, dx, dy) in ((X0 + 1, BOT - 1, 1, -1), (X1 - 1, BOT - 1, -1, -1)):
        for k in range(3):
            c.p[y][x + dx * k] = 2
            c.p[y + dy * k][x] = 2
        c.p[y + dy][x + dx] = 0
    # ornate lock plate across the seam (centred, keyhole in the middle)
    c.sprite([                                    # ' ' = keep what is under it
        "  ##  ",
        " #..# ",
        "#....#",
        "#.##-#",
        "#.##-#",
        " #--# ",
        "  ##  ",
    ], 11, 8)

    # --- details on the dark side and lid top, so the 3/4 view is one object
    def on_dark(x, y, v):
        if 0 <= x < c.w and 0 <= y < c.h and c.p[y][x] == 2:
            c.p[y][x] = v
    # lid top / back: a light highlight curve following the dome, and the
    # two bands running back over it
    for x in range(X0, X1 + 4):
        y = int(math.floor(dome_top(x + 0.5 - 1.5) - 1.5 + 0.5))
        on_dark(x, y + 1, 1)
    for bx in (7, 19):
        t = int(math.ceil(dome_top(bx + 1)))
        for k in range(1, 3):
            on_dark(bx + k, t - k, 1)
            on_dark(bx + 1 + k, t - k, 1)
    # right side: trim band (light) with a stud, seam (black), shadow line,
    # a vertical band with rivets and the bottom bracket edge
    for k in range(1, 4):
        on_dark(X1 + k, SEAM - 1 - k, 1)          # trim
        on_dark(X1 + k, SEAM - k, 3)              # seam
        on_dark(X1 + k, BOT - 1 - k, 1)           # bottom bracket edge
    on_dark(X1 + 2, SEAM - 3, 0)                  # stud on the side trim
    for y in range(SEAM, BOT - 3):
        on_dark(X1 + 2, y, 1)                     # side band
    on_dark(X1 + 2, SEAM + 2, 0)                  # rivets
    on_dark(X1 + 2, BOT - 5, 0)
    return c


def present():
    c = Canvas(32, 24)
    box3d(c, 6, 11, 22, 20, 3, 0, 0, 1)          # box: white front, light side
    box3d(c, 5, 8, 23, 11, 3, 0, 1, 2)           # lid: white top, light front
    for y in list(range(9, 11)) + list(range(12, 20)):   # ribbon down the front
        for x in (13, 14, 15):
            c.p[y][x] = 2
    for k in range(1, 3):                        # ribbon over the lid top
        for x in (13, 14, 15):
            c.p[8 - k][x + k] = 2
    c.sprite([                                   # bow on top
        ".###...###.",
        "#.-.#.#.-.#",
        "#...##+...#",
        ".####+####.",
    ], 11, 2)
    return c


def map_scroll():
    roll = [".##."] + ["#-+#"] * 13 + [".##."]     # cylinder lit from the left
    c = Canvas(32, 24)
    c.sprite([
        "######################",
        "......................",
        "..++..................",
        "...+..................",
        "...+++++..............",
        ".......+..............",
        ".......+++++..#...#...",
        "...............#.#....",
        "................#.....",
        "...............#.#....",
        "..............#...#...",
        "......................",
        "----------------------",
        "######################",
    ], 5, 5)
    c.sprite(roll, 1, 4)
    c.sprite(roll, 27, 4)
    c.drop_shadow()
    return c


def cloud():
    """Classic cumulus: flat bottom, three round puffs on top (middle one
    biggest and highest), white body, light-grey shading underneath, a
    highlight on the top-left puff. Mirror-symmetric outline."""
    c = Canvas(32, 24)
    left = ellipse(7.0, 13.5, 4.5)
    mid = ellipse(16.0, 10.0, 6.4)
    right = ellipse(25.0, 13.5, 4.5)
    base = lambda x, y: 2.5 <= x <= 29.5 and 14.5 <= y <= 18.0
    f = union(left, mid, right, base)
    m, e = c.shape(f, fill=0)
    for (x, y) in m - e:                          # light shading underneath
        if y + 0.5 > 15.5:
            c.p[y][x] = 1
    c.drop_shadow()
    return c

# --------------------------------------------------------- 16x32 bursts

def star(big):
    if big:
        rows = [
            ".......#.......",
            ".......#.......",
            ".#.....#.....#.",
            "..#...###...#..",
            "...#.##-##.#...",
            "....##-++##....",
            "...##-++++##...",
            "####-++#+++####",
            "...##+++++##...",
            "....##+++##....",
            "...#.##+##.#...",
            "..#...###...#..",
            ".#.....#.....#.",
            ".......#.......",
            ".......#.......",
        ]
        return sprite_icon(16, 32, rows, 0, 8, shadow=False)
    rows = [
        ".....#.....",
        ".#...#...#.",
        "..#.###.#..",
        "...##-##...",
        "..##-++##..",
        "####-++####",
        "..##+++##..",
        "...##+##...",
        "..#.###.#..",
        ".#...#...#.",
        ".....#.....",
    ]
    return sprite_icon(16, 32, rows, 2, 10, shadow=False)

# --------------------------------------------------------- walker 32x32

def walker():
    """Round body like a two-tone ball: top dark grey, bottom white, black
    band just below the middle running into the sides of a big recessed
    screen; three round buttons in a gentle arc below. Lit from top-left."""
    c = Canvas(32, 32)
    cx = cy = 16.0
    R = 15.3
    m, edge = c.shape(ellipse(cx, cy, R), fill=0)
    for (x, y) in m - edge:
        nx, ny = (x + 0.5 - cx) / R, (y + 0.5 - cy) / R
        t = (nx + ny) / math.sqrt(2)
        r = math.hypot(nx, ny)
        if y <= 14:                       # top half: dark grey, highlight
            v = 2
            if math.hypot(nx + 0.48, ny + 0.58) < 0.16:
                v = 1
            if math.hypot(nx + 0.50, ny + 0.60) < 0.07:
                v = 0
        elif y <= 16:
            v = 3                         # band across the vertical middle
        else:                             # bottom: white, thin shadow bottom-right
            v = 0
            if r > 0.86 and t > 0.25 and nx > 0.45:
                v = 1
        c.p[y][x] = v
    # screen: black frame, recessed light-grey face (dark inner top/left)
    # screen 20x14 incl. frame (about 63% x 45% of the body, ~1.4:1),
    # top at ~33% of the body height, bottom at ~76%; band meets its sides
    SX0, SY0, SX1, SY1 = 7, 10, 24, 21     # 18x12, centred on the band rows 15-16
    c.shape(rrect(SX0, SY0, SX1, SY1, 1.2), fill=1, aa=False)
    for x in range(SX0 + 1, SX1):
        c.p[SY0 + 1][x] = 2
    for y in range(SY0 + 1, SY1):
        c.p[y][SX0 + 1] = 2
    # three buttons in a gentle arc below: small, bigger + lower, small
    side = ["-#", "##"]
    mid = [".##.", "#-##", ".##."]
    c.sprite(side, 9, 24)          # columns 9-10 and 21-22: mirror pair
    c.sprite(side, 21, 24)
    c.sprite(mid, 14, 25)          # columns 14-17: centred, one row lower
    return c

# ---------------------------------------------------------------- table

def all_icons():
    icons = {
        "ball": ball(), "ball_light": ball_light(), "item": item(), "item_light": item_light(),
        "map_small": map_small(), "vial": vial(), "low_battery": low_battery(),
        "hp_blip": hp_blip(), "catch_star": catch_star(), "music_note": music_note(),
        "menu_arrow_left": menu_arrow(False), "menu_arrow_right": menu_arrow(True),
        "menu_return": menu_return(), "ir_arcs": ir_arcs(),
        "watt": watt(), "icon_radar": icon_radar(), "icon_dowsing": icon_dowsing(),
        "icon_connect": icon_connect(), "icon_trainer_card": icon_trainer_card(),
        "icon_inventory": icon_inventory(), "icon_settings": icon_settings(),
        "person": person(), "route_small": route_small(),
        "speaker_off": speaker(0), "speaker_low": speaker(1), "speaker_high": speaker(2),
        "radar_bubble1": bubble_radar(1), "radar_bubble2": bubble_radar(2),
        "radar_bubble3": bubble_radar(3), "radar_click": radar_click(),
        "bush_dark": grass_small(False), "bush_light": grass_small(True),
        "radar_bush": radar_grass(),
        "talk_exclaim": talk("exclaim"), "talk_heart": talk("heart"), "talk_music": talk("music"),
        "talk_smile": talk("smile"), "talk_neutral": talk("neutral"),
        "talk_ellipsis": talk("ellipsis"), "talk_exclaim_left": talk("exclaim", left=True),
        "chest": chest(), "present": present(), "map_scroll": map_scroll(), "cloud": cloud(),
        "star_attack": star(False), "star_critical": star(True), "walker": walker(),
    }
    for name, rows in SUITS.items():
        icons[name] = sprite_icon(8, 8, rows)
    icons.update(arrows())
    return icons


DESC = {
    "bush_dark": "0x1B50 tall grass tuft, dark",
    "bush_light": "0x1B90 tall grass tuft, light",
    "radar_bush": "0x1CB0 large tall grass tuft (radar)",
    "walker": "0x2350 pedometer picture: round two-tone body, band, screen, three buttons",
}


def main():
    if "--options" in sys.argv:
        options_sheet(os.path.join(ROOT, "hw", "dump", "radar_dowsing_options.png"))
        return
    show = "--print" in sys.argv
    icons = all_icons()
    for name, c in sorted(icons.items()):
        rows = c.rows()
        if show:
            print(name)
            print("\n".join(rows))
            print()
            continue
        path = os.path.join(ICONS, name + ".txt")
        old = ""
        if os.path.exists(path):
            with open(path, encoding="utf-8") as f:
                first = f.readline()
            if first.startswith(";") and "drawn by" not in first:
                old = first[1:].strip()
        desc = DESC.get(name) or old.split(" - ")[0] or name
        desc = re.sub(r"\s*\d+x\d+$", "", desc)
        with open(path, "w", newline="\n") as f:
            f.write("; %s - WalkerBox original art (%dx%d)\n" % (desc, c.w, c.h))
            f.write("; drawn by tools/draw_icons.py (rerun it to regenerate this file)\n")
            f.write("\n".join(rows) + "\n")
    if not show:
        print("wrote %d icons to art/icons/" % len(icons))


if __name__ == "__main__":
    main()
