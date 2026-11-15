#!/usr/bin/env python3
"""ui_scene3d.py - the connect-screen scene for the UI mockups (round 7): an open 3DS seen from BEHIND (the user's
reference photo angle: back of the lid with its cameras, the back edge with cartridge slot and IR window) and the
walker held behind it, its IR window aimed at the 3DS's. A tiny 3D model (boxes + a disc), one camera, flat
shading with a vertical sheen; the walker's screen is the real walker sprite mapped in perspective. Our drawing.

draw_scene(im, x0, y0, w, h, lcd_img) draws into the region (native px * S of the mockup image).
"""
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

# 3DS size (mm-ish): width, depth, base thickness, lid length, lid thickness, lid opening angle
W, DP, T, LL, LT, OPEN = 134.0, 74.0, 13.0, 74.0, 8.0, math.radians(97)
HINGE_IN = 7.0  # the hinge sits a little in from the back edge, so the base's back edge (ports) shows
# camera behind the 3DS (the back edge, z = DP, is nearer) and a little to the side, so the base's front runs off
# to the left behind the lid as in the user's photo; x runs left -> right as seen from behind
YAW, PITCH, PERSP = math.radians(32), math.radians(18), 0.0016
LIGHT = np.array([0.35, 0.8, 0.5])
LIGHT = LIGHT / np.linalg.norm(LIGHT)

BODY = np.array([96, 176, 226])  # 3DS shell colour (aqua, like the photo's)
DARK = np.array([30, 34, 40])


def rot(p):
    """World -> camera: yaw about y, then pitch about x. Camera looks along -z; +z points at the viewer."""
    x, y, z = p
    cy, sy = math.cos(YAW), math.sin(YAW)
    x, z = cy * x + sy * z, -sy * x + cy * z
    cp, sp = math.cos(PITCH), math.sin(PITCH)
    y, z = cp * y - sp * z, sp * y + cp * z
    return np.array([x, y, z])


class Cam:
    def __init__(self, scale, ox, oy):
        self.scale, self.ox, self.oy = scale, ox, oy

    def __call__(self, p):
        q = rot(np.asarray(p, float) - np.array([W / 2, 30, DP / 2]))
        f = 1 / (1 - q[2] * PERSP)
        return (self.ox + q[0] * f * self.scale, self.oy - q[1] * f * self.scale)


def shade(color, normal, sheen=0.0):
    n = rot(normal)
    k = 0.55 + 0.45 * max(0.0, float(np.dot(n / np.linalg.norm(n), rot(LIGHT))))
    c = np.clip(np.asarray(color) * k + sheen * 255, 0, 255)
    return tuple(int(v) for v in c)


def area2(pts):
    return sum(pts[i][0] * pts[(i + 1) % len(pts)][1] - pts[(i + 1) % len(pts)][0] * pts[i][1]
               for i in range(len(pts)))


def box_faces(corner, ex, ey, ez):
    """Six faces of a box from a corner and three edge vectors; each (points, outward normal)."""
    o = np.asarray(corner, float)
    ex, ey, ez = (np.asarray(v, float) for v in (ex, ey, ez))
    P = lambda a, b, c: o + a * ex + b * ey + c * ez  # noqa: E731
    return [
        ([P(0, 0, 0), P(1, 0, 0), P(1, 1, 0), P(0, 1, 0)], -ez),
        ([P(0, 0, 1), P(0, 1, 1), P(1, 1, 1), P(1, 0, 1)], ez),
        ([P(0, 0, 0), P(0, 1, 0), P(0, 1, 1), P(0, 0, 1)], -ex),
        ([P(1, 0, 0), P(1, 0, 1), P(1, 1, 1), P(1, 1, 0)], ex),
        ([P(0, 0, 0), P(0, 0, 1), P(1, 0, 1), P(1, 0, 0)], -ey),
        ([P(0, 1, 0), P(1, 1, 0), P(1, 1, 1), P(0, 1, 1)], ey),
    ]


def facing(normal):
    return rot(normal)[2] > 0


def draw_box(d, cam, faces, color, sheen_face=None):
    for pts, n in faces:
        if not facing(n):
            continue
        poly = [cam(p) for p in pts]
        d.polygon(poly, fill=shade(color, n))
        d.line(poly + [poly[0]], fill=tuple(int(v * 0.7) for v in shade(color, n)), width=1)


def on_face(o, u, v, a, b):
    """Point on a face: origin o + a*u + b*v."""
    return o + a * np.asarray(u, float) + b * np.asarray(v, float)


def face_rect(d, cam, o, u, v, a0, a1, b0, b1, fill):
    d.polygon([cam(on_face(o, u, v, a, b)) for a, b in ((a0, b0), (a1, b0), (a1, b1), (a0, b1))], fill=fill)


def face_circle(d, cam, o, u, v, ca, cb, r, fill, outline=None, n=24):
    pts = [cam(on_face(o, u, v, ca + r * math.cos(t) / np.linalg.norm(u), cb + r * math.sin(t) / np.linalg.norm(v)))
           for t in np.linspace(0, 2 * math.pi, n, endpoint=False)]
    d.polygon(pts, fill=fill, outline=outline)


def perspective_coeffs(dst, src):
    """Coefficients for Image.transform(PERSPECTIVE): output point -> input point."""
    A, B = [], []
    for (x, y), (X, Y) in zip(dst, src):
        A.append([x, y, 1, 0, 0, 0, -X * x, -X * y])
        A.append([0, 0, 0, x, y, 1, -Y * x, -Y * y])
        B += [X, Y]
    return np.linalg.solve(np.array(A, float), np.array(B, float)).tolist()


def paste_quad(im, tex, quad):
    """Map tex (RGBA) onto the screen-space quad (tl, tr, br, bl) in perspective."""
    xs, ys = [p[0] for p in quad], [p[1] for p in quad]
    x0, y0 = int(min(xs)), int(min(ys))
    w, h = int(max(xs)) - x0 + 2, int(max(ys)) - y0 + 2
    local = [(p[0] - x0, p[1] - y0) for p in quad]
    src = [(0, 0), (tex.width, 0), (tex.width, tex.height), (0, tex.height)]
    out = tex.transform((w, h), Image.PERSPECTIVE, perspective_coeffs(local, src), Image.BICUBIC)
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).polygon(local, fill=255)
    out.putalpha(Image.composite(out.getchannel("A"), mask, mask))
    im.paste(out, (x0, y0), out)


def draw_scene(im, rx, ry, rw, rh, lcd_img):
    d = ImageDraw.Draw(im, "RGBA")
    scale = rh / 128
    cam = Cam(scale, rx + rw * 0.38, ry + rh * 0.44)

    # ground shadow
    sh = Image.new("RGBA", im.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh)
    sd.polygon([cam((-4, 0, -6)), cam((W + 8, 0, -6)), cam((W + 8, 0, DP + 30)), cam((-4, 0, DP + 30))],
               fill=(20, 40, 16, 70))
    sh = sh.filter(ImageFilter.GaussianBlur(6 * scale))
    im.paste(sh, (0, 0), sh)

    # base (z = 0 front, z = DP back), lid hinged at the back top edge
    base = box_faces((0, 0, 0), (W, 0, 0), (0, T, 0), (0, 0, DP))
    draw_box(d, cam, base, BODY * 0.92)
    back_o = np.array([0, 0, DP])  # back face: u along x, v along y
    ex, ey = (W, 0, 0), (0, T, 0)
    face_rect(d, cam, back_o, ex, ey, 0.32, 0.62, 0.38, 0.62, tuple(DARK))           # cartridge slot
    face_rect(d, cam, back_o, ex, ey, 0.33, 0.61, 0.44, 0.56, (70, 74, 80))
    face_rect(d, cam, back_o, ex, ey, 0.68, 0.71, 0.25, 0.75, tuple(DARK))           # stylus
    face_rect(d, cam, back_o, ex, ey, 0.80, 0.92, 0.22, 0.80, (24, 6, 10))           # IR window (dark glass)
    face_rect(d, cam, back_o, ex, ey, 0.81, 0.86, 0.32, 0.50, (120, 50, 60))
    face_rect(d, cam, back_o, ex, ey, 0.07, 0.15, 0.30, 0.70, (200, 204, 210))       # power socket
    face_rect(d, cam, back_o, ex, ey, 0.09, 0.13, 0.42, 0.58, tuple(DARK))
    for xa in (0.0, 0.88):  # shoulder buttons
        draw_box(d, cam, box_faces((W * xa, T - 1, DP - 10), (W * 0.12, 0, 0), (0, 3, 0), (0, 0, 10)),
                 np.array([210, 214, 222]))

    a = OPEN
    dirv = np.array([0, math.sin(a), -math.cos(a)])  # along the lid, away from the hinge
    nrm = np.array([0, math.cos(a), math.sin(a)])    # the lid's outer (back) face
    hinge = np.array([0, T, DP - HINGE_IN - LT])
    lid = box_faces(hinge - nrm * 0, (W, 0, 0), dirv * LL, nrm * LT)
    draw_box(d, cam, lid, BODY)
    # outer face of the lid: sheen, two cameras near the top, a little light
    o = hinge + nrm * LT
    u, v = np.array([W, 0, 0]), dirv * LL
    for i in range(12):  # soft vertical sheen
        face_rect(d, cam, o, u, v, 0.02, 0.98, 0.25 + i * 0.05, 0.3 + i * 0.05, (255, 255, 255, 4 + i))
    face_rect(d, cam, o, u, v, 0.0, 1.0, 0.0, 0.08, (0, 0, 0, 40))  # hinge shadow
    for ca in (0.40, 0.55):
        face_circle(d, cam, o, u, v, ca, 0.86, 6.0, (20, 22, 28), (200, 210, 220))
        face_circle(d, cam, o, u, v, ca, 0.86, 2.6, (60, 80, 110))
    face_circle(d, cam, o, u, v, 0.30, 0.86, 1.2, (40, 220, 120))
    ir_spot = cam(on_face(back_o, ex, ey, 0.86, 0.5))

    # the walker, held behind the 3DS, its top (IR window) toward the 3DS's IR window
    wc = np.array([W * 1.02, T + 16, DP + 62])                # centre of the walker's face
    up = np.array([0.0, 0.25, -1.0])
    up /= np.linalg.norm(up)                                 # walker's "top" direction: toward the 3DS
    right = np.array([1.0, 0.0, 0.0])
    fn = np.cross(right, up)                                 # its face normal (up and back: seen by the camera)
    tilt = math.radians(28)                                  # turn the face a bit more toward the camera
    up2 = up * math.cos(tilt) + fn * math.sin(tilt)
    fn2 = np.cross(right, up2)
    R, TH = 21.0, 7.0
    ring = [wc + R * (math.cos(t) * right + math.sin(t) * up2) for t in np.linspace(0, 2 * math.pi, 48, endpoint=False)]
    back = [p - fn2 * TH for p in ring]
    d.polygon([cam(p) for p in back], fill=(150, 156, 168))
    for i in range(48):  # rim
        q = [cam(ring[i]), cam(ring[(i + 1) % 48]), cam(back[(i + 1) % 48]), cam(back[i])]
        if area2(q) < 0:
            d.polygon(q, fill=shade(np.array([214, 218, 228]), np.cross(fn2, ring[i] - wc)))
    face = [cam(p) for p in ring]
    d.polygon(face, fill=shade(np.array([246, 246, 250]), fn2))
    for k in range(10):  # sheen toward the top
        rr = R * (1 - k * 0.08)
        pts = [cam(wc + up2 * (R * 0.25) + rr * 0.8 * (math.cos(t) * right + math.sin(t) * up2 * 0.6))
               for t in np.linspace(0, 2 * math.pi, 32, endpoint=False)]
        d.polygon(pts, fill=(255, 255, 255, 10))
    d.polygon(face, outline=(140, 144, 156))
    # screen (the real sprite, mapped in perspective) with its dark frame
    fw, fh = 15.5, 12.0
    frame = [wc + up2 * 4 + a * right * fw + b * up2 * fh for a, b in ((-1, 1), (1, 1), (1, -1), (-1, -1))]
    d.polygon([cam(p) for p in frame], fill=(52, 54, 62))
    sw, shh = 13.0, 9.75
    scr = [wc + up2 * 4 + a * right * sw + b * up2 * shh for a, b in ((-1, 1), (1, 1), (1, -1), (-1, -1))]
    paste_quad(im, lcd_img, [cam(p) for p in scr])
    d = ImageDraw.Draw(im, "RGBA")
    for bx, br in ((-9, 2.3), (0, 3.0), (9, 2.3)):  # three buttons
        c = wc - up2 * 12.5 + right * bx
        pts = [cam(c + br * (math.cos(t) * right + math.sin(t) * up2)) for t in np.linspace(0, 2 * math.pi, 20)]
        d.polygon(pts, fill=(214, 218, 226), outline=(130, 134, 146))
    # IR window on the walker's top edge
    tip = wc + up2 * R
    win = [cam(tip + right * a - fn2 * b) for a, b in ((-5, 1), (5, 1), (5, 6), (-5, 6))]
    d.polygon(win, fill=(24, 6, 10))
    walker_ir = cam(tip - fn2 * 3.5)

    # IR beams between the two windows (glow)
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    mx, my = (ir_spot[0] + walker_ir[0]) / 2, (ir_spot[1] + walker_ir[1]) / 2
    ang = math.degrees(math.atan2(ir_spot[1] - walker_ir[1], ir_spot[0] - walker_ir[0]))
    dist = math.hypot(ir_spot[0] - walker_ir[0], ir_spot[1] - walker_ir[1])
    for k, rr in enumerate((7, 13, 19)):
        r = rr * scale
        ld.arc([walker_ir[0] - r, walker_ir[1] - r, walker_ir[0] + r, walker_ir[1] + r], ang - 24, ang + 24,
               fill=(226, 72, 40, 255), width=max(2, int(2.0 * scale)))
    glow = layer.filter(ImageFilter.GaussianBlur(3 * scale))
    im.paste(glow, (0, 0), glow)
    im.paste(layer, (0, 0), layer)
    return (mx, my), walker_ir, ir_spot
