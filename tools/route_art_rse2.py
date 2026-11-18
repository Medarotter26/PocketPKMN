#!/usr/bin/env python3
"""route_art_rse2.py - course pictures for the post-game Ruby / Sapphire / Emerald courses, second half (13).

Same rules as tools/route_art.py: 32x24, 4 shades, far (light) to near (dark) layers, one clear subject, places only
(no creatures: user on SM round 4, "They too clearly show real animals"). Run it to write art/routes/<slug>.txt for
these 13 slugs only and the sheet art_src/route_pictures_rse2.png.
"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route_art as ra  # noqa: E402

C, W, H = ra.C, ra.W, ra.H


def ellipse(c, cx, cy, rx, ry, v):
    for y in range(H):
        for x in range(W):
            if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1:
                c.p[y][x] = v


def ellipse_ring(c, cx, cy, rx, ry, v, steps=64):
    for k in range(steps):
        a = 2 * math.pi * k / steps
        c.set(cx + rx * math.cos(a), cy + ry * math.sin(a), v)


def edge(c, inner, v, x0=0, y0=0, x1=W - 1, y1=H - 1):
    """Black (or v) rim on the pixels of shade `inner` that touch another shade, inside a box."""
    src = [r[:] for r in c.p]
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            if src[y][x] != inner:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H and src[ny][nx] != inner:
                    c.p[y][x] = v
                    break


def splat(c, x, y, v=3):
    """Paint splash: a blob with droplets flung out."""
    c.rect(x, y, x + 1, y + 1, v)
    for dx, dy in ((-2, 0), (3, -1), (1, -2), (-1, 2), (3, 2)):
        c.set(x + dx, y + dy, v)


def prism_garden():
    """Prism Garden: a fountain crowned by a big glass prism - a beam of light comes in from the left and fans out
    of it to the right in bands, like a spectrum - water falling from the upper bowl into a wide basin, clipped
    hedges and topiary behind, flowers in front."""
    c = C(0)
    for k in range(4):                                             # the fanned-out bands of light
        c.poly([(18.5, 4 + k * 0.5), (32, 0.5 + k * 2), (32, 2.5 + k * 2), (18.5, 4.5 + k * 0.5)], (1, 2, 1, 2)[k])
    for k in range(13):                                            # the incoming beam
        c.set(k, 1 + k * 0.3, 2 if k % 2 else 1)
        c.set(k, 2 + k * 0.3, 1)
    c.rect(0, 12, W - 1, H - 1, 1)                                 # lawn
    for x0, x1 in ((0, 9), (23, 31)):                              # clipped hedges along the back
        c.rect(x0, 9, x1, 12, 2)
        for x in range(x0, x1 + 1, 3):
            c.set(x + 1, 8, 2)
            c.set(x + 1, 9, 1)
        c.rect(x0, 12, x1, 12, 3)
    c.rect(9, 9, 9, 12, 3)
    c.rect(31, 9, 31, 12, 3)
    c.rect(10, 12, 22, 12, 2)
    for x in (2, 29):                                              # topiary balls in front of them
        c.disc(x, 13, 2.2, 2)
        c.rect(x - 1, 15, x + 1, 15, 3)
        c.set(x - 1, 12, 1)
        c.set(x + 1, 14, 3)
        c.set(x + 2, 13, 3)
    ellipse(c, 16, 18, 10.5, 3.6, 3)                               # lower basin
    ellipse(c, 16, 17.7, 9.4, 2.6, 1)
    for x0, x1, y in ((8, 10, 17), (21, 24, 18), (11, 13, 19)):
        c.rect(x0, y, x1, y, 0)
    c.rect(6, 19, 26, 21, 2)                                       # basin front wall
    c.rect(6, 19, 26, 19, 0)
    c.rect(6, 21, 26, 21, 3)
    c.rect(6, 19, 6, 21, 3)
    c.rect(26, 19, 26, 21, 3)
    c.rect(15, 11, 17, 18, 2)                                      # column
    c.rect(17, 11, 17, 18, 3)
    c.set(15, 18, 3)
    c.rect(10, 10, 22, 10, 0)                                      # upper bowl
    c.rect(11, 11, 21, 11, 2)
    c.rect(13, 12, 19, 12, 3)
    c.set(10, 10, 3)
    c.set(22, 10, 3)
    c.rect(11, 11, 21, 11, 2)
    c.rect(21, 11, 21, 11, 3)
    for x in (10, 22):                                             # water falling from the bowl
        for y in range(11, 17):
            c.set(x + (1 if x == 22 else -1) * (y > 13), y, 0)
    # the prism: a glass triangle standing on the bowl, lit face white, far face light grey, black outline
    c.poly([(15.5, -0.5), (21.5, 9.5), (9.5, 9.5)], 3)
    c.poly([(15.5, 1.5), (15.5, 8.6), (11.2, 8.6)], 0)
    c.poly([(15.6, 1.5), (19.8, 8.6), (15.6, 8.6)], 1)
    c.line(17, 5, 18.5, 7.5, 2)
    c.line(13, 6, 14, 4, 1)
    for x in range(1, 32, 4):                                      # flowers in front
        y = 22 + (x // 4) % 2
        ra.flower(c, x, y, 0)
    return c


def moonlit_hill():
    """Moonlit Hill: a huge cratered full moon in a black starry sky, a round grassy hill rising in front of it
    with a crooked lone tree black against the moon, a glinting moon-rock, a path winding down, low far hills."""
    c = C(3)
    for x, y in ((2, 2), (6, 6), (1, 10), (27, 2), (30, 8), (26, 12), (4, 15), (9, 1), (22, 0), (30, 15), (5, 11)):
        c.set(x, y, 0 if (x + y) % 3 else 1)
    for x, y in ((6, 6), (28, 4)):                                 # two twinkling stars
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, 1)
        c.set(x, y, 0)
    c.disc(17, 8, 7.4, 0)                                          # the full moon
    for x, y, r in ((14, 5, 1.6), (20, 9, 2.0), (15, 10, 1.0), (19, 4, 0.9), (22, 6, 0.6)):
        c.disc(x, y, r, 1)
    c.ground(lambda x: 15 + 1.0 * math.sin(x / 3.0 + 1), 1)       # far hills (moonlit)
    def crest(x):
        return 13 + 0.045 * (x - 17) ** 2
    c.ground(crest, 2)                                             # the round hill
    for x in range(W):
        y = crest(x)
        c.set(x, y, 0 if 10 <= x <= 24 else 1)                     # rim light along the crest
    for i in range(10):                                            # lone pine, black against the moon
        y = 4 + i
        half = [0, 1, 1, 2, 1, 2, 3, 2, 3, 4][i]
        c.rect(15 - half, y, 15 + half, y, 3)
    c.rect(15, 14, 15, 15, 3)
    for x, y in ((14, 6), (13, 9), (12, 12)):
        c.set(x, y, 2)                                             # moonlight on the needles
    c.rect(25, 14, 27, 15, 3)                                      # moon-rock on the slope, glinting
    c.set(26, 13, 3)
    c.set(26, 14, 0)
    c.set(27, 12, 1)
    c.set(28, 13, 1)
    for k in range(40):                                            # path winding down
        t = k / 39.0
        x = 18 + 5 * math.sin(t * 5.0) * t
        y = 13.5 + t * 10.5
        c.set(x, y, 1)
        if t > 0.45:
            c.set(x + 1, y, 1)
    for x, y in ((5, 20), (27, 19), (9, 23), (25, 23), (3, 23), (29, 22), (13, 18)):   # grass tufts
        c.set(x, y, 3)
        c.set(x + 1, y - 1, 3)
        c.set(x + 2, y, 3)
    return c


def rocky_inlet():
    """Rocky Inlet: a narrow strip of sea running in from the horizon between tall rock walls stepped in ledges,
    widening toward us, foam where it breaks on the rocks, a sea stack in the channel, a sea cave on the right."""
    c = C(0)
    ra.cloud(c, 3, 2, 7)
    ra.cloud(c, 22, 1, 6)
    c.rect(0, 7, W - 1, 7, 2)                                      # horizon
    c.rect(0, 8, W - 1, H - 1, 1)                                  # sea
    for y, x0, x1 in ((10, 14, 16), (13, 12, 14), (17, 17, 20), (19, 10, 13), (22, 15, 19)):
        c.rect(x0, y, x1, y, 0)
    left = [(0, 3), (5, 4), (11, 8), (12, 11), (9, 15), (10, 19), (6, 24), (0, 24)]
    right = [(32, 2), (26, 3), (20, 8), (19, 12), (22, 15), (21, 19), (25, 24), (32, 24)]
    c.poly(left, 2)
    c.poly(right, 2)
    c.poly([(11, 8), (12, 11), (9, 15), (10, 19), (6, 24), (4, 24), (7, 19), (6, 15), (9, 11), (8, 8)], 3)
    c.poly([(20, 8), (19, 12), (22, 15), (21, 19), (25, 24), (28, 24), (24, 19), (25, 15), (22, 12), (23, 8)], 3)
    for pts in (left[:7], right[:7]):
        for a, b in zip(pts, pts[1:]):
            c.line(a[0], a[1], b[0], b[1], 3)
    for y, xa, xb in ((7, 0, 6), (12, 0, 7), (17, 0, 5), (21, 0, 3)):   # ledges (light tops), strata under them
        c.line(xa, y, xb, y + 1, 1)
        c.line(xa, y + 2, xb - 1, y + 3, 3)
    for y, xa, xb in ((6, 24, 31), (11, 25, 31), (17, 27, 31)):
        c.line(xa, y + 1, xb, y, 1)
        c.line(xa + 1, y + 3, xb, y + 2, 3)
    c.rect(28, 19, 31, 23, 3)                                      # sea cave in the right wall
    c.rect(29, 18, 30, 18, 3)
    c.rect(29, 22, 31, 22, 1)
    c.poly([(14, 16), (15, 11), (17, 12), (18, 16)], 3)            # sea stack in the channel
    c.set(15, 12, 2)
    c.set(15, 13, 2)
    c.rect(13, 16, 18, 16, 0)
    for x, y in ((12, 11), (10, 15), (11, 19), (8, 23), (19, 12), (21, 15), (20, 19), (24, 23)):
        c.set(x, y, 0)                                             # foam on the rocks
        c.set(x + (1 if x < 16 else -1), y - 1, 0)
    ra.gsc_bird(c, 15, 4)
    return c


GLYPHS = [["###", "#.#", "###", ".#."], [".#.", "###", ".#.", "#.#"], ["#.#", ".#.", "#.#", "#.#"],
          ["##.", "#.#", "##.", "#.."]]


def glyph(c, x, y, k, v):
    for dy, row in enumerate(GLYPHS[k % len(GLYPHS)]):
        for dx, ch in enumerate(row):
            if ch == "#":
                c.set(x + dx, y + dy, v)


def mind_garden():
    """Mind Garden: a raked-sand zen garden - straight rake lines, rings raked round each stone - with a tall
    standing stone carved with ancient glyphs and two smaller stones, a tiled wall behind with a pine over it."""
    c = C(0)
    ra.pine2(c, 27, 8, 9)                                          # pine behind the wall
    ra.pine2(c, 3, 8, 7)
    c.rect(0, 5, W - 1, 6, 3)                                      # tiled roof of the wall
    for x in range(0, W, 2):
        c.set(x, 4, 3)
        c.set(x + 1, 6, 2)
    c.rect(0, 7, W - 1, 10, 1)                                     # the wall
    c.rect(0, 10, W - 1, 10, 2)
    for x in range(4, 32, 8):
        c.rect(x, 7, x, 9, 2)
    for y in (12, 14, 17, 20, 23):                                 # rake lines, wider apart toward us
        c.rect(0, y, W - 1, y, 1)
    def stone(cx, base, w, h, glyphs, rx, ry):
        ellipse(c, cx, base, rx, ry, 0)                            # sand raked in rings round the stone
        ellipse_ring(c, cx, base, rx, ry, 1, 80)
        ellipse_ring(c, cx, base, rx * 0.72, ry * 0.7, 1, 80)
        c.poly([(cx - w - 1, base + 0.5), (cx - w, base - h + 4), (cx - w + 2, base - h + 1), (cx, base - h),
                (cx + w - 1, base - h + 2), (cx + w, base - h * 0.5), (cx + w + 1, base + 0.5)], 2)
        x0, x1 = int(cx - w - 1), int(cx + w + 1)
        edge(c, 2, 3, x0, max(0, base - h - 1), x1, min(H - 1, base + 1))
        for y in range(base - h + 3, base):                        # lit left side
            if c.get(int(cx - w) + 1, y) == 2:
                c.set(int(cx - w) + 1, y, 1)
        for k, (gx, gy) in enumerate(glyphs):
            glyph(c, gx, gy, k + cx, 0)
    stone(11, 21, 6, 10, ((8, 13), (13, 15)), 9.5, 2.6)
    stone(25, 16, 3, 5, ((24, 12),), 5.5, 1.6)
    stone(27, 22, 2, 3, (), 4, 1.2)
    return c


def shadow_hollow():
    """Shadow Hollow: a hollow sinking between two black slopes under a heavy sky, a leaning ruined bell tower on the
    far rim, pale gravestones down the slopes, mist pooled in the bottom, will-o'-the-wisps drifting."""
    c = C(0)
    c.dither(0, 0, W - 1, 11, 0, 1, 0.55)                          # heavy sky
    c.ground(lambda x: 10 + 0.6 * math.sin(x / 2.5), 2)           # far rim
    for i in range(12):                                            # leaning ruined tower on the rim
        y = 10 - i
        x = 17 + i // 4
        c.rect(x, y, x + 4, y, 3)
    c.rect(20, -1, 20, 0, 3)                                       # broken top
    c.set(22, 0, 0)
    c.set(21, 0, 0) if False else None
    c.rect(19, 3, 20, 5, 1)                                        # arched bell window
    c.set(20, 2, 1) if False else c.set(19, 2, 3)
    c.rect(19, 7, 19, 8, 1)
    c.dither(0, 14, W - 1, 18, 1, 0, 0.35)                         # mist pooled in the bottom
    c.poly([(0, 10), (6, 11), (12, 15), (15, 20), (0, 24)], 3)     # left slope
    c.poly([(32, 11), (26, 12), (21, 16), (18, 20), (32, 24)], 3)  # right slope
    c.rect(0, 20, W - 1, H - 1, 3)
    c.dither(13, 18, 19, 19, 1, 2, 0.5)
    def grave(x, base, h, cross=False):
        if cross:
            c.rect(x + 1, base - h, x + 1, base, 1)
            c.rect(x, base - h + 1, x + 2, base - h + 1, 1)
        else:
            c.rect(x, base - h + 1, x + 2, base, 1)
            c.set(x + 1, base - h, 1)
            c.rect(x + 2, base - h + 2, x + 2, base, 2)
            c.set(x + 1, base - h + 2, 2)
    grave(2, 14, 4)
    grave(7, 15, 3, True)
    grave(3, 21, 5, True)
    grave(9, 20, 4)
    grave(27, 15, 4, True)
    grave(23, 17, 3)
    grave(25, 22, 5)
    for x, y, s in ((8, 5, 1), (13, 10, -1), (28, 7, 1)):         # wisps: white flames, grey tails
        c.rect(x, y, x + 1, y + 1, 0)
        c.set(x, y - 1, 0)
        c.set(x - s, y + 2, 1)
        c.set(x - 2 * s, y + 3, 1)
        c.set(x - 2 * s, y + 4, 2)
    return c


def mimics_grove():
    """Mimic's Grove: a grove of round trees; the one in the middle is plainly odd - a striped, painted crown
    dripping paint, on a crooked trunk - an easel with an unfinished tree painting in front, paint splashes and a
    tipped paint pot on the grass."""
    c = C(0)
    for x in range(-1, W + 2, 5):                                  # far tree line (light)
        c.disc(x, 7, 2.8, 1)
    c.rect(0, 7, W - 1, 11, 1)
    c.rect(0, 11, W - 1, H - 1, 1)
    c.rect(0, 11, W - 1, 11, 2)
    ra.tree2(c, 27, 17, r=4, h=5)                                  # ordinary near trees
    ra.tree2(c, 3, 13, r=3, h=4)
    # the odd "tree": a stiff trunk with two arms held out stiffly, round leaf balls on the arm ends and three on top,
    # spotted with white paint dots - nothing like the real trees beside it
    c.rect(15, 7, 17, 17, 3)
    c.rect(16, 8, 16, 16, 2)
    for y in (10, 14):
        c.set(15, y, 2)
    c.line(14, 12, 11, 9, 3)                                       # arms
    c.line(18, 11, 21, 8, 3)
    c.rect(13, 17, 19, 17, 3)
    for bx, by, r in ((10, 7, 2.2), (22, 6, 2.2), (13, 4, 2.3), (19, 4, 2.3), (16, 2.5, 2.4)):
        c.disc(bx, by, r + 0.9, 3)
        c.disc(bx, by, r, 2)
    for x, y in ((10, 6), (22, 5), (13, 3), (19, 3), (16, 1), (15, 4), (21, 7), (12, 5)):
        c.set(x, y, 0)                                             # paint dots
    # easel with an unfinished tree painting, front left
    c.line(3, 23, 5, 14, 3)
    c.line(11, 23, 9, 14, 3)
    c.line(7, 15, 7, 23, 3)
    c.rect(1, 13, 12, 19, 3)
    c.rect(2, 14, 11, 18, 0)
    c.disc(6, 15.5, 1.6, 2)
    c.rect(6, 17, 6, 18, 3)
    c.rect(9, 15, 10, 15, 1)
    c.set(10, 17, 1)
    # tipped paint pot spilling, and splashes
    c.rect(20, 20, 23, 22, 3)
    c.rect(19, 20, 19, 22, 2)
    c.rect(20, 21, 20, 21, 0)
    c.rect(15, 22, 19, 23, 2)
    c.rect(16, 21, 17, 21, 2)
    for x, y in ((14, 19), (29, 21), (24, 18), (11, 21)):
        splat(c, x, y, 3 if x % 2 else 2)
    return c


def tide_pools():
    """Tide Pools: low tide - a wide shelf of dark wet rock running out to the sea, bright pools of still water in
    it reflecting the sky, seaweed and barnacles at their rims, the low waves breaking at the far edge."""
    c = C(0)
    c.disc(6, 3, 2.6, 1)
    ra.cloud(c, 18, 3, 8)
    c.rect(0, 8, W - 1, 8, 2)
    c.rect(0, 9, W - 1, 11, 1)                                     # the sea
    for x in range(1, W, 5):
        c.rect(x, 10, x + 1, 10, 0)
    for x in range(W):                                             # breaking waves at the shelf edge
        c.set(x, 12, 0 if x % 4 < 2 else 1)
    c.rect(0, 13, W - 1, H - 1, 2)                                 # rock shelf
    c.rect(0, 13, W - 1, 13, 3)
    for x0, y0, x1, y1 in ((2, 16, 6, 20), (20, 14, 24, 19), (10, 20, 13, 23), (29, 18, 31, 21)):
        c.line(x0, y0, x1, y1, 3)                                  # cracks
    pools = ((8, 15.5, 5, 1.2), (24, 16, 4, 1.1), (18, 20.5, 7, 2.0), (4, 21.5, 3.5, 1.5))
    for cx, cy, rx, ry in pools:
        ellipse(c, cx, cy + 0.6, rx + 0.9, ry + 0.7, 3)            # dark rim
        ellipse(c, cx, cy, rx, ry, 0)                              # water showing the sky
        c.rect(int(cx - rx / 2), int(cy + ry * 0.5), int(cx + rx / 3), int(cy + ry * 0.5), 1)   # ripple
    c.rect(17, 21, 19, 21, 2)                                      # a shell in the big pool
    c.set(18, 20, 2)
    for x, y in ((12, 15), (13, 16), (29, 15), (30, 16), (27, 22), (28, 23), (1, 17), (14, 17), (2, 18)):
        c.set(x, y, 1)                                             # barnacles
    for x, y, h in ((11, 21, 3), (25, 21, 3), (26, 20, 2), (1, 21, 3)):
        for k in range(h):
            c.set(x + (k % 2), y - k, 3)                           # seaweed fronds
    return c


def lily_pond():
    """Lily Pond: a still pond seen low across the water - a dark far bank of trees, light streaks on the water, big
    round notched lily pads growing from small far ones to large near ones, a white lily in bloom on the nearest,
    cattails framing both sides."""
    c = C(0)
    ra.cloud(c, 4, 2, 7)
    ra.cloud(c, 21, 1, 5)
    for x in range(1, W + 3, 5):                                   # far bank of trees
        c.disc(x, 5, 2.6, 1)
        c.set(x - 1, 3, 0)
    c.rect(0, 6, W - 1, 7, 1)
    c.rect(0, 8, W - 1, 8, 3)
    c.rect(0, 9, W - 1, H - 1, 2)                                  # the pond, dark and still
    c.dither(0, 9, W - 1, 9, 2, 1, 0.5)                            # far bank reflected
    for x, y, w in ((3, 11, 4), (19, 10, 6), (14, 13, 3), (27, 15, 3), (1, 18, 2)):
        c.rect(x, y, x + w, y, 0)                                  # light on the water
    def pad(cx, cy, rx, ry):
        ellipse(c, cx, cy + 0.6, rx, ry, 3)                        # dark underside
        ellipse(c, cx, cy, rx, ry, 1)                              # the pad, light green
        c.poly([(cx, cy), (cx + rx + 1, cy - ry * 0.3), (cx + rx + 1, cy + ry + 1)], 2)   # the notch
        if rx >= 4:
            c.line(cx - rx * 0.6, cy, cx - 1, cy, 0)               # a vein catching the light
    for cx, cy in ((8, 11), (15, 10), (25, 12)):                   # small far pads
        c.rect(cx - 1, cy, cx + 1, cy, 1)
    pad(22, 14, 3.2, 1.0)
    pad(6, 15, 3.5, 1.2)
    pad(24, 19.5, 5.5, 1.8)
    pad(11, 20, 7, 2.3)
    # the lily on the nearest pad: pointed white petals, light centre
    c.poly([(11, 20), (8, 16), (10, 17.5), (11, 14.5), (12, 17.5), (14, 16)], 0)
    c.rect(8, 19, 14, 20, 0)
    c.set(7, 19, 0)
    c.set(15, 19, 0)
    c.rect(10, 18, 12, 18, 0)
    c.set(11, 18, 1)
    c.set(10, 19, 1)
    c.set(12, 19, 1)
    for x, top, s in ((1, 11, 1), (3, 15, 0), (30, 10, 0), (28, 15, 1)):   # cattails
        c.rect(x, top, x, H - 1, 3)
        c.rect(x, top + 1, x + s, top + 3, 3)
        c.set(x, top, 2)
    c.line(2, 23, 0, 17, 3)                                        # long blades
    c.line(29, 23, 31, 18, 3)
    return c


def deep_current():
    """Deep Current: open ocean under a grey sky, nothing but sea - dark swells to the horizon and one huge black
    wave rearing up in front, its crest curling right over into a tube, edged with white foam, spray flying off the
    lip; a tall spout of spray far out where something big surfaced."""
    c = C(1)
    for x, y, w in ((1, 2, 8), (13, 4, 6), (24, 1, 6)):            # pale breaks in the grey sky
        c.rect(x + 1, y, x + w - 2, y, 0)
        c.rect(x, y + 1, x + w - 1, y + 1, 0)
    c.rect(0, 9, W - 1, 9, 3)                                      # horizon
    c.rect(0, 10, W - 1, H - 1, 2)                                 # deep sea
    for y in (11, 13, 16, 20):                                     # swells: light crests
        for x in range(W):
            if (x + y * 3) % 8 < 3:
                c.set(x, y, 1)
            elif (x + y * 3) % 8 == 3:
                c.set(x, y + 1, 3)
    c.rect(4, 4, 4, 9, 0)                                          # spout far out
    c.rect(3, 3, 5, 3, 0)
    c.set(2, 2, 0)
    c.set(6, 2, 0)
    c.rect(2, 9, 6, 9, 3)
    c.rect(3, 8, 5, 8, 3)
    # the wave: a round crest over a broad body, a hollow tube under the curl
    cx, cy, R = 21, 9, 7
    c.disc(cx, cy, R, 3)
    c.poly([(8, 24), (12, 18), (15, 15), (20, 13), (32, 4), (32, 24)], 3)
    c.disc(cx - 1, cy + 1.5, 3.4, 2)                               # the tube
    c.poly([(13.5, 11), (18, 9.5), (19, 14), (15, 15)], 2)         # opening down-left
    c.disc(cx - 1.5, cy + 2.5, 1.5, 1)                             # light deep inside the tube
    for k in range(80):                                            # white foam along the curling lip
        a = math.radians(-5 - 205 * k / 79.0)
        for rr in (R + 0.3, R - 0.6):
            c.set(cx + rr * math.cos(a), cy + rr * math.sin(a), 0)
    for k in range(40):                                            # the lip folding in at its tip
        a = math.radians(160 + 70 * k / 39.0)
        c.set(cx - 2 + 4.3 * math.cos(a), cy + 1 + 4.3 * math.sin(a), 0)
    for x, y in ((12, 11), (11, 13), (13, 14), (11, 9), (10, 11), (14, 16), (9, 13)):
        c.set(x, y, 0)                                             # spray flung off the lip
    for k in range(3):                                             # streaks on the wave's face
        c.line(17 + k * 4, 23, 23 + k * 3, 16 - k, 2)
    for x, y in ((1, 22), (6, 19), (3, 17)):
        c.rect(x, y, x + 2, y, 0)                                  # foam on the swells
    return c


def snowdrift_pass():
    """Snowdrift Pass: a pass between two dark mountain shoulders, buried in deep wind-carved drifts with overhanging
    crests, a line of striped marker poles half buried leading up through it, falling snow."""
    c = C(0)
    for x, y in ((14, 1), (17, 3), (20, 0), (11, 2), (16, 6), (23, 1)):
        c.set(x, y, 1)                                             # snow in the sky
    c.poly([(0, 2), (6, 1), (12, 7), (14, 12), (0, 14)], 2)        # mountain shoulders
    c.poly([(32, 1), (25, 2), (19, 8), (18, 12), (32, 14)], 2)
    for a, b in (((0, 2), (6, 1)), ((6, 1), (12, 7)), ((12, 7), (14, 12)),
                 ((31, 1), (25, 2)), ((25, 2), (19, 8)), ((19, 8), (18, 12))):
        c.line(a[0], a[1], b[0], b[1], 3)
    for x, y in ((3, 4), (8, 4), (27, 5), (22, 7), (5, 8), (29, 9), (10, 9), (25, 9)):
        c.rect(x, y, x + 1, y, 0)                                  # snow streaks on the rock
        c.set(x + 2, y + 1, 0)
    c.poly([(12, 7), (15, 9), (17, 9), (19, 8), (18, 12), (14, 12)], 1)    # far notch of the pass
    # drifts: white mounds, each with a grey crest line and a light shadow under its overhang
    for base, amp, ph, per in ((12, 1.2, 0.5, 4.0), (16, 1.8, 2.0, 5.0), (20, 2.0, 4.0, 6.5)):
        f = (lambda x, b=base, a=amp, p=ph, q=per: b + a * math.sin(x / q + p))
        c.ground(f, 0)
        for x in range(W):
            y = f(x)
            c.set(x, y, 2)
            if math.cos(x / per + ph) < 0:
                c.set(x, y + 1, 1)                                 # shade on the lee side
    for x, y, h in ((15, 12, 2), (13, 15, 3), (10, 19, 4), (5, 23, 7)):   # marker poles, half buried
        for k in range(h + 1):
            c.set(x, y - k, 3 if (k // max(1, h // 3)) % 2 == 0 else 1)
        c.set(x, y - h, 3)
    for x, y in ((2, 10), (25, 13), (21, 17), (29, 18), (3, 16), (16, 3), (23, 21), (14, 22), (8, 1), (27, 7)):
        c.set(x, y, 0 if c.get(x, y) else 1)                       # falling snow
    return c


def wild_plains():
    """Wild Plains: a huge sky over a low far horizon, a dust cloud rising where something big is running out of
    sight, a great round hill lying alone in the grass like a sleeper, a track and wind-bent tall grass in front."""
    c = C(0)
    ra.cloud(c, 2, 2, 8)
    ra.cloud(c, 22, 3, 7)
    c.rect(0, 12, W - 1, 12, 2)
    c.rect(0, 13, W - 1, H - 1, 1)
    c.poly([(24, 12), (25, 10), (30, 10), (31, 12)], 1)            # far mesa
    c.rect(25, 10, 30, 10, 2)
    for x, y, r in ((3, 11, 1.8), (6, 10, 2.4), (9, 11, 1.6)):     # dust cloud on the horizon
        c.disc(x, y, r, 1)
    for x in (2, 5, 8, 10):
        c.set(x, 12, 3)
    def inside(x, y):
        return y <= 17 and ((x - 19) / 8.0) ** 2 + ((y - 18) / 9.0) ** 2 <= 1
    for y in range(H):                                             # the great round hill, lying on the grass
        for x in range(W):
            if inside(x, y):
                rim = not (inside(x, y - 1) and inside(x + 1, y) and inside(x - 1, y))
                c.p[y][x] = 3 if rim or (x - 19) * 0.8 - (y - 18) * 0.3 > 4.5 else 2
    for x, y in ((13, 11), (14, 10), (12, 12), (15, 10), (11, 14), (13, 12)):
        c.set(x, y, 1)                                             # light catching its top
    for x in range(9, 28, 2):
        c.set(x, 17, 3 if x > 18 else 2)                           # grass at its foot
    ra.tree2(c, 21, 10, r=2, h=2)                                  # a lone tree on top
    for k in range(30):                                            # a track to the dust cloud
        t = k / 29.0
        x = 4 + 10 * (1 - t) ** 1.5 + 2 * t
        y = 23 - t * 10
        c.set(x, y, 0)
        if t < 0.5:
            c.set(x + 1, y, 0)
    for i, x in enumerate((0, 2, 3, 6, 18, 20, 21, 23, 25, 26, 28, 30, 31)):   # tall grass, bent by the wind
        h = 3 + (i * 5) % 4
        for k in range(h):
            c.set(x + k // 3, 23 - k, 3 if (i % 3) else 2)
    for y, x0, x1 in ((19, 21, 27), (21, 15, 19)):
        c.rect(x0, y, x1, y, 0)                                    # wind streaks
    return c


def titans_rest():
    """Titans' Rest: a ring of giant standing stones in perspective - two huge capped trilithons in front framing
    the view, smaller ones fading behind - a sun setting in the central gap, worn steps leading up to it."""
    c = C(0)
    c.disc(16, 9, 3.3, 1)                                          # setting sun
    for k in range(-3, 4):
        if k:
            c.line(16 + k * 1.6, 9, 16 + k * 6, 3 - abs(k), 1)
    c.rect(0, 12, W - 1, 12, 2)
    c.rect(0, 13, W - 1, H - 1, 1)
    def stone(x, base, w, h, v, edge=3):
        c.rect(x, base - h, x + w - 1, base, v)
        c.rect(x + w - 1, base - h, x + w - 1, base, edge)
        if v >= 2 and w >= 3:
            c.rect(x, base - h + 1, x, base, 1)                    # lit left face
        c.set(x, base - h, 0 if v >= 2 else 1)
    for x, h in ((3, 3), (8, 4), (22, 4), (27, 3)):                # far ring (light)
        stone(x, 12, 2, h, 1, edge=2)
    c.rect(7, 7, 12, 7, 1)
    c.rect(21, 7, 26, 7, 1)
    for x, h in ((5, 6), (24, 6)):                                 # middle stones
        stone(x, 15, 3, h, 2)
    c.rect(4, 8, 27, 8, 0) if False else None
    for x0 in (0, 22):                                             # two giant trilithons in front
        stone(x0, 23, 4, 18, 2)
        stone(x0 + 6, 23, 4, 18, 2)
        c.rect(x0 - 1, 2, x0 + 10, 4, 3)                           # lintel
        c.rect(x0, 2, x0 + 9, 2, 2)
        for y in (9, 15, 20):                                      # weathering cracks
            c.set(x0 + 2, y, 3)
            c.set(x0 + 8, y + 1, 3)
    for x, y in ((12, 20), (19, 21), (13, 13)):                    # fallen rubble
        c.rect(x, y, x + 1, y, 3)
    for i, y in enumerate((22, 19, 17, 15)):                       # worn steps up the middle
        w = 6 - i
        c.rect(16 - w, y, 15 + w, y, 0)
        c.rect(16 - w, y + 1, 15 + w, y + 1, 2)
    c.rect(0, 23, W - 1, 23, 3)
    return c


def wyrm_peak():
    """Wyrm Peak: one jagged black peak with a tall hooked fang of rock on top and lesser spires on its shoulders,
    a dark storm cloud with a lightning bolt, mist wrapped round its waist, a lair cave half way up with huge claw
    gouges beside it, scree in front."""
    c = C(0)
    c.dither(0, 8, W - 1, 17, 0, 1, 0.5)                           # haze low down
    c.poly([(0, 0), (14, 0), (13, 2), (10, 3), (8, 5), (4, 5), (0, 7)], 2)    # storm cloud
    c.rect(0, 0, 13, 0, 3)
    c.line(5, 5, 7, 8, 3)                                          # lightning, black against the haze
    c.line(7, 8, 5, 9, 3)
    c.line(5, 9, 8, 14, 3)
    c.rect(27, 6, 31, 6, 1)                                        # a far cloud
    c.rect(26, 7, 31, 7, 1)
    c.poly([(0, 18), (3, 13), (6, 15), (9, 18)], 1)                # far peaks either side
    c.poly([(25, 18), (28, 12), (30, 14), (32, 12), (32, 18)], 1)
    # the peak: a hooked fang on top, spires on both shoulders
    c.poly([(3, 24), (7, 18), (9, 14), (10, 9), (11, 7), (12, 10), (14, 8), (16, 5), (18, 3), (21, 2), (24, 2),
            (21, 4), (19, 6), (20, 8), (22, 7), (23, 11), (25, 9), (26, 13), (28, 18), (32, 22), (32, 24)], 3)
    c.line(21, 2, 18, 3, 2)                                        # lit left edges
    c.line(18, 3, 16, 5, 2)
    c.line(16, 5, 14, 8, 2)
    c.line(11, 7, 10, 9, 2)
    c.line(10, 9, 9, 14, 2)
    c.line(9, 14, 7, 18, 2)
    c.line(7, 18, 4, 23, 2)
    c.set(24, 10, 2)
    for x in range(8, 27):                                         # mist round the waist
        if x % 5 != 4:
            c.set(x, 13 + (x // 7) % 2, 1)
    c.rect(11, 14, 13, 14, 0)
    c.rect(19, 13, 21, 13, 0)
    # lair cave: a black mouth framed by a grey rim, claw gouges beside it
    c.poly([(14, 21), (15, 18), (17.5, 16.5), (20, 18), (21, 21)], 2)
    c.poly([(15, 21), (16, 18.5), (17.5, 17.6), (19, 18.5), (20, 21)], 3)
    for k in range(3):
        c.line(22 + k * 1.5, 16 + k, 24 + k * 1.5, 19 + k, 1)
    c.poly([(0, 24), (0, 22), (8, 21), (16, 22), (24, 21), (32, 22), (32, 24)], 2)    # scree
    for x, y in ((2, 23), (7, 22), (12, 23), (19, 23), (26, 23), (30, 22)):
        c.rect(x, y, x + 1, y, 3)
        c.set(x, y - 1, 1)
    return c


RSE2 = {"prism_garden": prism_garden, "moonlit_hill": moonlit_hill, "rocky_inlet": rocky_inlet,
        "mind_garden": mind_garden, "shadow_hollow": shadow_hollow, "mimics_grove": mimics_grove,
        "tide_pools": tide_pools, "lily_pond": lily_pond, "deep_current": deep_current,
        "snowdrift_pass": snowdrift_pass, "wild_plains": wild_plains, "titans_rest": titans_rest,
        "wyrm_peak": wyrm_peak}
ra.COURSES.update(RSE2)


if __name__ == "__main__":
    os.makedirs(ra.OUT, exist_ok=True)
    for name, fn in RSE2.items():
        c = fn()
        path = os.path.join(ra.OUT, f"{name}.txt")
        with open(path, "w") as f:
            f.write(f"; {name}: 32x24, . white  - light grey  + dark grey  # black (tools/route_art_rse2.py)\n")
            for row in c.p:
                f.write("".join(ra.CH[v] for v in row) + "\n")
        print("->", path)
    ra.preview_names(os.path.join(ra.ROOT, "art_src", "route_pictures_rse2.png"), list(RSE2))
