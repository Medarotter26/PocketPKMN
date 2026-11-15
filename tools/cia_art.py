#!/usr/bin/env python3
"""cia_art.py <out dir> - PocketPKMN's own HOME Menu art for the CIA (and the 3dsx icon). All drawing is ours.

Writes:
  banner.png  256x128  the upper-screen banner shown when the app is selected in the HOME Menu
  icon.png     48x48   the app icon (smdh; the 24x24 small icon is made from it by the smdh tool)
  jingle.wav           the short banner sound: our own three-note chime (sine tones, 1.4 s, 16-bit mono 32728 Hz)
  preview.png          banner + icon side by side at 2x, for checking by eye

Colours = tools/ui_mockups.py's palette (greens, white walker with a green screen = the app mark in the header).
Font: Inter (SIL OFL 1.1). Not Console Square Beta: it has no licence file yet (docs/UI_PLAN.md: it stays out until
the user OKs it). Falls back to PIL's default if Inter is missing.
"""
import math
import os
import struct
import sys
import wave

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FONT = "/usr/share/fonts/opentype/inter/Inter-Bold.otf"  # Inter, SIL Open Font Licence 1.1 (WSL system font)
FONT_MED = "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf"

SS = 4  # draw at 4x, box-filter down (as tools/ui_atlas.py)
BG1, BG2 = (196, 228, 170), (232, 246, 220)
LATTICE = (178, 214, 152)
BAND_TOP, BAND_BOT = (70, 128, 60), (34, 74, 28)
TEXT = (40, 60, 36)
SUB = (96, 120, 88)
LCD = (150, 196, 128)
LCD_EDGE = (60, 62, 70)
BODY_EDGE = (150, 156, 168)


def font(path, px):
    try:
        return ImageFont.truetype(path, px)
    except OSError:
        return ImageFont.load_default()


def gradient(w, h, top, bottom):
    im = Image.new("RGB", (w, h), top)
    d = ImageDraw.Draw(im)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return im


def walker_mark(im, cx, cy, r, red_top=True):
    """The app mark (ui_mockups.app_icon) drawn large: round white walker, green screen, three buttons."""
    d = ImageDraw.Draw(im, "RGBA")
    sh = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([cx - r + r * 0.06, cy - r + r * 0.12, cx + r + r * 0.06, cy + r + r * 0.12],
                               fill=(24, 52, 18, 90))
    sh = sh.filter(ImageFilter.GaussianBlur(r * 0.08))
    im.paste(sh, (0, 0), sh)
    d = ImageDraw.Draw(im, "RGBA")
    steps = int(r)
    for i in range(steps, 0, -1):  # body shaded from the top left
        t = i / steps
        c = int(255 - 46 * t ** 2)
        o = -(1 - t) * r * 0.2
        d.ellipse([cx - i + o, cy - i + o, cx + i + o, cy + i + o], fill=(c, c, min(255, c + 4)))
    if red_top:  # user 2026-11-15: "have the pokewalkers red top half" - red cap, same top-left shading
        red = Image.new("RGB", im.size, (0, 0, 0))
        rd = ImageDraw.Draw(red)
        for i in range(steps, 0, -1):
            t = i / steps
            k = 1 - 0.30 * t ** 2
            o = -(1 - t) * r * 0.2
            rd.ellipse([cx - i + o, cy - i + o, cx + i + o, cy + i + o],
                       fill=(int(236 * k + 19 * (1 - k)), int(58 * k), int(48 * k)))
        mask = Image.new("L", im.size, 0)
        md = ImageDraw.Draw(mask)
        md.ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
        md.rectangle([0, cy - r * 0.05, im.size[0], im.size[1]], fill=0)  # red ends under the black band
        im.paste(red, (0, 0), mask)
        d = ImageDraw.Draw(im, "RGBA")
        d.ellipse([cx - r * 0.55, cy - r * 0.86, cx + r * 0.1, cy - r * 0.55], fill=(255, 255, 255, 70))  # shine
    w = max(1, int(r * 0.05))
    s = 2 * r
    x0, y0 = cx - r, cy - r
    if red_top:
        # proportions from the user's photo of a real Pokewalker (2026-11-15): thin black band along the red/white
        # split, the screen (thin black frame, ~3:2) sits across it with its top quarter in the red, three big white
        # buttons low in the white half, the middle one larger
        # user: "Move the dividing color line down some" -> band 0.12 r lower than first drawn
        hw = math.sqrt(max(0.0, r * r - (r * 0.10) ** 2))
        d.rectangle([cx - hw, cy - r * 0.10, cx + hw, cy - r * 0.01], fill=(22, 20, 22))
        # user: "make the screen 1px shorter on the top and bottom" -> SS (= 1 output pixel) in from each
        # user: "Move the entire screen up 2px" -> 2 output pixels (2 * SS) up
        fx0, fy0, fx1, fy1 = cx - r * 0.59, cy - r * 0.42 + SS - 2 * SS, cx + r * 0.59, cy + r * 0.44 - SS - 2 * SS
        d.rounded_rectangle([fx0, fy0, fx1, fy1], radius=r * 0.07, fill=(22, 20, 22))
        fw = r * 0.045
        d.rectangle([fx0 + fw, fy0 + fw, fx1 - fw, fy1 - fw], fill=(196, 200, 168))
        # a soft LCD shine, no Pokemon art
        d.rectangle([fx0 + fw * 2, fy0 + fw * 2, fx1 - fw * 2, fy0 + fw * 2 + r * 0.06], fill=(255, 255, 255, 50))
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=BODY_EDGE, width=w)
        # user: "move the two side smaller buttons up a little" -> sides at 0.63 r, middle stays at 0.70 r
        for bx, by_, br in ((-0.46, 0.63, 0.125), (0.0, 0.70, 0.155), (0.46, 0.63, 0.125)):
            bcx, bcy, rr = cx + r * bx, cy + r * by_, r * br
            d.ellipse([bcx - rr + rr * 0.08, bcy - rr + rr * 0.14, bcx + rr + rr * 0.08, bcy + rr + rr * 0.14],
                      fill=(150, 154, 164, 120))  # soft shadow
            d.ellipse([bcx - rr, bcy - rr, bcx + rr, bcy + rr], fill=(240, 241, 244), outline=(170, 174, 184),
                      width=max(1, int(w * 0.7)))
        return
    sx0, sx1, sy0, sy1, by = 0.25, 0.75, 0.22, 0.58, 0.76
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=BODY_EDGE, width=w)
    d.rounded_rectangle([x0 + s * sx0, y0 + s * sy0, x0 + s * sx1, y0 + s * sy1], radius=r * 0.05,
                        fill=LCD, outline=LCD_EDGE, width=w)
    # a little step-counter shine on the screen (no Pokemon art)
    d.rounded_rectangle([x0 + s * (sx0 + 0.03), y0 + s * (sy0 + 0.03), x0 + s * (sx1 - 0.03), y0 + s * (sy0 + 0.07)],
                        radius=r * 0.03, fill=(255, 255, 255, 60))
    for bx, br in ((0.34, 0.05), (0.5, 0.065), (0.66, 0.05)):
        bcx, bcy = x0 + s * bx, y0 + s * by
        d.ellipse([bcx - s * br, bcy - s * br, bcx + s * br, bcy + s * br], fill=(214, 218, 226),
                  outline=BODY_EDGE, width=w)


def banner():
    """The HOME Menu banner. User 2026-11-15: "fix the banner it just needs the pokewalker and the text. It does not
    need the green background" -> transparent: only the walker mark and the name + subtitle (no gradient, lattice,
    glow, green band or "homebrew")."""
    W, H = 256 * SS, 128 * SS

    def draw(bg):
        im = Image.new("RGB", (W, H), bg)
        walker_mark(im, 62 * SS, 64 * SS, 40 * SS)
        d = ImageDraw.Draw(im, "RGBA")
        px = 30
        while px > 10:  # largest size that fits between the mark and the right edge (8 px margin)
            f = font(FONT, px * SS)
            if d.textlength("PocketPKMN", font=f) <= (256 - 112 - 8) * SS:
                break
            px -= 1
        # white with a dark green outline: readable on light and dark HOME Menu themes (no background)
        d.text((112 * SS, 56 * SS), "PocketPKMN", font=f, fill=(255, 255, 255, 255), anchor="lm", stroke_width=2 * SS,
               stroke_fill=TEXT)
        f2 = font(FONT_MED, 12 * SS)
        d.text((114 * SS, 82 * SS), "Pok\u00e9walker companion", font=f2, fill=(255, 255, 255, 255), anchor="lm",
               stroke_width=int(1.5 * SS), stroke_fill=TEXT)
        return im.resize((256, 128), Image.BOX)

    # User: "Why is the shine on the pokewalker transparent now" - drawing a half-transparent highlight straight onto a
    # transparent canvas replaces the pixels (a see-through hole). Drawn on black and on white instead (opaque, so the
    # shine blends onto the red as before), then the real transparency is recovered from the two.
    a, b = draw((0, 0, 0)), draw((255, 255, 255))
    out = Image.new("RGBA", a.size)
    pa, pb, po = a.load(), b.load(), out.load()
    for y in range(a.height):
        for x in range(a.width):
            ca, cb = pa[x, y], pb[x, y]
            alpha = 255 - max(0, min(255, round(sum(cb[i] - ca[i] for i in range(3)) / 3)))
            po[x, y] = (0, 0, 0, 0) if alpha <= 0 else \
                tuple(max(0, min(255, round(ca[i] * 255 / alpha))) for i in range(3)) + (alpha,)
    return out


def icon():
    N = 48 * SS
    im = Image.new("RGB", (N, N), BG2)
    bg = gradient(N, N, BG2, BG1)
    im.paste(bg, (0, 0))
    d = ImageDraw.Draw(im, "RGBA")
    d.rectangle([0, N - 10 * SS, N, N], fill=BAND_BOT)
    walker_mark(im, N // 2, 22 * SS, 19 * SS)
    return im.resize((48, 48), Image.BOX)


def jingle(path, rate=32728):
    """Three soft rising notes (C6 E6 G6, then a held C7 with the chord), decaying sines - our own."""
    notes = [(0.00, 1046.5, 0.35), (0.16, 1318.5, 0.35), (0.32, 1568.0, 0.35), (0.48, 2093.0, 0.9)]
    total = 1.4
    n = int(total * rate)
    buf = [0.0] * n
    for start, freq, dur in notes:
        s0 = int(start * rate)
        for i in range(int(dur * rate)):
            if s0 + i >= n:
                break
            t = i / rate
            env = min(1.0, t / 0.008) * math.exp(-t * 5.0)
            buf[s0 + i] += 0.22 * env * (math.sin(2 * math.pi * freq * t) + 0.25 * math.sin(4 * math.pi * freq * t))
    fade = int(0.05 * rate)
    for i in range(fade):
        buf[n - 1 - i] *= i / fade
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(v * 32767)))) for v in buf))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "cia")
    os.makedirs(out, exist_ok=True)
    b = banner()
    i = icon()
    b.save(os.path.join(out, "banner.png"))
    i.save(os.path.join(out, "icon.png"))
    jingle(os.path.join(out, "jingle.wav"))
    pv = Image.new("RGB", (256 * 2 + 16 + 48 * 2, 128 * 2), (44, 44, 50))
    bb = b.resize((512, 256), Image.NEAREST)
    pv.paste(bb, (0, 0), bb)
    pv.paste(i.resize((96, 96), Image.NEAREST), (528, 0))
    pv.save(os.path.join(out, "preview.png"))
    print("wrote banner.png icon.png jingle.wav preview.png to", out)


if __name__ == "__main__":
    main()
