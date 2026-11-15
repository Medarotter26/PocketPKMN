#!/usr/bin/env python3
"""ui_mockups.py <out dir> - WalkerBox UI mockups (Pokemon-Bank-like layout, all drawing our own).

Each PNG = a 3DS: top screen 400x240 over bottom screen 320x240, drawn at 2x. Pokemon pictures come from the
user's local caches only (art_src/sprites5to7/small colour icons, sd_out walker sprites); the mockups are for
the user's eyes, not for the repo or a release. Font: Inter (OFL), from the WSL system fonts.
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import pwimg  # noqa: E402

S = 2  # drawing scale
FRAME = 0  # walker animation frame (0/1); main() renders both and writes an animated PNG (round 7)
APP_NAME = "PocketPKMN"  # user, round 7: "Change name to PocketPKMN"
TOP_W, BOT_W, H = 400, 320, 240
FONT_DIR = "/usr/share/fonts/opentype/inter/"

# palette (our own): greens like the reference, red-orange accent
BG1, BG2 = (196, 228, 170), (232, 246, 220)
LATTICE = (178, 214, 152)
BAND = (58, 112, 50)
BAND_TEXT = (255, 255, 255)
PANEL = (255, 255, 255)
PANEL_EDGE = (150, 196, 120)
TEXT = (40, 60, 36)
SUB = (96, 120, 88)
ACCENT = (226, 72, 40)
LCD_BG, LCD_SHADES = (196, 206, 168), [(196, 206, 168), (140, 152, 116), (82, 92, 66), (30, 36, 24)]


UI_FONT_DIR = os.path.join(ROOT, "art_src", "fonts", "Console Square Beta", "wbx")  # copies with é added
# our weight names -> the user's font (Console Square Beta, added 2026-11-15)
WEIGHTS = {"Medium": "Regular", "SemiBold": "Medium", "Bold": "Bold"}


def font(size, weight="SemiBold"):
    p = os.path.join(UI_FONT_DIR, f"ConsoleSquareBeta-{WEIGHTS.get(weight, 'Regular')}.otf")
    if os.path.exists(p):
        return ImageFont.truetype(p, size * S)
    return ImageFont.truetype(os.path.join(FONT_DIR, f"Inter-{weight}.otf"), size * S)


def sc(*v):
    return [x * S for x in v]


def background(w):
    """Gradient, a soft light glow, faint diagonal light streaks, then the lattice (round 4: less flat)."""
    im = Image.new("RGB", (w * S, H * S), BG1)
    d = ImageDraw.Draw(im, "RGBA")
    for y in range(H * S):  # vertical gradient
        t = y / (H * S)
        c = tuple(int(BG1[i] + (BG2[i] - BG1[i]) * t) for i in range(3))
        d.line([(0, y), (w * S, y)], fill=c)
    glow = Image.new("RGBA", im.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse(sc(-w * 0.2, -H * 0.6, w * 0.7, H * 0.55), fill=(255, 255, 240, 70))
    for k in range(-2, 6):  # light streaks
        x0 = k * w / 4
        gd.polygon(sc(x0, 0, x0 + 26, 0, x0 + 26 - H * 0.6, H, x0 - H * 0.6, H), fill=(255, 255, 255, 22))
    glow = glow.filter(ImageFilter.GaussianBlur(14 * S))
    im.paste(glow, (0, 0), glow)
    d = ImageDraw.Draw(im, "RGBA")
    for k in range(-H, w, 16):  # faint diagonal lattice (round 7: fainter)
        d.line([(k * S, 0), ((k + H) * S, H * S)], fill=LATTICE + (80,), width=1)
        d.line([((k + H) * S, 0), (k * S, H * S)], fill=LATTICE + (80,), width=1)
    return im


def soft_shadow(d, x0, y0, x1, y1, r, off=(1, 1.5), blur=2, alpha=45, color=(24, 52, 18), ellipse=False):
    """A blurred drop shadow (round 4: real shadows instead of hard edges)."""
    pad = int(blur * 3 * S)
    w, h = int((x1 - x0) * S) + 2 * pad, int((y1 - y0) * S) + 2 * pad
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    if ellipse:
        ld.ellipse([pad, pad, w - pad, h - pad], fill=color + (alpha,))
    else:
        ld.rounded_rectangle([pad, pad, w - pad, h - pad], radius=r * S, fill=color + (alpha,))
    layer = layer.filter(ImageFilter.GaussianBlur(blur * S))
    d._image.paste(layer, (int((x0 + off[0]) * S) - pad, int((y0 + off[1]) * S) - pad), layer)


def shadow_text(d, xy, text, f, fill, anchor, shadow=(0, 0, 0, 110)):
    d.text((xy[0] + 0.8 * S, xy[1] + 0.8 * S), text, font=f, fill=shadow, anchor=anchor)
    d.text(xy, text, font=f, fill=fill, anchor=anchor)


PANEL_FILL = (255, 255, 255, 190)  # translucent: the background lattice shows through (user: transparency)


def grad_rrect(d, x0, y0, x1, y1, top, bottom, r):
    """Rounded rectangle with a vertical gradient, pasted onto the draw's image."""
    w, h = int((x1 - x0) * S), int((y1 - y0) * S)
    tile = Image.new("RGBA", (w, h))
    td = ImageDraw.Draw(tile)
    for yy in range(h):
        t = yy / max(1, h - 1)
        td.line([(0, yy), (w, yy)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(len(top))))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=r * S, fill=255)
    if len(top) == 4:  # keep the gradient's own alpha inside the mask
        mask = Image.composite(tile.getchannel("A"), mask, mask)
    d._image.paste(tile, (int(x0 * S), int(y0 * S)), mask)


PANEL_EDGE_SOFT = (176, 208, 156)


def panel(d, x, y, w, h, fill=None, edge=None, r=6, glossy=False):
    """Quiet panel like the system apps (round 7: "too vectorized / too much going on"): a light, nearly flat
    fill, one thin border, a faint shadow. Buttons (glossy) = Bank-style light grey bars."""
    soft_shadow(d, x, y, x + w, y + h, r)
    if fill is not None:
        d.rounded_rectangle(sc(x, y, x + w, y + h), radius=r * S, fill=fill)
    elif glossy:
        grad_rrect(d, x, y, x + w, y + h, (252, 252, 252, 255), (230, 233, 228, 255), r)
    else:
        grad_rrect(d, x, y, x + w, y + h, (252, 253, 250, 236), (244, 248, 240, 228), r)
    d.rounded_rectangle(sc(x, y, x + w, y + h), radius=r * S,
                        outline=edge if edge is not None else ((182, 188, 180) if glossy else PANEL_EDGE_SOFT),
                        width=S)


GREEN_BAND = ((82, 144, 68), (56, 110, 46))  # round 7: lower contrast
RED_BAND = ((196, 64, 52), (150, 36, 32))


def band(d, x, y, w, h, text, size=12, colors=GREEN_BAND, fade=None):
    """Plain banner, quiet gradient (rounds 6-7). `fade` is ignored (kept so callers need no change)."""
    top, bottom = colors
    soft_shadow(d, x, y, x + w, y + h, 3, off=(0, 1), blur=1.5, alpha=50)
    grad_rrect(d, x, y, x + w, y + h, top, bottom, 3)
    f = font(size)
    shadow_text(d, ((x + w / 2) * S, (y + h / 2) * S), text, f, BAND_TEXT, "mm", shadow=(0, 0, 0, 60))


def info_box(d, x, y, w, h, title, value, size=10):
    """Bank-style info box (the "Free trial period!" box the user circled): soft rounded box, title in bold,
    the value under it (round 7: "Walking since" bold, the date like 11/18/26 under it)."""
    d.rounded_rectangle(sc(x, y, x + w, y + h), radius=6 * S, fill=(255, 255, 255, 235), outline=(150, 196, 128),
                        width=S)
    lines = title.split("\n")
    n = len(lines) + 1
    step = size + 3
    y0 = y + h / 2 - (n - 1) * step / 2
    for i, t in enumerate(lines):
        label(d, x + w / 2, y0 + i * step, t, size, TEXT, "Bold", "mm")
    label(d, x + w / 2, y0 + len(lines) * step, value, size, TEXT, "Medium", "mm")


def label(d, x, y, text, size=11, color=TEXT, weight="SemiBold", anchor="la"):
    d.text(sc(x, y), text, font=font(size, weight), fill=color, anchor=anchor)


def bullet(d, x, y, text, size=11):
    d.rectangle(sc(x, y + 3, x + 5, y + 8), fill=BAND)
    label(d, x + 9, y, text, size)


KEY_COLOURS = {"A": (214, 62, 52), "B": (226, 176, 36), "X": (56, 116, 206), "Y": (64, 156, 76)}  # New 3DS


def key_badge(d, cx, cy, key, r=8):
    """Button letter as a shaded round badge, colourless (round 6: "remove the colored backgrounds")."""
    soft_shadow(d, cx - r, cy - r, cx + r, cy + r, 0, off=(0.5, 1.5), blur=1.5, alpha=110, ellipse=True)
    light, dark = (252, 252, 252), (176, 180, 186)
    for i in range(int(2 * r * S)):  # vertical gradient inside the circle
        t = i / (2 * r * S)
        col = tuple(int(light[k] + (dark[k] - light[k]) * t) for k in range(3))
        yy = (cy - r) * S + i
        half = ((r * S) ** 2 - (yy - cy * S) ** 2) ** 0.5 if abs(yy - cy * S) < r * S else 0
        d.line([(cx * S - half, yy), (cx * S + half, yy)], fill=col)
    d.ellipse(sc(cx - r, cy - r, cx + r, cy + r), outline=(140, 144, 152, 255), width=S)
    d.ellipse(sc(cx - r * 0.65, cy - r * 0.85, cx + r * 0.65, cy - r * 0.05), fill=(255, 255, 255, 110))  # gloss
    shadow_text(d, sc(cx, cy + 0.3), key, font(10, "Bold"), (58, 62, 70), "mm", shadow=(255, 255, 255, 160))


def button(d, x, y, w, h, text, key=None, selected=False):
    panel(d, x, y, w, h, edge=ACCENT if selected else None, r=3, glossy=True)
    d.rectangle(sc(x + 3, y + 4, x + 6, y + h - 4), fill=(70, 130, 62))  # Bank-style left stripe, flat
    f = font(12)
    tw = d.textlength(text, font=f)
    d.text(((x + w / 2) * S - tw / 2, (y + h / 2) * S), text, font=f, fill=TEXT, anchor="lm")
    if key:
        key_badge(d, x + w - 15, y + h / 2, key)
    if selected:  # our bracket marks
        for sx in (x - 4, x + w + 4):
            dx = 4 if sx < x else -4
            d.line(sc(sx, y + 2, sx, y + h - 2), fill=ACCENT, width=2 * S)
            d.line(sc(sx, y + 2, sx + dx, y + 2), fill=ACCENT, width=2 * S)
            d.line(sc(sx, y + h - 2, sx + dx, y + h - 2), fill=ACCENT, width=2 * S)


def icon(num, size=32):
    p = os.path.join(ROOT, "art_src", "sprites5to7", "small", f"{num}.png")
    im = Image.open(p).convert("RGBA")
    if im.width > 40:  # animated Gen V icons: 2 frames side by side or stacked - first frame
        im = im.crop((0, 0, 32, 32)) if im.height >= 32 else im
    if im.height > 40:
        im = im.crop((0, 0, im.width, im.width))
    bb = im.getbbox()
    if bb:
        im = im.crop(bb)
    f = min(1.0, size / max(im.size))
    if f < 1:
        im = im.resize((max(1, int(im.width * f)), max(1, int(im.height * f))), Image.NEAREST)
    return im.resize((im.width * S, im.height * S), Image.NEAREST)


def paste_icon(im, num, cx, cy):
    ic = icon(num)
    # soft oval shadow under the Pokemon (round 4)
    hw = ic.width / S / 2
    hh = ic.height / S / 2
    soft_shadow(ImageDraw.Draw(im, "RGBA"), cx - hw * 0.8, cy + hh - 4, cx + hw * 0.8, cy + hh + 1, 0, off=(0, 0),
                blur=1.5, alpha=90, ellipse=True)
    im.paste(ic, (int(cx * S - ic.width / 2), int(cy * S - ic.height / 2)), ic)


def walker_lcd(im, d, x, y, dex, route_text, scale=2):
    """The walker's screen, just the Pokemon (user 2026-11-15): the 64x48 large sprite in a tight frame.
    route_text is no longer drawn."""
    w, h = 64, 48
    soft_shadow(d, x - 6, y - 6, x + w * scale + 6, y + h * scale + 6, 8, blur=3, alpha=110)
    grad_rrect(d, x - 6, y - 6, x + w * scale + 6, y + h * scale + 6, (98, 98, 108), (48, 48, 54), 8)
    d.line(sc(x - 2, y - 4.5, x + w * scale + 2, y - 4.5), fill=(255, 255, 255, 70), width=S)
    d.rectangle(sc(x, y, x + w * scale, y + h * scale), fill=LCD_BG)
    d.line(sc(x, y + 0.5, x + w * scale, y + 0.5), fill=(0, 0, 0, 60), width=S)  # inner shadow on the glass
    data = open(os.path.join(ROOT, "sd_out", "3ds", "walkerbox", "sprites", f"{dex:03d}.bin"), "rb").read()
    sh = pwimg.decode(data[0x180 + FRAME * 0x300:0x480 + FRAME * 0x300], 64, 48)  # round 7: animated
    px = scale * S
    for yy in range(48):
        for xx in range(64):
            v = sh[yy * 64 + xx]
            if v:
                X, Y = x * S + xx * px, y * S + yy * px
                d.rectangle([X, Y, X + px - 1, Y + px - 1], fill=LCD_SHADES[v])
    lcd_grid(d, x, y, w, h, px)


def lcd_texture(dex, k=6):
    """The walker's 64x48 screen as an image (LCD colours + grid), for mapping in perspective."""
    data = open(os.path.join(ROOT, "sd_out", "3ds", "walkerbox", "sprites", f"{dex:03d}.bin"), "rb").read()
    sh = pwimg.decode(data[0x180 + FRAME * 0x300:0x480 + FRAME * 0x300], 64, 48)
    tex = Image.new("RGBA", (64 * k, 48 * k), LCD_BG + (255,))
    td = ImageDraw.Draw(tex)
    for yy in range(48):
        for xx in range(64):
            v = sh[yy * 64 + xx]
            if v:
                td.rectangle([xx * k, yy * k, xx * k + k - 1, yy * k + k - 1], fill=LCD_SHADES[v])
    for i in range(1, 64):
        td.line([(i * k, 0), (i * k, 48 * k)], fill=(0, 0, 0, 40))
    for j in range(1, 48):
        td.line([(0, j * k), (64 * k, j * k)], fill=(0, 0, 0, 40))
    return tex


def lcd_grid(d, x, y, w, h, px):
    """LCD pixel grid over a walker screen (user, round 4): a faint dark line between every LCD pixel."""
    X0, Y0 = x * S, y * S
    for i in range(1, w):
        d.line([(X0 + i * px, Y0), (X0 + i * px, Y0 + h * px - 1)], fill=(0, 0, 0, 34), width=1)
    for j in range(1, h):
        d.line([(X0, Y0 + j * px), (X0 + w * px - 1, Y0 + j * px)], fill=(0, 0, 0, 34), width=1)


def device(top, bottom):
    pad = 8
    out = Image.new("RGB", (TOP_W * S + 2 * pad * S, (2 * H + 3 * pad) * S), (44, 44, 50))
    out.paste(top, (pad * S, pad * S))
    out.paste(bottom, (((TOP_W - BOT_W) // 2 + pad) * S, (H + 2 * pad) * S))
    return out


def app_icon(d, x, y, s=12):
    """WalkerBox's own little app mark: a round walker with a green screen (our drawing)."""
    d.ellipse(sc(x, y, x + s, y + s), fill=(250, 250, 252), outline=(150, 156, 168), width=1)
    d.rounded_rectangle(sc(x + s * 0.25, y + s * 0.22, x + s * 0.75, y + s * 0.58), radius=1 * S,
                        fill=(150, 196, 128), outline=(60, 62, 70), width=1)
    for bx in (0.33, 0.5, 0.67):
        d.ellipse(sc(x + s * bx - 0.9, y + s * 0.74 - 0.9, x + s * bx + 0.9, y + s * 0.74 + 0.9), fill=(150, 156, 168))


def status_bar(d, w, text):
    """Header (round 6, "more like an official app"): app mark + name left, page title right."""
    soft_shadow(d, 0, 0, w, 16, 0, off=(0, 1.5), blur=2, alpha=90)
    grad_rrect(d, 0, 0, w, 16, (70, 128, 60), (34, 74, 28), 0)
    d.line(sc(0, 0.5, w, 0.5), fill=(255, 255, 255, 60), width=S)
    d.line(sc(0, 15.5, w, 15.5), fill=(20, 44, 16, 200), width=S)
    app_icon(d, 4, 2, 12)
    shadow_text(d, sc(20, 8), APP_NAME, font(9, "Bold"), (240, 250, 232), "lm")
    shadow_text(d, sc(w - 6, 8), text, font(9, "Medium"), (230, 246, 220), "rm")


def footer(d, w, left=(), right=()):
    """Bottom-screen footer with button hints, like the system's own apps (round 6)."""
    y = H - 24
    soft_shadow(d, 0, y, w, H, 0, off=(0, -1.5), blur=2, alpha=70)
    grad_rrect(d, 0, y, w, H, (248, 250, 246), (214, 224, 208), 0)
    d.line(sc(0, y + 0.5, w, y + 0.5), fill=(140, 170, 124, 255), width=S)
    x = 8
    for key, text in left:
        key_badge(d, x + 7, y + 12, key, 7)
        label(d, x + 18, y + 12, text, 10, TEXT, "Bold", "lm")
        x += 26 + d.textlength(text, font=font(10, "Bold")) / S
    x = w - 8
    for key, text in reversed(right):
        tw = d.textlength(text, font=font(10, "Bold")) / S
        label(d, x, y + 12, text, 10, TEXT, "Bold", "rm")
        key_badge(d, x - tw - 11, y + 12, key, 7)
        x -= tw + 32


# ------------------------------------------------------------------ screens

def home():
    top = background(TOP_W)
    d = ImageDraw.Draw(top, "RGBA")
    """No Storage (user 2026-11-15): the walk is the centre; one main button (Send / End walk)."""
    status_bar(d, TOP_W, "Trainer Sam · ID 48213")
    panel(d, 8, 22, 384, 124)
    band(d, 8, 22, 384, 20, "Your walker", 12, RED_BAND)
    walker_lcd(top, d, 24, 56, 706, "", 1)
    label(d, 104, 52, "Pudding is on a walk", 13, TEXT, "Bold")
    facts = [("Route", "Test Path"), ("Watts kept", "173"), ("Training", "Speed (+1 per 20 steps)")]
    for i, (k, v) in enumerate(facts):
        cx, cy = 104 + (i % 2) * 104, 72 + (i // 2) * 24
        label(d, cx, cy, k, 8, SUB, "SemiBold")
        label(d, cx, cy + 9, v, 10, TEXT, "Bold")
    info_box(d, 300, 50, 84, 42, "Walking since", "11/18/26")  # round 7
    game_icon(d, 104, 120, 20)
    label(d, 130, 130, "Goes back to Pokémon X (Nate) at walk end", 9, SUB, "Medium", "lm")
    panel(d, 8, 154, 384, 80)
    band(d, 8, 154, 384, 20, "Games on this 3DS")
    for i, g in enumerate(["Pokémon Blue", "Pokémon Crystal", "Pokémon X", "Alpha Sapphire"]):
        cx = 56 + i * 96
        game_icon(d, cx - 16, 180, 32)
        label(d, cx, 222, g, 9, TEXT, "SemiBold", "mm")
    bot = background(BOT_W)
    d = ImageDraw.Draw(bot, "RGBA")
    band(d, 0, 8, 320, 22, "What would you like to do?", 12)
    # the one main action: End walk while a Pokemon walks, else Send to walker
    panel(d, 12, 40, 296, 70, edge=ACCENT, glossy=True)
    grad_rrect(d, 16, 46, 21, 104, (110, 176, 92), (40, 88, 34), 1)
    d.rounded_rectangle(sc(30, 52, 76, 98), radius=6 * S, fill=(255, 246, 236, 255), outline=PANEL_EDGE, width=S)
    paste_icon(bot, 706, 53, 75)
    label(d, 88, 62, "End walk", 15, TEXT, "Bold")
    label(d, 88, 84, "Bring Pudding back to Pokémon X", 10, SUB, "Medium")
    cx, cy = 290, 75
    key_badge(d, cx, cy, "A", 10)
    for sx in (8, 312):  # brackets on the main action
        dx = 4 if sx < 100 else -4
        d.line(sc(sx, 42, sx, 108), fill=ACCENT, width=2 * S)
        d.line(sc(sx, 42, sx + dx, 42), fill=ACCENT, width=2 * S)
        d.line(sc(sx, 108, sx + dx, 108), fill=ACCENT, width=2 * S)
    tiles = [("Walker tools", "Link test, backup,", "restore, reset"), ("Sprites", "Every walker", "sprite"),
             ("Settings", "Trainer, guard,", "game backups")]
    for i, (t, s1, s2) in enumerate(tiles):
        x = 12 + i * 100
        panel(d, x, 120, 96, 70, glossy=True)
        label(d, x + 48, 140, t, 11, TEXT, "Bold", "mm")
        label(d, x + 48, 160, s1, 8, SUB, "Medium", "mm")
        label(d, x + 48, 172, s2, 8, SUB, "Medium", "mm")
    footer(d, BOT_W, [("B", "Close")], [("A", "Select")])
    return device(top, bot)


def game_select(game="Pokémon X", facts=(("Trainer: Nate", "ID No. 11111"), ("Game card", "Generation 6")),
                popup=False):
    top = background(TOP_W)
    d = ImageDraw.Draw(top, "RGBA")
    status_bar(d, TOP_W, "Send to walker")
    # the games' own HOME-menu icons (from their SMDH at run time; placeholders here): the chosen one in the
    # middle with red brackets, its neighbours smaller (L / R)
    for cx, size, chosen in ((120, 34, False), (200, 48, True), (280, 34, False)):
        soft_shadow(d, cx - size / 2, 54 - size / 2, cx + size / 2, 54 + size / 2, 6, blur=2, alpha=90)
        game_icon(d, cx - size / 2, 54 - size / 2, size)
        if not chosen:
            d.rounded_rectangle(sc(cx - size / 2, 54 - size / 2, cx + size / 2, 54 + size / 2), radius=6 * S,
                                fill=(230, 240, 222, 120))
    for sx, sy, dx, dy in ((170, 24, 1, 1), (230, 24, -1, 1), (170, 84, 1, -1), (230, 84, -1, -1)):
        d.line(sc(sx, sy, sx + 8 * dx, sy), fill=ACCENT, width=2 * S)
        d.line(sc(sx, sy, sx, sy + 8 * dy), fill=ACCENT, width=2 * S)
    band(d, 0, 90, 400, 22, game, 13, RED_BAND, fade=40)
    panel(d, 12, 118, 376, 60)
    bullet(d, 22, 128, facts[0][0])
    bullet(d, 210, 128, facts[0][1])
    bullet(d, 22, 152, facts[1][0])
    bullet(d, 210, 152, facts[1][1])
    panel(d, 12, 186, 376, 44)
    label(d, 24, 199, "Choose the game whose Pokémon goes for a walk.", 11, TEXT, "Medium")
    label(d, 24, 215, "L / R: other games (4 found)", 10, SUB, "Medium")
    bot = background(BOT_W)
    d = ImageDraw.Draw(bot, "RGBA")
    button(d, 22, 92, 276, 38, "Use this game", "A", selected=True)
    footer(d, BOT_W, [("B", "Back")], [("L", "Previous"), ("R", "Next")])
    if popup:
        # user (round 5): a game with no routes yet -> "Not yet compatible" when it is chosen
        d.rectangle(sc(0, 0, BOT_W, H), fill=(10, 24, 8, 140))  # dim what is behind
        panel(d, 24, 44, 272, 152, glossy=True)
        band(d, 24, 44, 272, 24, "Not yet compatible", 12, RED_BAND)
        label(d, 160, 88, f"{game} has no walker", 11, TEXT, "SemiBold", "mm")
        label(d, 160, 104, "routes yet, so its Pokémon", 11, TEXT, "SemiBold", "mm")
        label(d, 160, 120, "can't go for a walk.", 11, TEXT, "SemiBold", "mm")
        label(d, 160, 140, "Routes come one game at a time.", 9, SUB, "Medium", "mm")
        button(d, 90, 156, 140, 30, "OK", "A", selected=True)
    return device(top, bot)


def not_compatible():
    return game_select("Pokémon Crystal", (("Trainer: SAM", "ID No. 37070"), ("Virtual Console", "Generation 2")),
                       popup=True)


def box_view():
    top = background(TOP_W)
    d = ImageDraw.Draw(top, "RGBA")
    status_bar(d, TOP_W, "Storage - Pokémon X")
    panel(d, 12, 26, 196, 202)
    walker_lcd(top, d, 26, 40, 706, "Pudding", 1)
    label(d, 110, 120, "As on the walker", 9, SUB, "Medium", "mm")
    paste_icon(top, 706, 110, 160)
    panel(d, 216, 26, 172, 202)
    band(d, 216, 26, 172, 22, "Goodra   Lv.100", 12)
    bullet(d, 226, 58, "Nickname: Pudding")
    bullet(d, 226, 80, "Female  ·  Shiny")
    bullet(d, 226, 102, "Dex No. 706")
    bullet(d, 226, 124, "OT: Nate")
    bullet(d, 226, 146, "ID No. 11111")
    bullet(d, 226, 168, "From: Pokémon X")
    d.rounded_rectangle(sc(226, 194, 378, 216), radius=4 * S, fill=(255, 236, 228), outline=ACCENT, width=S)
    label(d, 302, 205, "On a walk", 11, ACCENT, "Bold", "mm")
    bot = background(BOT_W)
    d = ImageDraw.Draw(bot, "RGBA")
    band(d, 30, 6, 260, 22, "WalkerBox  1", 13)
    for side, x in (("<", 14), (">", 306)):
        d.polygon(sc(*( (x + 6, 10, x + 6, 24, x - 2, 17) if side == "<" else (x - 6, 10, x - 6, 24, x + 2, 17))),
                  fill=BAND)
    mons = [706, 658, 495, 501, 724, 727, 730, 571, 609, 635, 700, 778, 745, 653, 656, 785, 800, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    gx, gy, cw, ch = 22, 34, 46, 30
    for i, m in enumerate(mons):
        cx, cy = gx + (i % 6) * cw + cw / 2, gy + (i // 6) * ch + ch / 2
        if i == 0:
            d.rounded_rectangle(sc(cx - 21, cy - 14, cx + 21, cy + 14), radius=4 * S, fill=(255, 246, 236),
                                outline=ACCENT, width=2 * S)
        if m:
            paste_icon(bot, m, cx, cy)
        else:
            d.ellipse(sc(cx - 2, cy - 2, cx + 2, cy + 2), fill=LATTICE)
        if i == 0:  # walking mark
            d.ellipse(sc(cx + 10, cy - 13, cx + 19, cy - 4), fill=ACCENT)
            label(d, cx + 14.5, cy - 8.5, "W", 6, (255, 255, 255), "Bold", "mm")
    button(d, 6, 200, 100, 32, "Game box", "X")
    button(d, 110, 200, 100, 32, "Send", "A", selected=True)
    button(d, 214, 200, 100, 32, "Back", "B")
    return device(top, bot)


GAME_BOX = [658, 495, 501, 724, 727, 730, 571, 609, 635, 700, 778, 745, 653, 656, 785, 800, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]


def game_box_bottom(walking=None, cursor=0):
    """Bottom screen: the game's box (6 x 5) + the one WalkerBox slot on the right."""
    bot = background(BOT_W)
    d = ImageDraw.Draw(bot, "RGBA")
    band(d, 26, 6, 200, 22, "Pokémon X · Box 1", 12)
    d.polygon(sc(18, 10, 18, 24, 10, 17), fill=BAND)
    d.polygon(sc(234, 10, 234, 24, 242, 17), fill=BAND)
    gx, gy, cw, ch = 8, 34, 40, 31
    for i, m in enumerate(GAME_BOX):
        cx, cy = gx + (i % 6) * cw + cw / 2, gy + (i // 6) * ch + ch / 2
        if i == cursor:
            d.rounded_rectangle(sc(cx - 19, cy - 14, cx + 19, cy + 14), radius=4 * S, fill=(255, 246, 236),
                                outline=ACCENT, width=2 * S)
        if m:
            paste_icon(bot, m, cx, cy)
        else:
            d.ellipse(sc(cx - 2, cy - 2, cx + 2, cy + 2), fill=LATTICE)
    # the one WalkerBox slot: the Pokemon on a walk
    x0, y0, x1, y1 = 254, 6, 314, 190
    panel(d, x0, y0, x1 - x0, y1 - y0, edge=ACCENT if cursor == -1 else PANEL_EDGE)
    grad_rrect(d, x0, y0, x1, y0 + 22, GREEN_BAND[0], GREEN_BAND[1], 4)
    d.line(sc(x0 + 4, y0 + 1, x1 - 4, y0 + 1), fill=(255, 255, 255, 90), width=S)
    shadow_text(d, sc((x0 + x1) / 2, y0 + 11), "Walker", font(11, "Bold"), BAND_TEXT, "mm")
    # a small walker drawn by us
    d.ellipse(sc(266, 40, 302, 76), fill=(236, 236, 236), outline=(120, 120, 120), width=S)
    d.rectangle(sc(274, 50, 294, 64), fill=LCD_BG)
    if walking:
        d.rounded_rectangle(sc(262, 88, 306, 124), radius=4 * S, fill=(255, 246, 236),
                            outline=ACCENT if cursor == -1 else PANEL_EDGE, width=S * (2 if cursor == -1 else 1))
        paste_icon(bot, walking, 284, 106)
        info_box(d, 259, 134, 50, 40, "Walking\nsince", "11/18/26", size=8)
    else:
        d.rounded_rectangle(sc(262, 88, 306, 124), radius=4 * S, outline=PANEL_EDGE, width=S)
        label(d, 284, 106, "free", 10, SUB, "Medium", "mm")
        label(d, 284, 146, "Pick a", 9, SUB, "Medium", "mm")
        label(d, 284, 160, "Pokémon", 9, SUB, "Medium", "mm")
        label(d, 284, 174, "to send", 9, SUB, "Medium", "mm")
    return bot, d


def small_lcd(d, x, y, dex, frame, scale=1):
    """The walker's small (32x24) sprite on a mini LCD."""
    data = open(os.path.join(ROOT, "sd_out", "3ds", "walkerbox", "sprites", f"{dex:03d}.bin"), "rb").read()
    sh = pwimg.decode(data[frame * 0xC0:(frame + 1) * 0xC0], 32, 24)
    soft_shadow(d, x - 3, y - 3, x + 32 * scale + 3, y + 24 * scale + 3, 4, blur=2, alpha=100)
    grad_rrect(d, x - 3, y - 3, x + 32 * scale + 3, y + 24 * scale + 3, (98, 98, 108), (48, 48, 54), 4)
    d.rectangle(sc(x, y, x + 32 * scale, y + 24 * scale), fill=LCD_BG)
    px = scale * S
    for yy in range(24):
        for xx in range(32):
            v = sh[yy * 32 + xx]
            if v:
                X, Y = x * S + xx * px, y * S + yy * px
                d.rectangle([X, Y, X + px - 1, Y + px - 1], fill=LCD_SHADES[v])
    lcd_grid(d, x, y, 32, 24, px)


def game_icon(d, x, y, size=36):
    """Where the game's HOME-menu icon goes (read from the title's SMDH on the 3DS; placeholder here)."""
    grad_rrect(d, x, y, x + size, y + size, (250, 250, 250), (214, 222, 230), 6)
    d.rounded_rectangle(sc(x, y, x + size, y + size), radius=6 * S, outline=(150, 160, 170), width=S)
    label(d, x + size / 2, y + size / 2 - 5, "HOME", 7, (120, 130, 140), "Bold", "mm")
    label(d, x + size / 2, y + size / 2 + 5, "icon", 7, (120, 130, 140), "Bold", "mm")


def gauge(d, x, y, w, name, value, maximum, text, color=BAND):
    label(d, x, y, name, 9, SUB, "SemiBold")
    label(d, x + w, y, text, 9, TEXT, "Bold", "ra")
    d.rounded_rectangle(sc(x, y + 12, x + w, y + 17), radius=2 * S, fill=(222, 232, 214, 255))
    if value > 0:  # flat bar (round 7)
        d.rounded_rectangle(sc(x, y + 12, x + max(5, w * min(1.0, value / maximum)), y + 17), radius=2 * S,
                            fill=color)


def ev_bars(d, x, y, w, evs, training=None, cap=252):
    names = ["HP", "Atk", "Def", "SpA", "SpD", "Spe"]
    label(d, x, y, "EVs", 9, SUB, "SemiBold")
    label(d, x + w, y, f"{sum(evs)} / 510", 9, TEXT, "Bold", "ra")
    for i, (n, v) in enumerate(zip(names, evs)):
        yy = y + 13 + i * 10
        label(d, x, yy + 4, n, 8, ACCENT if n == training else SUB, "Bold", "lm")
        bx = x + 26
        d.rounded_rectangle(sc(bx, yy + 2, x + w - 22, yy + 7), radius=2 * S, fill=(222, 232, 214, 255))
        if v:  # flat bar (round 7)
            c = ACCENT if n == training else BAND
            d.rounded_rectangle(sc(bx, yy + 2, bx + max(4, (x + w - 22 - bx) * v / cap), yy + 7), radius=2 * S,
                                fill=c)
        label(d, x + w, yy + 4, str(v), 8, TEXT, "Bold", "rm")


def pokemon_top(dex, title, gender, info, gauges, evs, training, game, walking=False):
    """Top screen of the game box (user: game icons, more bevel, transparency, no empty space)."""
    top = background(TOP_W)
    d = ImageDraw.Draw(top, "RGBA")
    status_bar(d, TOP_W, f"{game} · Nate")
    # left: the walker pictures
    panel(d, 8, 22, 188, 212)
    walker_lcd(top, d, 38, 34, dex, "", 2)
    label(d, 102, 146, "On the walker" if walking else "How it will look on the walker", 9, SUB, "Medium", "mm")
    small_lcd(d, 40, 162, dex, FRAME)  # round 7: one walker icon (animates with the big one)
    label(d, 56, 196, "walker icon", 8, SUB, "Medium", "mm")
    d.rounded_rectangle(sc(118, 156, 182, 192), radius=6 * S, fill=(255, 255, 255, 140), outline=PANEL_EDGE, width=S)
    paste_icon(top, dex, 150, 174)
    label(d, 150, 200, "box icon", 8, SUB, "Medium", "mm")
    game_icon(d, 18, 206, 22)
    label(d, 46, 212, game, 9, TEXT, "Bold", "lm")
    label(d, 46, 224, "Nate · ID 11111", 8, SUB, "Medium", "lm")
    # right: the Pokemon
    panel(d, 204, 22, 188, 212)
    band(d, 204, 22, 188, 22, title, 12)
    # Console Square has no gender signs: DejaVu for those two characters only
    d.text(sc(384, 33), gender, font=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 13 * S),
           fill=(150, 210, 255) if gender == "♂" else (255, 170, 190), anchor="rm")
    for i, (k, v) in enumerate(info):
        cx, cy = 214 + (i % 2) * 90, 50 + (i // 2) * 22
        if k == "Walking since":  # round 7: title bold, the date under it
            label(d, cx, cy, k, 9, TEXT, "Bold")
            label(d, cx, cy + 10, v, 10, TEXT, "Medium")
            continue
        label(d, cx, cy, k, 8, SUB, "SemiBold")
        label(d, cx, cy + 9, v, 10, TEXT, "Bold")
    gy = 50 + ((len(info) + 1) // 2) * 22 + 2
    for name, val, mx, text, color in gauges:
        gauge(d, 214, gy, 168, name, val, mx, text, color)
        gy += 22
    ev_bars(d, 214, gy, 168, evs, training)
    return top, d


def game_box():
    """Choosing a Pokemon in a game to send: the walker slot is free."""
    top, d = pokemon_top(658, "Greninja   Lv.36", "♂",
                         [("Nickname", "Shade"), ("Dex No.", "658"), ("Nature", "Jolly"), ("Ability", "Protean")],
                         [("Friendship", 140, 255, "140", BAND), ("Next level", 2210, 3354, "1,144 EXP", (70, 130, 190))],
                         [0, 64, 0, 4, 0, 112], None, "Pokémon X")
    bot, d = game_box_bottom(walking=None, cursor=0)
    footer(d, BOT_W, [("B", "Back")], [("X", "Games"), ("A", "Send")])
    return device(top, bot)


def walking_slot():
    """A Pokemon is on a walk: the cursor on the walker slot shows how it is doing."""
    top, d = pokemon_top(706, "Pudding   Lv.62", "♀",
                         [("Route", "Test Path"), ("Walking since", "11/18/26"), ("Training", "Speed"),
                          ("Steps (last link)", "1,840")],
                         [("Speed EVs from this walk", 92, 252, "+92", ACCENT),
                          ("Next level", 1840, 4100, "2,260 steps", (70, 130, 190))],
                         [0, 0, 0, 0, 0, 92], "Spe", "Pokémon X", walking=True)
    # "on a walk" tag in place of the caption under the walker picture
    d.rounded_rectangle(sc(40, 138, 164, 154), radius=4 * S, fill=(255, 236, 228, 255), outline=ACCENT, width=S)
    label(d, 102, 146, "On a walk now", 9, ACCENT, "Bold", "mm")
    bot, d = game_box_bottom(walking=706, cursor=-1)
    footer(d, BOT_W, [("B", "Back")], [("X", "Games"), ("A", "End walk")])
    return device(top, bot)


def send():
    top = background(TOP_W)
    d = ImageDraw.Draw(top, "RGBA")
    status_bar(d, TOP_W, "Send to walker")
    walker_lcd(top, d, 136, 40, 501, "Test Path", 2)  # still centred on the screen
    panel(d, 12, 172, 376, 56)
    bullet(d, 22, 182, "Oshawott  Lv.12  ·  from Pokémon X (Nate)")
    bullet(d, 22, 204, "Training: Speed  (+1 EV every 20 steps)")
    bot = background(BOT_W)
    d = ImageDraw.Draw(bot, "RGBA")
    band(d, 10, 8, 300, 22, "Route", 12)
    for i, (r, sub) in enumerate([("Test Path", "Lillipup, Pidove, Patrat"), ("(your routes later)", "")]):
        y = 36 + i * 34
        panel(d, 14, y, 292, 28, edge=ACCENT if i == 0 else PANEL_EDGE, r=5)
        label(d, 26, y + 14, r, 12, TEXT if i == 0 else SUB, "Bold", "lm")
        label(d, 296, y + 14, sub, 9, SUB, "Medium", "rm")
    band(d, 10, 108, 300, 22, "Train an EV on this walk", 12)
    stats = ["Off", "HP", "Atk", "Def", "SpA", "SpD", "Spe"]
    for i, s in enumerate(stats):
        x = 14 + i * 42
        sel = s == "Spe"
        d.rounded_rectangle(sc(x, 136, x + 38, 158), radius=5 * S, fill=(255, 246, 236) if sel else PANEL,
                            outline=ACCENT if sel else PANEL_EDGE, width=S * (2 if sel else 1))
        label(d, x + 19, 147, s, 11, TEXT, "Bold", "mm")
    button(d, 14, 172, 292, 34, "Send to walker", "A", selected=True)
    footer(d, BOT_W, [("B", "Back")], [("L", "Route"), ("R", "EV")])
    return device(top, bot)


def draw_3ds_closed(d, x, y, w=154, h=66):
    """A closed 3DS seen from above, back edge (hinge, IR window) on the right (round 5 redraw; our drawing)."""
    soft_shadow(d, x, y, x + w, y + h, 10, off=(2, 4), blur=4, alpha=110)
    grad_rrect(d, x, y, x + w, y + h, (92, 94, 108), (30, 31, 36), 10)
    grad_rrect(d, x + 6, y + 5, x + w - 18, y + h - 5, (118, 120, 136), (52, 54, 62), 7)  # lid surface
    d.polygon(sc(x + 10, y + 7, x + 70, y + 7, x + 40, y + h - 7, x + 10, y + h - 7), fill=(255, 255, 255, 26))
    d.line(sc(x + 10, y + 6, x + w - 22, y + 6), fill=(255, 255, 255, 90), width=S)
    # hinge on the back edge
    for i in range(10):
        t = i / 9
        c = int(46 + 60 * (1 - abs(t - 0.4) * 1.6))
        d.line(sc(x + w - 14 + i, y + 8, x + w - 14 + i, y + h - 8), fill=(c, c, c + 8), width=S)
    # shoulder buttons and the IR window (dark red glass) in the middle of the back edge
    for yy in (y + 2, y + h - 10):
        grad_rrect(d, x + w - 6, yy, x + w + 2, yy + 8, (150, 152, 160), (70, 72, 80), 2)
    grad_rrect(d, x + w - 2, y + h / 2 - 8, x + w + 4, y + h / 2 + 8, (90, 30, 34), (24, 6, 8), 2)
    d.line(sc(x + w - 1, y + h / 2 - 6, x + w - 1, y + h / 2), fill=(255, 255, 255, 110), width=S)
    label(d, x + (w - 14) / 2, y + h + 9, "3DS (closed, from above)", 8, SUB, "Medium", "mm")


def draw_walker(im, d, cx, cy, r=40):
    """The walker from the front, its top (IR window) turned toward the 3DS (round 5 redraw; our drawing)."""
    soft_shadow(d, cx - r, cy - r, cx + r, cy + r, 0, off=(2, 4), blur=4, alpha=110, ellipse=True)
    for i in range(int(r * S), 0, -1):  # body: light from the top left
        t = i / (r * S)
        c = int(255 - 52 * t ** 2)
        ox, oy = -(1 - t) * 8 * S, -(1 - t) * 8 * S
        d.ellipse([cx * S - i + ox, cy * S - i + oy, cx * S + i + ox, cy * S + i + oy], fill=(c, c, min(255, c + 4)))
    d.ellipse(sc(cx - r, cy - r, cx + r, cy + r), outline=(140, 144, 156), width=S)
    d.ellipse(sc(cx - r + 3, cy - r + 3, cx + r - 3, cy + r - 3), outline=(255, 255, 255, 140), width=S)
    # IR window on the side facing the 3DS
    grad_rrect(d, cx - r - 2, cy - 8, cx - r + 5, cy + 8, (90, 30, 34), (24, 6, 8), 2)
    small_lcd(d, cx - 16, cy - 20, 706, 0)  # the walker's screen, with its LCD grid
    for bx, br in ((cx - 13, 3.5), (cx, 4.5), (cx + 13, 3.5)):  # three buttons
        soft_shadow(d, bx - br, cy + 18 - br, bx + br, cy + 18 + br, 0, off=(0.5, 1), blur=1, alpha=110,
                    ellipse=True)
        d.ellipse(sc(bx - br, cy + 18 - br, bx + br, cy + 18 + br), fill=(214, 218, 226), outline=(130, 134, 146),
                  width=S)
        d.ellipse(sc(bx - br * 0.6, cy + 18 - br * 0.8, bx + br * 0.3, cy + 18 - br * 0.1), fill=(255, 255, 255, 160))
    label(d, cx, cy + r + 9, "walker", 8, SUB, "Medium", "mm")


def poly_grad(d, pts, top, bottom):
    """Polygon (native coords) filled with a vertical gradient."""
    ys = [p[1] for p in pts]
    xs = [p[0] for p in pts]
    x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
    w, h = int((x1 - x0) * S) + 2, int((y1 - y0) * S) + 2
    tile = Image.new("RGBA", (w, h))
    td = ImageDraw.Draw(tile)
    for yy in range(h):
        t = yy / max(1, h - 1)
        td.line([(0, yy), (w, yy)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).polygon([((px - x0) * S, (py - y0) * S) for px, py in pts], fill=255)
    d._image.paste(tile, (int(x0 * S), int(y0 * S)), mask)


def quad_pt(q, u, v):
    """Point in a quad (a, b, c, d) = (front-left, front-right, back-right, back-left): u across, v front->back."""
    a, b, c, dd = q
    f = (a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u)
    k = (dd[0] + (c[0] - dd[0]) * u, dd[1] + (c[1] - dd[1]) * u)
    return (f[0] + (k[0] - f[0]) * v, f[1] + (k[1] - f[1]) * v)


def sub_quad(q, u0, u1, v0, v1):
    return [quad_pt(q, u0, v0), quad_pt(q, u1, v0), quad_pt(q, u1, v1), quad_pt(q, u0, v1)]


def draw_3ds_open(im, d):
    """An open 3DS in a three-quarter view (round 6, laid out like the user's reference; our own drawing)."""
    A, B, C, D = (66, 126), (220, 140), (268, 112), (116, 100)  # base top face
    T = 7  # thickness
    soft_shadow(d, 56, 112, 280, 156, 20, off=(4, 4), blur=6, alpha=90, ellipse=True)
    poly_grad(d, [A, B, (B[0], B[1] + T), (A[0], A[1] + T)], (222, 224, 230), (176, 180, 188))  # front face
    poly_grad(d, [B, C, (C[0], C[1] + T), (B[0], B[1] + T)], (200, 204, 212), (150, 154, 164))  # right face
    poly_grad(d, [A, B, C, D], (250, 250, 252), (226, 228, 234))  # top face
    base = (A, B, C, D)
    poly_grad(d, sub_quad(base, 0.3, 0.72, 0.18, 0.86), (60, 62, 70), (30, 32, 38))  # bottom screen bezel
    poly_grad(d, sub_quad(base, 0.33, 0.69, 0.24, 0.8), (178, 214, 150), (128, 176, 104))  # bottom screen
    cp = quad_pt(base, 0.15, 0.62)  # circle pad
    d.ellipse(sc(cp[0] - 9, cp[1] - 5, cp[0] + 9, cp[1] + 5), fill=(205, 208, 214), outline=(150, 154, 164),
              width=S)
    for du, dv in ((0.0, 0.1), (0.05, 0.0), (-0.05, 0.0), (0.0, -0.1)):  # ABXY, colourless
        p = quad_pt(base, 0.86 + du, 0.6 + dv)
        d.ellipse(sc(p[0] - 3, p[1] - 2, p[0] + 3, p[1] + 2), fill=(214, 216, 222), outline=(140, 144, 152), width=1)
    # lid, opened up and a little back, with its right edge showing
    C2, D2 = (258, 40), (104, 33)  # kept inside the panel (top edge y 26)
    poly_grad(d, [C, C2, (C2[0] + 6, C2[1] + 4), (C[0] + 6, C[1] + 3)], (196, 200, 208), (150, 154, 164))
    poly_grad(d, [D, C, C2, D2], (236, 238, 242), (252, 252, 254))
    lid = (D, C, C2, D2)  # here "front" = hinge, "back" = the lid's top edge
    poly_grad(d, sub_quad(lid, 0.1, 0.9, 0.12, 0.88), (52, 54, 62), (28, 30, 36))  # top screen bezel
    poly_grad(d, sub_quad(lid, 0.14, 0.86, 0.17, 0.83), (150, 196, 128), (196, 228, 170))  # top screen, lit
    for v in (0.32, 0.48, 0.64):  # a hint of WalkerBox's own screen on it
        poly_grad(d, sub_quad(lid, 0.22, 0.78, v, v + 0.08), (250, 252, 248), (232, 240, 226))
    poly_grad(d, sub_quad(lid, 0.18, 0.82, 0.72, 0.8), (96, 162, 80), (38, 86, 32))
    # hinge
    d.line(sc(D[0] + 2, D[1] + 1, C[0] - 2, C[1] + 1), fill=(170, 174, 184), width=4 * S)
    d.line(sc(D[0] + 2, D[1], C[0] - 2, C[1]), fill=(236, 238, 242), width=S)
    # IR window: dark red glass on the right side near the back
    poly_grad(d, [(C[0] - 9, C[1] + 6), (C[0] - 2, C[1] + 2), (C[0] - 2, C[1] + 7), (C[0] - 9, C[1] + 11)],
              (110, 34, 40), (30, 8, 10))
    return (C[0] - 5, C[1] + 6)  # where the IR window is


def draw_walker_angled(im, d, cx, cy, rx=30, ry=24):
    """The walker turned a little, its top edge (IR window) aimed at the 3DS (round 6)."""
    soft_shadow(d, cx - rx, cy - ry, cx + rx, cy + ry, 0, off=(3, 5), blur=5, alpha=110, ellipse=True)
    d.ellipse(sc(cx - rx + 3, cy - ry + 4, cx + rx + 3, cy + ry + 4), fill=(150, 154, 166))  # its thickness
    for i in range(int(rx * S), 0, -1):  # body, lit from the top left
        t = i / (rx * S)
        c = int(255 - 50 * t ** 2)
        o = -(1 - t) * 7 * S
        d.ellipse([cx * S - i + o, cy * S - i * ry / rx + o, cx * S + i + o, cy * S + i * ry / rx + o],
                  fill=(c, c, min(255, c + 4)))
    d.ellipse(sc(cx - rx, cy - ry, cx + rx, cy + ry), outline=(140, 144, 156), width=S)
    small_lcd(d, cx - 14, cy - 15, 706, 0, scale=0.875)
    for bx in (cx - 11, cx, cx + 11):
        r = 3.6 if bx == cx else 3
        d.ellipse(sc(bx - r, cy + 13 - r * 0.8, bx + r, cy + 13 + r * 0.8), fill=(214, 218, 226),
                  outline=(130, 134, 146), width=S)
    # IR window on the top-left edge, facing the 3DS
    win = (cx - rx * 0.78, cy - ry * 0.6)
    d.ellipse(sc(win[0] - 5, win[1] - 3, win[0] + 5, win[1] + 3), fill=(40, 10, 14), outline=(90, 30, 34), width=S)
    return win


def ir_beams(im, x, cy, x_end, start=-38, end=38, radii=(12, 24, 36)):
    """Glowing IR arcs around (x, cy) between the angles start..end."""
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    for k, rr in enumerate(radii):
        box = sc(x - rr, cy - rr, x + rr, cy + rr)
        ld.arc(box, start, end, fill=ACCENT + (255,), width=int(2.5 * S))
    glow = layer.filter(ImageFilter.GaussianBlur(3 * S))
    im.paste(glow, (0, 0), glow)
    im.paste(layer, (0, 0), layer)


def connect():
    top = background(TOP_W)
    d = ImageDraw.Draw(top, "RGBA")
    status_bar(d, TOP_W, "Send to walker - part 1 of 2")
    panel(d, 12, 26, 376, 140)
    # round 8: the user's models rendered in Blender (tools/render_connect.sh): 3DS seen from behind, the walker
    # at its IR window. Falls back to the drawn scene (tools/ui_scene3d.py) when the render is missing.
    rp = os.path.join(ROOT, "art_src", "render", "connect_scene.png")
    if os.path.exists(rp):
        scene_img = Image.open(rp).convert("RGBA")
        # centre what is drawn (not the whole render) in the panel area above the caption (user: "move both
        # models higher up so they fit in their rectangle better")
        bb = scene_img.getchannel("A").point(lambda a: 255 if a > 12 else 0).getbbox()
        area_top, area_bottom = 30 * S, 146 * S
        ox = 16 * S
        oy = int(area_top + (area_bottom - area_top - (bb[3] - bb[1])) / 2 - bb[1]) if bb else 27 * S
        top.paste(scene_img, (ox, oy), scene_img)
        pos = {}
        for line in open(os.path.join(ROOT, "art_src", "render", "connect_scene.txt")):
            k, x, y = line.split()
            pos[k] = (ox + int(x), oy + int(y))
        # IR beam: glowing arcs from the walker's window toward the 3DS's
        (wx, wy), (tx, ty) = pos["IRWALKER"], pos["IR3DS"]
        ang = math.degrees(math.atan2(ty - wy, tx - wx))
        layer = Image.new("RGBA", top.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        for rr in (7, 13, 19):
            r = rr * S
            ld.arc([wx - r, wy - r, wx + r, wy + r], ang - 28, ang + 28, fill=ACCENT + (255,), width=2 * S)
        glow = layer.filter(ImageFilter.GaussianBlur(2.5 * S))
        top.paste(glow, (0, 0), glow)
        top.paste(layer, (0, 0), layer)
        d = ImageDraw.Draw(top, "RGBA")
        # (round 8: the distance moved into the caption; the picture keeps the devices apart for clarity)
    else:
        import ui_scene3d
        mid, _, _ = ui_scene3d.draw_scene(top, 16 * S, 28 * S, 368 * S, 122 * S, lcd_texture(706))
        d = ImageDraw.Draw(top, "RGBA")
        label(d, mid[0] / S + 4, mid[1] / S - 16, "1-2 cm", 9, SUB, "Bold", "mm")
    label(d, 200, 156, "Point the walker's top at the 3DS's IR window, 1-2 cm away", 10, TEXT, "Medium", "mm")
    panel(d, 12, 174, 376, 54)
    label(d, 24, 190, "Reading the walker (safety backup) ...", 11, TEXT, "SemiBold", "lm")
    d.rounded_rectangle(sc(24, 204, 376, 216), radius=6 * S, fill=(226, 236, 218))
    d.rounded_rectangle(sc(24, 204, 24 + 352 * 0.62, 216), radius=6 * S, fill=BAND)
    label(d, 370, 190, "62 %", 11, SUB, "Bold", "rm")
    bot = background(BOT_W)
    d = ImageDraw.Draw(bot, "RGBA")
    panel(d, 14, 30, 292, 110)
    label(d, 160, 50, "On the walker:", 12, TEXT, "Bold", "mm")
    label(d, 160, 76, "press the middle button,", 11, SUB, "Medium", "mm")
    label(d, 160, 94, "choose  Connect", 11, SUB, "Medium", "mm")
    label(d, 160, 116, "Keep it still until this says Done.", 10, SUB, "Medium", "mm")
    footer(d, BOT_W, [("B", "Cancel")])
    return device(top, bot)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "art_src", "ui")
    os.makedirs(out, exist_ok=True)
    for name, fn in (("1_home", home), ("2_game_select", game_select), ("2b_not_compatible", not_compatible),
                     ("3a_game_box", game_box),
                     ("3b_walking_slot", walking_slot), ("4_send", send), ("5_connect", connect)):
        global FRAME
        FRAME = 0
        a = fn()
        FRAME = 1
        b = fn()
        FRAME = 0
        path = os.path.join(out, f"mock_{name}.png")
        if a.tobytes() != b.tobytes():  # round 7: the walker sprite animates -> animated PNG, 0.5 s per frame
            a.save(path, save_all=True, append_images=[b], duration=500, loop=0)
            print("->", path, "(animated)")
        else:
            a.save(path)
            print("->", path)


if __name__ == "__main__":
    main()
