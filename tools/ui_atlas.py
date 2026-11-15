#!/usr/bin/env python3
"""ui_atlas.py - the app's UI pieces (v0.32+), drawn with the SAME functions as tools/ui_mockups.py.

v0.35 (user: "less sharp pixel edges ... anti aliasing"): every piece is drawn at SS x and scaled down
(box filter), so rounded corners, rings, badges and shadows get smooth edges; the backgrounds are drawn
exactly like the 2x mockups and halved. Same sizes and positions as before.

Writes gfx/ui_*.png + gfx/ui_atlas.t3s (pieces, RGBA) + gfx/ui_bgtop.t3s / gfx/ui_bgbot.t3s (backgrounds).
The build turns them into textures inside the 3dsx (no romfs, no SD access at run time).

No text is drawn into any piece: all text is drawn by the app with its own font (system font until the user
OKs Console Square Beta), so none of the user's font ends up in the pictures.

Pieces with transparency are rendered twice (on black and on white) and the alpha is recovered from the two,
so shadows and translucent fills come out exactly as the mockups composite them.
Run in WSL:  python3 tools/ui_atlas.py
"""
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ui_mockups as um  # noqa: E402

SS = 4  # supersampling for the pieces (v0.35); the app shows them at 1x
um.S = SS
um.shadow_text = lambda *a, **k: None  # no text in the pieces
um.label = lambda *a, **k: None

OUT = os.path.join(ROOT, "gfx")
P = 6  # room for the drop shadow around a piece

# name -> (canvas w, h, draw(d)) ; the C side knows each piece's slice sizes (source/ui_core.c, PIECES)
PIECES = []


def piece(name, w, h):
    def deco(fn):
        PIECES.append((name, w, h, fn))
        return fn
    return deco


def render(w, h, fn):
    """Draw fn at SS x on black and on white, scale both down (box filter = averaging, which commutes with the
    compositing), then recover straight RGBA."""
    um.S = SS
    a = Image.new("RGB", (w * SS, h * SS), (0, 0, 0))
    b = Image.new("RGB", (w * SS, h * SS), (255, 255, 255))
    fn(ImageDraw.Draw(a, "RGBA"))
    fn(ImageDraw.Draw(b, "RGBA"))
    a = a.resize((w, h), Image.Resampling.BOX)
    b = b.resize((w, h), Image.Resampling.BOX)
    out = Image.new("RGBA", (w, h))
    pa, pb, po = a.load(), b.load(), out.load()
    for y in range(h):
        for x in range(w):
            ca, cb = pa[x, y], pb[x, y]
            alpha = 255 - max(0, min(255, round(sum(cb[i] - ca[i] for i in range(3)) / 3)))
            if alpha <= 0:
                po[x, y] = (0, 0, 0, 0)
            else:
                po[x, y] = tuple(max(0, min(255, round(ca[i] * 255 / alpha))) for i in range(3)) + (alpha,)
    return out


# --- 9-slice pieces: a 40x40 shape at (P, P) --------------------------------------------------------
N = 40


# v0.36 (user: "Boxes having a slight gradient inside them would look nice too"): the quiet panel, the info box
# and the icon box get a visible top-to-bottom gradient (the mockups' panel was nearly flat).
PANEL_TOP, PANEL_BOTTOM = (255, 255, 252, 244), (212, 228, 198, 238)  # v0.39 user: "a little stronger"


def nine(name, **kw):
    @piece(name, N + 2 * P, N + 2 * P)
    def _(d):
        if name == "ui_panel":
            um.soft_shadow(d, P, P, P + N, P + N, 6)
            um.grad_rrect(d, P, P, P + N, P + N, PANEL_TOP, PANEL_BOTTOM, 6)
            d.rounded_rectangle(um.sc(P, P, P + N, P + N), radius=6 * SS, outline=um.PANEL_EDGE_SOFT, width=SS)
        else:
            um.panel(d, P, P, N, N, **kw)


# Selected button (v0.34, user: the red-edged "bubble selector with the red squares on the side looks ugly"):
# no red, no side marks - the same button with a 2-px green ring and a faint green tint.
SEL_RING = (72, 140, 58)


def selected(name, r):
    @piece(name, N + 2 * P, N + 2 * P)
    def _(d):
        um.panel(d, P, P, N, N, r=r, glossy=True, edge=SEL_RING)
        d.rounded_rectangle(um.sc(P, P, P + N, P + N), radius=r * SS, fill=(120, 190, 100, 28))
        d.rounded_rectangle(um.sc(P + 1, P + 1, P + N - 1, P + N - 1), radius=max(1, r - 1) * SS, outline=SEL_RING,
                            width=SS)


# same order as before (the C side indexes the atlas by this order)
nine("ui_panel")                                   # quiet panel, r 6
nine("ui_gloss6", glossy=True)                     # big glossy button / tile, r 6
selected("ui_gloss6_sel", 6)
nine("ui_gloss3", glossy=True, r=3)                # list button, r 3
selected("ui_gloss3_sel", 3)


@piece("ui_info", N + 2 * P, N + 2 * P)
def _(d):
    um.grad_rrect(d, P, P, P + N, P + N, (255, 255, 255, 240), (218, 234, 208, 238), 6)
    d.rounded_rectangle(um.sc(P, P, P + N, P + N), radius=6 * SS, outline=(150, 196, 128), width=SS)


@piece("ui_iconbox", N + 2 * P, N + 2 * P)
def _(d):
    um.grad_rrect(d, P, P, P + N, P + N, (255, 250, 242), (238, 218, 196), 6)
    d.rounded_rectangle(um.sc(P, P, P + N, P + N), radius=6 * SS, outline=um.PANEL_EDGE, width=SS)


@piece("ui_gameicon", N + 2 * P, N + 2 * P)  # frame behind / instead of a HOME icon
def _(d):
    um.grad_rrect(d, P, P, P + N, P + N, (250, 250, 250), (214, 222, 230), 6)
    d.rounded_rectangle(um.sc(P, P, P + N, P + N), radius=6 * SS, outline=(150, 160, 170), width=SS)


@piece("ui_lcdframe", N + 2 * P, N + 2 * P)  # the walker screen's dark frame; the LCD sits 6 px inside
def _(d):
    x0, y0, x1, y1 = P, P, P + N, P + N
    um.soft_shadow(d, x0, y0, x1, y1, 8, blur=3, alpha=110)
    um.grad_rrect(d, x0, y0, x1, y1, (98, 98, 108), (48, 48, 54), 8)
    d.line(um.sc(x0 + 4, y0 + 1.5, x1 - 4, y0 + 1.5), fill=(255, 255, 255, 70), width=SS)


# --- 3-slice bands (stretched sideways only) ------------------------------------------------------------
for h in (20, 22):
    for nm, col in (("green", um.GREEN_BAND), ("red", um.RED_BAND)):
        def _band(d, h=h, col=col):
            um.band(d, P, P, N, h, "", 12, col)
        PIECES.append((f"ui_band{h}_{nm}", N + 2 * P, h + 2 * P, _band))


# --- fixed pieces -------------------------------------------------------------------------------------
for r in (7, 8, 10):
    def _key(d, r=r):
        um.key_badge(d, P + r, P + r, "", r)
    PIECES.append((f"ui_key{r}", 2 * r + 2 * P, 2 * r + 2 * P, _key))


@piece("ui_stripe", 5, 58)  # main button's green stripe
def _(d):
    um.grad_rrect(d, 0, 0, 5, 58, (110, 176, 92), (40, 88, 34), 1)


@piece("ui_status", 400, 16 + P)  # header bar, no text (name / page title drawn by the app)
def _(d):
    w = 400
    um.soft_shadow(d, 0, 0, w, 16, 0, off=(0, 1.5), blur=2, alpha=90)
    um.grad_rrect(d, 0, 0, w, 16, (70, 128, 60), (34, 74, 28), 0)
    d.line(um.sc(0, 0.5, w, 0.5), fill=(255, 255, 255, 60), width=SS)
    d.line(um.sc(0, 15.5, w, 15.5), fill=(20, 44, 16, 200), width=SS)
    # v0.49 (user: "The icon on the top left of the top green bar should be updated"): the app icon's walker
    # (tools/cia_art.py walker_mark, red top) instead of the old round mark
    mark = status_mark(12)
    d._image.paste(mark.convert("RGB"), (2 * SS, 2 * SS), mark.getchannel("A"))


def status_mark(px):
    """The HOME Menu icon's walker (cia_art.walker_mark) on a clear background, px x px at SS x."""
    import cia_art as ca
    n = 48 * ca.SS
    # drawn on black and on white, alpha recovered (a translucent shine drawn on a clear canvas came out see-through:
    # user "The shine is green")
    k = Image.new("RGBA", (n, n), (0, 0, 0, 255))
    w = Image.new("RGBA", (n, n), (255, 255, 255, 255))
    for im in (k, w):
        ca.walker_mark(im, n // 2, n // 2 - ca.SS, 21 * ca.SS)
    size = (px * SS, px * SS)
    k = k.convert("RGB").resize(size, Image.Resampling.BOX)
    w = w.convert("RGB").resize(size, Image.Resampling.BOX)
    out = Image.new("RGBA", size)
    pk, pw, po = k.load(), w.load(), out.load()
    for y in range(size[1]):
        for x in range(size[0]):
            c0, c1 = pk[x, y], pw[x, y]
            al = 255 - max(0, min(255, round(sum(c1[i] - c0[i] for i in range(3)) / 3)))
            po[x, y] = (0, 0, 0, 0) if al == 0 else tuple(min(255, round(c0[i] * 255 / al)) for i in range(3)) + (al,)
    return out


@piece("ui_footer", 320, 24 + P)  # bottom-screen footer, no hints (drawn by the app)
def _(d):
    w, y = 320, P
    um.soft_shadow(d, 0, y, w, y + 24, 0, off=(0, -1.5), blur=2, alpha=70)
    um.grad_rrect(d, 0, y, w, y + 24, (248, 250, 246), (214, 224, 208), 0)
    d.line(um.sc(0, y + 0.5, w, y + 0.5), fill=(140, 170, 124, 255), width=SS)


# user: "the 3ds buttons and the pokewalker buttons the the rest of the 3ds top half should have outlines too" ->
# the render's own line art (tools/render_3ds.py looks "lines=2"); "Just model outlines. Not outlines around them" ->
# no 2D outline around the models (OUTLINE = 0), the soft shadow stays.
RENDER = "connect_scene_rot-40_lift-60_lines1_arcs1"  # thin lines (user: "way too thick" -> lines1); IR arcs flat in the render (user: "The signal picture should be flat, facing the ceiling")
ARCS_2D = False  # the old arcs drawn on top of the picture (before the 3D arcs)  # art_src/render/<RENDER>.png + .txt (IR window positions)
OUTLINE = 0  # 2D outline around the models, in 1x pixels (0 = none)


def scene_with_arcs(n):
    """v0.50 (user: "animate the signal image going from the pokewalker to the 3ds ... showing 0, 1 2, 3, 0, 1, 2, 3"):
    the render without arcs plus the n arcs nearest the walker, taken from the render with arcs (same camera; each arc
    = one connected patch of the difference, ordered by its distance from the walker's IR window)."""
    import numpy as np
    from scipy import ndimage
    from PIL import ImageFilter
    full = Image.open(os.path.join(ROOT, "art_src", "render", RENDER + ".png")).convert("RGBA")
    if n >= 4:
        return full
    plain = Image.open(os.path.join(ROOT, "art_src", "render", RENDER.replace("_arcs1", "") + ".png")).convert("RGBA")
    a, b = np.asarray(plain).astype(int), np.asarray(full).astype(int)
    lab, cnt = ndimage.label(np.abs(a - b).sum(axis=2) > 30)
    wx, wy = [int(v) for v in [ln.split()[1:] for ln in open(os.path.join(
        ROOT, "art_src", "render", RENDER + ".txt")) if ln.startswith("IRWALKER")][0]]
    arcs = []
    for i in range(1, cnt + 1):
        ys, xs = np.nonzero(lab == i)
        if len(xs) > 20:
            arcs.append((np.hypot(xs.mean() - wx, ys.mean() - wy), i))
    arcs.sort()
    out = plain.copy()
    for _, i in arcs[:n]:
        m = Image.fromarray(((lab == i) * 255).astype("uint8")).filter(ImageFilter.MaxFilter(5))
        out.paste(full, (0, 0), m)  # the arc's own pixels (with their edge) from the render with arcs
    return out


def connect_scene(arcs=4):
    """v0.46 (user: "Add in the 3ds and pokewalker model infographic"): the Blender render
    (art_src/render/connect_scene.png, 2x; models: Pokewalker by Callum Lax CC BY 4.0, 3DS from 3D Warehouse) with
    the red IR arcs, laid out exactly as tools/ui_mockups.py connect() does at 2x, then halved (premultiplied box
    filter). Canvas = the picture area of the connect screen's top panel: 368 x 116 at (16, 30)."""
    import math as _m
    from PIL import ImageFilter
    S2 = 2
    W2, H2 = 368 * S2, 116 * S2
    # user: "Use the bottom but now move it down a little bit" -> walker_rot -40, walker_lift -60
    rp = os.path.join(ROOT, "art_src", "render", RENDER + ".png")
    scene = Image.open(rp).convert("RGBA")
    bb = scene.getchannel("A").point(lambda a: 255 if a > 12 else 0).getbbox()
    area_top, area_bottom = 30 * S2, 146 * S2
    ox, oy = 16 * S2, int(area_top + (area_bottom - area_top - (bb[3] - bb[1])) / 2 - bb[1])
    canvas = Image.new("RGBA", (W2, H2), (0, 0, 0, 0))
    at = (ox - 16 * S2, oy - 30 * S2)  # panel area origin (16, 30) at 2x
    if arcs < 4:  # v0.50: placement above stays the full render's, so every frame lines up
        scene = scene_with_arcs(arcs)
    # user: "Then add outlines and shadows to the models": a soft drop shadow (down-right) and a thin dark outline
    # around each model, both made from the render's alpha.
    a = scene.getchannel("A").point(lambda v: 255 if v > 24 else 0)
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    sh_mask = Image.new("L", canvas.size, 0)
    sh_mask.paste(a, (at[0] + 3 * S2, at[1] + 5 * S2))
    sh_mask = sh_mask.filter(ImageFilter.GaussianBlur(4 * S2)).point(lambda v: v * 95 // 255)
    # v0.49 (user: "The shadow under the 3ds model ends abruptly"): the shadow ran off the canvas's bottom edge
    # (cut straight there); it now fades out over the last 14 px so it never ends in a line
    fade = Image.new("L", canvas.size, 255)
    fd = ImageDraw.Draw(fade)
    fz = 14 * S2
    for yy in range(fz):
        fd.line([(0, H2 - 1 - yy), (W2, H2 - 1 - yy)], fill=int(255 * (yy / fz) ** 1.5))
    from PIL import ImageChops
    sh_mask = ImageChops.multiply(sh_mask, fade)
    shadow.paste((24, 52, 18, 255), (0, 0), sh_mask)
    canvas.alpha_composite(shadow)
    if OUTLINE > 0:
        ol_mask = Image.new("L", canvas.size, 0)
        ol_mask.paste(a, at)
        ol_mask = ol_mask.filter(ImageFilter.MaxFilter(2 * OUTLINE * S2 + 1))
        outline = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
        outline.paste((34, 44, 40, 255), (0, 0), ol_mask)
        canvas.alpha_composite(outline)
    canvas.alpha_composite(scene, at)
    pos = {}
    for line in open(os.path.join(ROOT, "art_src", "render", RENDER + ".txt")):
        k, x, y = line.split()
        pos[k] = (ox - 16 * S2 + int(x), oy - 30 * S2 + int(y))
    (wx, wy), (tx, ty) = pos["IRWALKER"], pos["IR3DS"]
    ang = _m.degrees(_m.atan2(ty - wy, tx - wx))
    layer = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    # user: "the signal picture coming from the pokewalker ... you need to remake it for the new angles and positions":
    # four arcs from the walker's IR window toward the 3DS's, spaced over the first ~55 % of the gap (so they never
    # touch the 3DS), each a little wider and fainter than the last.
    dist = _m.hypot(tx - wx, ty - wy)
    for k, f in enumerate((0.16, 0.29, 0.42, 0.55) if ARCS_2D else ()):
        r = dist * f
        span = 20 + k * 3
        alpha = 255 - k * 40
        ld.arc([wx - r, wy - r, wx + r, wy + r], ang - span, ang + span, fill=um.ACCENT + (alpha,),
               width=int(1.6 * S2))
    glow = layer.filter(ImageFilter.GaussianBlur(2.5 * S2))
    canvas.alpha_composite(glow)
    canvas.alpha_composite(layer)
    return canvas.convert("RGBa").resize((368, 116), Image.Resampling.BOX).convert("RGBA")


def main():
    os.makedirs(OUT, exist_ok=True)
    names = []
    for name, w, h, fn in PIECES:
        render(w, h, fn).save(os.path.join(OUT, name + ".png"))
        names.append(name + ".png")
    connect_scene().save(os.path.join(OUT, "ui_connect_scene.png"))  # index 19 (after the 19 pieces)
    names.append("ui_connect_scene.png")
    for k in range(4):  # v0.50: indices 20-23 = the same picture with 0, 1, 2, 3 arcs (the animation)
        connect_scene(k).save(os.path.join(OUT, f"ui_connect_scene_a{k}.png"))
        names.append(f"ui_connect_scene_a{k}.png")
    with open(os.path.join(OUT, "ui_atlas.t3s"), "w", newline="\n") as f:
        f.write("--atlas -f rgba8888 -z auto\n" + "\n".join(names) + "\n")
    # Backgrounds (v0.47, user: "Add a toggle for all 3 in settings"): one sheet per screen with four pictures, in
    # this order = the Settings choice (0 Classic = the 2x mockup background halved, 1 Poke Ball pattern,
    # 2 Walker screen, 3 Route scene - tools/bg_options.py).
    import bg_options as bgo
    um.S = 2
    for nm, w in (("bgtop", 400), ("bgbot", 320)):
        # v0.48: 1-3 are seamless scrolling tiles (A, B: 400 x 256 vertical repeat; C: 512 x 240 horizontal repeat)
        pics = [um.background(w).resize((w, um.H), Image.Resampling.BOX), bgo.tile_a(448), bgo.tile_b(400),
                bgo.tile_c()]
        files = []
        for k, im in enumerate(pics):
            fn = f"ui_{nm}_img.png" if k == 0 else f"ui_{nm}_img{k}.png"
            im.save(os.path.join(OUT, fn))
            files.append(fn)
        with open(os.path.join(OUT, f"ui_{nm}.t3s"), "w", newline="\n") as f:
            f.write("--atlas -f rgb565 -z auto\n" + "\n".join(files) + "\n")
    print(f"{len(names)} pieces + 2 x 4 backgrounds -> {OUT}")


if __name__ == "__main__":
    main()
