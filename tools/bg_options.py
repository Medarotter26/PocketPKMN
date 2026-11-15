#!/usr/bin/env python3
"""bg_options.py - candidate app backgrounds (user 2026-11-15: "see if you can create a new background for the app").
Each option is drawn at 2x and halved, for the top (400x240) and bottom (320x240) screens, and shown with a sample
panel + band (as the app draws them) on art_src/ui/bg_options.png. The chosen one goes into tools/ui_atlas.py.
All drawing is our own (no game art).
usage (WSL): python3 tools/bg_options.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
S = 2
H = 240


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def vgrad(w, h, top, bottom):
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        d.line([(0, y), (w, y)], fill=lerp(top, bottom, y / max(1, h - 1)))
    return im


def tint(size, color, mask):
    """A solid colour layer whose alpha is mask (blurring / rotating only the mask: no grey fringes)."""
    layer = Image.new("RGBA", size, color + (0,))
    layer.putalpha(mask)
    return layer


# A: "Pokeball pattern" - soft green, a scattered pattern of faint ball outlines (our own simple circle + line)
def option_a(w):
    W, Hh = w * S, H * S
    im = vgrad(W, Hh, (206, 234, 186), (234, 246, 224)).convert("RGBA")
    mask = Image.new("L", im.size, 0)
    d = ImageDraw.Draw(mask)
    step = 64 * S
    for row, y in enumerate(range(-step, Hh + step, step // 2 * 1)):
        off = (row % 2) * step // 2
        for x in range(-step + off, W + step, step):
            r = 18 * S
            c = 70
            d.ellipse([x - r, y - r, x + r, y + r], outline=c, width=3 * S // 2)
            d.line([(x - r, y), (x - 6 * S, y)], fill=c, width=3 * S // 2)
            d.line([(x + 6 * S, y), (x + r, y)], fill=c, width=3 * S // 2)
            d.ellipse([x - 6 * S, y - 6 * S, x + 6 * S, y + 6 * S], outline=c, width=3 * S // 2)
    mask = mask.rotate(-12, resample=Image.BICUBIC, center=(W / 2, Hh / 2))
    im.alpha_composite(tint(im.size, (150, 196, 128), mask))
    g = Image.new("L", im.size, 0)
    ImageDraw.Draw(g).ellipse([-W * 0.2, -Hh * 0.6, W * 0.7, Hh * 0.55], fill=90)
    im.alpha_composite(tint(im.size, (255, 255, 240), g.filter(ImageFilter.GaussianBlur(16 * S))))
    return im.convert("RGB").resize((w, H), Image.BOX)


# B: "Walker screen" - the Pokewalker's own LCD look: pale olive with a soft dot matrix, a little vignette
def option_b(w):
    W, Hh = w * S, H * S
    im = vgrad(W, Hh, (214, 222, 186), (198, 208, 168)).convert("RGBA")
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    pitch = 6 * S
    for y in range(0, Hh, pitch):
        for x in range(0, W, pitch):
            d.rectangle([x + S, y + S, x + pitch - S, y + pitch - S], fill=(255, 255, 240, 34))
    im.alpha_composite(layer)
    vig = Image.new("L", im.size, 0)
    ImageDraw.Draw(vig).ellipse([-W * 0.3, -Hh * 0.4, W * 1.3, Hh * 1.4], fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(40 * S))
    dark = Image.new("RGBA", im.size, (90, 100, 70, 70))
    dark.putalpha(Image.eval(vig, lambda v: int((255 - v) * 0.35)))
    im.alpha_composite(dark)
    return im.convert("RGB").resize((w, H), Image.BOX)


# C: "Route" - a calm outdoor scene: light sky, soft clouds, rolling green hills along the bottom (route feel)
def option_c(w):
    W, Hh = w * S, H * S
    im = vgrad(W, Hh, (200, 230, 244), (236, 246, 236)).convert("RGBA")
    rnd = random.Random(7 + w)
    clouds = Image.new("L", im.size, 0)
    cd = ImageDraw.Draw(clouds)
    for _ in range(5):
        cx, cy = rnd.uniform(0, W), rnd.uniform(Hh * 0.08, Hh * 0.4)
        for k in range(4):
            r = rnd.uniform(14, 26) * S
            cd.ellipse([cx + k * 16 * S - r, cy - r * 0.6, cx + k * 16 * S + r, cy + r * 0.6], fill=150)
    im.alpha_composite(tint(im.size, (255, 255, 255), clouds.filter(ImageFilter.GaussianBlur(3 * S))))
    hills = Image.new("RGBA", im.size, (0, 0, 0, 0))
    hd = ImageDraw.Draw(hills)
    for k, (base, amp, col, ph) in enumerate(((0.72, 0.06, (182, 222, 160, 255), 0.3),
                                               (0.80, 0.05, (156, 206, 132, 255), 1.7),
                                               (0.88, 0.04, (132, 190, 110, 255), 3.1))):
        pts = [(0, Hh)]
        for x in range(0, W + 8, 8):
            y = Hh * base + Hh * amp * math.sin(x / W * math.pi * 2.2 + ph)
            pts.append((x, y))
        pts.append((W, Hh))
        hd.polygon(pts, fill=col)
    im.alpha_composite(hills)
    return im.convert("RGB").resize((w, H), Image.BOX)


def sample(im):
    """The app's panel + band over the background, to judge contrast."""
    im = im.convert("RGBA")
    d = ImageDraw.Draw(im, "RGBA")
    w = im.width
    d.rounded_rectangle([10, 30, w - 10, 150], radius=6, fill=(250, 252, 248, 236), outline=(176, 208, 156))
    d.rounded_rectangle([10, 30, w - 10, 50], radius=3, fill=(70, 128, 58))
    d.rounded_rectangle([10, 160, w // 2 - 6, 228], radius=6, fill=(244, 246, 242, 245), outline=(182, 188, 180))
    return im.convert("RGB")


def main():
    out_dir = os.path.join(ROOT, "art_src", "ui")
    os.makedirs(out_dir, exist_ok=True)
    opts = (("A  Poke Ball pattern", option_a), ("B  Walker screen", option_b), ("C  Route scene", option_c))
    sheet = Image.new("RGB", (400 + 320 + 30, len(opts) * (H + 34) + 10), (44, 44, 50))
    d = ImageDraw.Draw(sheet)
    for i, (name, fn) in enumerate(opts):
        y = 10 + i * (H + 34)
        d.text((10, y), name, fill=(240, 240, 240))
        top, bot = fn(400), fn(320)
        top.save(os.path.join(out_dir, "bg_opt_%s_top.png" % name[0]))
        bot.save(os.path.join(out_dir, "bg_opt_%s_bottom.png" % name[0]))
        sheet.paste(sample(top), (10, y + 16))
        sheet.paste(sample(bot), (420, y + 16))
    path = os.path.join(out_dir, "bg_options.png")
    sheet.save(path)
    print("->", path)


if __name__ == "__main__":
    main()


# --- v0.48 scrolling tiles (user: "a vertical scrolling effect on the first 2 backgrounds would be nice, and a horizontal
# scroll on the third"). Seamless in the scroll direction; the light effects (glow / shading) are drawn by the app on
# top and do not move.
TILE_V = 256  # vertical repeat of A and B (texture height)
TILE_H = 512  # horizontal repeat of C (texture width)


def tile_a(w=400):
    """Poke Ball pattern, repeating every TILE_V rows (and every w columns when w is a multiple of 64: v0.50 448 for the
    diagonal scroll): ball outlines on a flat soft green (no tilt: a tilt would not
    repeat). Base colour = the middle of option A's gradient."""
    W, Hh = w * S, TILE_V * S
    im = Image.new("RGBA", (W, Hh), (220, 240, 205, 255))
    mask = Image.new("L", im.size, 0)
    d = ImageDraw.Draw(mask)
    step = 64 * S  # 4 rows of balls per 256 px, every other row shifted half a step
    for row in range(-1, Hh // (step // 2) + 2):
        y = row * step // 2
        off = (row % 2) * step // 2
        for x in range(-step + off, W + step, step):
            for dy in (-Hh, 0, Hh):  # draw across the seam so the tile wraps
                yy = y + dy
                r = 18 * S
                d.ellipse([x - r, yy - r, x + r, yy + r], outline=70, width=3 * S // 2)
                d.line([(x - r, yy), (x - 6 * S, yy)], fill=70, width=3 * S // 2)
                d.line([(x + 6 * S, yy), (x + r, yy)], fill=70, width=3 * S // 2)
                d.ellipse([x - 6 * S, yy - 6 * S, x + 6 * S, yy + 6 * S], outline=70, width=3 * S // 2)
    im.alpha_composite(tint(im.size, (150, 196, 128), mask))
    return im.convert("RGB").resize((w, TILE_V), Image.BOX)


def tile_b(w=400):
    """Walker screen: pale olive with a soft dot matrix, 8-px pitch (256 = 32 dots: repeats exactly)."""
    W, Hh = w * S, TILE_V * S
    im = Image.new("RGBA", (W, Hh), (206, 215, 177, 255))
    mask = Image.new("L", im.size, 0)
    d = ImageDraw.Draw(mask)
    pitch = 8 * S
    for y in range(0, Hh, pitch):
        for x in range(0, W, pitch):
            d.rectangle([x + S, y + S, x + pitch - 2 * S, y + pitch - 2 * S], fill=40)
    im.alpha_composite(tint(im.size, (255, 255, 240), mask))
    return im.convert("RGB").resize((w, TILE_V), Image.BOX)


def tile_c():
    """Route scene, repeating every TILE_H columns: sky gradient, clouds (wrapped), hills whose waves fit the width a
    whole number of times."""
    W, Hh = TILE_H * S, H * S
    im = vgrad(W, Hh, (200, 230, 244), (236, 246, 236)).convert("RGBA")
    rnd = random.Random(11)
    clouds = Image.new("L", (3 * W, Hh), 0)  # three tiles wide, blurred, the middle one kept: wraps seamlessly
    cd = ImageDraw.Draw(clouds)
    for _ in range(6):
        cx, cy = rnd.uniform(0, W), rnd.uniform(Hh * 0.08, Hh * 0.4)
        for k in range(4):
            r = rnd.uniform(14, 26) * S
            for dx in (0, W, 2 * W):
                x = cx + dx + k * 16 * S
                cd.ellipse([x - r, cy - r * 0.6, x + r, cy + r * 0.6], fill=150)
    clouds = clouds.filter(ImageFilter.GaussianBlur(3 * S)).crop((W, 0, 2 * W, Hh))
    im.alpha_composite(tint(im.size, (255, 255, 255), clouds))
    hills = Image.new("RGBA", im.size, (0, 0, 0, 0))
    hd = ImageDraw.Draw(hills)
    for base, amp, col, cycles, ph in ((0.72, 0.06, (182, 222, 160, 255), 2, 0.3),
                                       (0.80, 0.05, (156, 206, 132, 255), 3, 1.7),
                                       (0.88, 0.04, (132, 190, 110, 255), 4, 3.1)):
        pts = [(0, Hh)]
        for x in range(0, W + 8, 8):
            pts.append((x, Hh * base + Hh * amp * math.sin(x / W * math.pi * 2 * cycles + ph)))
        pts.append((W, Hh))
        hd.polygon(pts, fill=col)
    im.alpha_composite(hills)
    return im.convert("RGB").resize((TILE_H, H), Image.BOX)
