"""pwimg.py - shared helpers for the WalkerBox image tools (Python 3, stdlib only).

Walker image format: 8-px-tall stripes, left to right, 2 bytes per column
(more significant bitplane first), bit 0 = top pixel of the stripe.
Shade = (msb << 1) | lsb: 0 white, 1 light grey, 2 dark grey, 3 black.

Contents:
  encode / decode         codec (same as source/pw_image.c)
  load_font / render_text text renderer (same rules as pw_text_image())
  load_icon               ASCII-art icon reader ('.', '-', '+', '#')
  SLOTS                   the menu art slot list for EEPROM 0x0280-0x8C6F
  render_slot             shades (or raw bytes) for one slot
  write_png               minimal PNG writer (zlib + struct)
"""
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art")
ICONS = os.path.join(ART, "icons")

ART_START = 0x0280
ART_END = 0x8C70          # exclusive; 0x8C70-0x8EFF must never be touched

# --------------------------------------------------------------------- codec

def img_size(w, h):
    assert h % 8 == 0, "height must be a multiple of 8"
    return w * (h // 8) * 2


def encode(shades, w, h):
    out = bytearray(img_size(w, h))
    for s in range(h // 8):
        for x in range(w):
            hi = lo = 0
            for b in range(8):
                v = shades[(s * 8 + b) * w + x] & 3
                hi |= ((v >> 1) & 1) << b
                lo |= (v & 1) << b
            o = (s * w + x) * 2
            out[o] = hi
            out[o + 1] = lo
    return bytes(out)


def decode(data, w, h):
    shades = bytearray(w * h)
    for s in range(h // 8):
        for x in range(w):
            o = (s * w + x) * 2
            hi, lo = data[o], data[o + 1]
            for b in range(8):
                shades[(s * 8 + b) * w + x] = (((hi >> b) & 1) << 1) | ((lo >> b) & 1)
    return shades

# ---------------------------------------------------------------------- font

FONT_ROWS = 14       # 2 accent rows + 9 cap rows + 3 descender rows
LINE_H = 16
Y_OFFSET = 1         # glyph row 0 goes to line row 1 (caps on rows 3-11)
E_ACUTE = 95
CE_ACUTE = 96

_font = None


def load_font(path=None):
    """Returns a list of 97 glyphs, each (width, [14 row strings])."""
    global _font
    if _font is not None and path is None:
        return _font
    path = path or os.path.join(ART, "font.txt")
    glyphs = {}
    name = None
    rows = []

    def finish():
        if name is None:
            return
        if len(rows) not in (12, 14):
            raise ValueError("glyph %r: %d rows (need 12 or 14)" % (name, len(rows)))
        width = len(rows[0])
        for r in rows:
            if len(r) != width or set(r) - set(".#"):
                raise ValueError("glyph %r: bad row %r" % (name, r))
        full = rows if len(rows) == 14 else ["." * width] * 2 + rows
        if name in glyphs:
            raise ValueError("glyph %r defined twice" % name)
        glyphs[name] = (width, full)

    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if line.startswith(";"):
                continue
            if line.startswith("[") and line.endswith("]") and len(line) >= 3:
                finish()
                name = line[1:-1]
                rows = []
                continue
            if line.strip() == "":
                finish()
                name = None
                rows = []
                continue
            if name is None:
                raise ValueError("pixel row outside a glyph: %r" % line)
            rows.append(line)
        finish()

    names = {"space": " ", "e_acute": "é", "E_acute": "É"}
    table = {}
    for k, v in glyphs.items():
        table[names.get(k, k)] = v
    out = []
    for c in range(0x20, 0x7F):
        if chr(c) not in table:
            raise ValueError("font is missing %r" % chr(c))
        out.append(table[chr(c)])
    for c in ("é", "É"):
        if c not in table:
            raise ValueError("font is missing %r" % c)
        out.append(table[c])
    extra = set(table) - set(chr(c) for c in range(0x20, 0x7F)) - {"é", "É"}
    if extra:
        raise ValueError("font has unknown glyphs: %r" % sorted(extra))
    if path == os.path.join(ART, "font.txt"):
        _font = out
    return out


def glyph_index(ch):
    o = ord(ch)
    if 0x20 <= o <= 0x7E:
        return o - 0x20
    if ch == "é":
        return E_ACUTE
    if ch == "É":
        return CE_ACUTE
    return ord("?") - 0x20


def text_width(s):
    font = load_font()
    best = 0
    for line in s.split("\n"):
        cur = sum(font[glyph_index(c)][0] + 1 for c in line)
        if cur:
            cur -= 1
        best = max(best, cur)
    return best


def draw_text_line(shades, w, h, s, x, line_top, shade=3):
    """Draws one line of text with its 16-px line box starting at line_top.
    Returns the x after the last glyph's gap."""
    font = load_font()
    for ch in s:
        gw, rows = font[glyph_index(ch)]
        for r in range(FONT_ROWS):
            y = line_top + Y_OFFSET + r
            for c in range(gw):
                if rows[r][c] == "#":
                    xx = x + c
                    if 0 <= xx < w and 0 <= y < h:
                        shades[y * w + xx] = shade
        x += gw + 1
    return x


def render_text(s, w, h):
    """Same rules as pw_text_image(): returns (shades, fit)."""
    font = load_font()
    shades = bytearray(w * h)
    lines = s.split("\n")
    slots = h // LINE_H
    fit = True
    if slots < 1:
        y0 = (h - LINE_H) // 2
        fit = False
    elif len(lines) <= slots:
        y0 = (h - len(lines) * LINE_H) // 2
    else:
        y0 = 0
        fit = False
    for i, line in enumerate(lines):
        if slots >= 1 and i >= slots:
            if line:
                fit = False
            continue
        x = 1
        for ch in line:
            gw = font[glyph_index(ch)][0]
            if x + gw > w:
                fit = False
            x = draw_text_line(shades, w, h, ch, x, y0 + i * LINE_H)
    return shades, fit

# --------------------------------------------------------------------- icons

SHADE_CHARS = {".": 0, "-": 1, "+": 2, "#": 3}


def load_rows(name):
    path = os.path.join(ICONS, name + ".txt")
    rows = []
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\r\n")
            if line.startswith(";") or line.strip() == "":
                continue
            rows.append(line)
    return rows


def load_icon(name, w, h):
    rows = load_rows(name)
    if len(rows) != h:
        raise ValueError("%s: %d rows, slot needs %d" % (name, len(rows), h))
    shades = bytearray(w * h)
    for y, r in enumerate(rows):
        if len(r) != w:
            raise ValueError("%s row %d: width %d, slot needs %d" % (name, y, len(r), w))
        for x, ch in enumerate(r):
            if ch not in SHADE_CHARS:
                raise ValueError("%s row %d: bad char %r" % (name, y, ch))
            shades[y * w + x] = SHADE_CHARS[ch]
    return shades


def load_mask(name):
    """8x8 art -> 8 AND-mask bytes, one per column, bit 0 = top row,
    bit set = keep the pixel ('.'), clear = make it white ('#')."""
    rows = load_rows(name)
    if len(rows) != 8 or any(len(r) != 8 for r in rows):
        raise ValueError("%s: mask must be 8x8" % name)
    out = bytearray(8)
    for x in range(8):
        v = 0
        for y in range(8):
            if rows[y][x] == ".":
                v |= 1 << y
            elif rows[y][x] != "#":
                raise ValueError("%s: mask uses only '.' and '#'" % name)
        out[x] = v
    return bytes(out)

# --------------------------------------------------------- framed text art

def _rect(shades, w, x0, y0, x1, y1, shade):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            shades[y * w + x] = shade


def _raised_box(shades, w, h, fill, bevel):
    """Rounded black box over columns 0..w-2 / rows 0..h-2 with a 1-px dark
    grey drop shadow on the right and bottom (light from the top-left).
    bevel: white inner top/left line, dark grey inner bottom/right line."""
    bw, bh = w - 1, h - 1
    _rect(shades, w, 1, 1, bw - 2, bh - 2, fill)
    if bevel:
        for x in range(1, bw - 1):
            shades[1 * w + x] = 0
            shades[(bh - 2) * w + x] = 2
        for y in range(1, bh - 1):
            shades[y * w + 1] = 0
            shades[y * w + bw - 2] = 2
        shades[(bh - 2) * w + 1] = 1
        shades[1 * w + bw - 2] = 1
    for x in range(1, bw - 1):
        shades[0 * w + x] = 3
        shades[(bh - 1) * w + x] = 3
    for y in range(1, bh - 1):
        shades[y * w + 0] = 3
        shades[y * w + bw - 1] = 3
    for x in range(2, w):                       # drop shadow
        shades[(h - 1) * w + x] = 2
    for y in range(2, h - 1):
        shades[y * w + w - 1] = 2


def render_heading(s, w, h):
    """Menu heading: raised bevelled box, WHITE face (maximum text contrast on
    the LCD), black text centred; depth only from the frame (dark inner
    bottom/right bevel line + 1-px dark drop shadow)."""
    shades = bytearray(w * h)
    _raised_box(shades, w, h, 0, True)
    tw = text_width(s)
    fit = tw <= w - 6
    draw_text_line(shades, w, h, s, (w - 1 - tw) // 2, (h - LINE_H) // 2 - 1)
    return shades, fit


def render_frame(s, w, h):
    """Label frame: raised white box with a drop shadow, black text centred."""
    shades = bytearray(w * h)
    _raised_box(shades, w, h, 0, False)
    tw = text_width(s)
    fit = tw <= w - 5
    draw_text_line(shades, w, h, s, (w - 1 - tw) // 2, (h - LINE_H) // 2 - 1)
    return shades, fit


def render_digit(ch, w, h):
    shades = bytearray(w * h)
    tw = text_width(ch)
    draw_text_line(shades, w, h, ch, (w - tw) // 2, (h - LINE_H) // 2)
    return shades, tw <= w


def _triangle(shades, w, cx, cy, direction, size=4):
    """Small solid triangle with its tip at (cx, cy), pointing 'left',
    'right' or 'down'."""
    for i in range(size):
        for j in range(-i, i + 1):
            if direction == "left":
                x, y = cx + i, cy + j
            elif direction == "right":
                x, y = cx - i, cy + j
            else:
                x, y = cx + j, cy - i
            shades[y * w + x] = 3


def render_placard(w, h):
    """Battle directions 96x32: left button = Attack, right = Evade,
    middle = Catch (as in the walker's battle input)."""
    shades = bytearray(w * h)
    # line 1: "<Attack" left, "Evade>" right
    _triangle(shades, w, 1, 7, "left")
    draw_text_line(shades, w, h, "Attack", 6, 0)
    tw = text_width("Evade")
    draw_text_line(shades, w, h, "Evade", w - 6 - tw, 0)
    _triangle(shades, w, w - 2, 7, "right")
    # line 2: "Catch" centred, with a down triangle on each side
    tw = text_width("Catch")
    x = (w - tw) // 2
    draw_text_line(shades, w, h, "Catch", x, 16)
    _triangle(shades, w, x - 7, 16 + 10, "down")
    _triangle(shades, w, x + tw + 6, 16 + 10, "down")
    # thin dotted dark grey rule between the lines
    for xx in range(4, w - 4, 2):
        shades[15 * w + xx] = 2
    return shades, True

# ---------------------------------------------------------------- slot list
# Addresses and sizes from Dmitry Grinberg's EEPROM map (dmitry.gr, "pokewalker"),
# with the end-address typos there fixed from the image sizes.
# kind: icon (art/icons/<arg>.txt), text, heading, frame, digit, blank,
#       mask (raw 8-byte AND mask, listed as 4x8 so that w*(h/8)*2 == 8),
#       placard.

def _S(addr, w, h, name, kind, arg=None):
    return {"addr": addr, "w": w, "h": h, "name": name, "kind": kind, "arg": arg}


def _build_slots():
    s = []
    for i, ch in enumerate("0123456789:-/"):
        nm = {":": "colon", "-": "dash", "/": "slash"}.get(ch, ch)
        s.append(_S(0x0280 + i * 32, 8, 16, "digit_" + nm, "digit", ch))
    s += [
        _S(0x0420, 16, 16, "watt", "icon", "watt"),
        _S(0x0460, 8, 8, "ball", "icon", "ball"),
        _S(0x0470, 8, 8, "ball_light", "icon", "ball_light"),
        _S(0x0488, 8, 8, "item", "icon", "item"),
        _S(0x0498, 8, 8, "item_light", "icon", "item_light"),
        _S(0x04A8, 8, 8, "map_small", "icon", "map_small"),
        _S(0x04B8, 8, 8, "suit_heart", "icon", "suit_heart"),
        _S(0x04C8, 8, 8, "suit_spade", "icon", "suit_spade"),
        _S(0x04D8, 8, 8, "suit_diamond", "icon", "suit_diamond"),
        _S(0x04E8, 8, 8, "suit_club", "icon", "suit_club"),
    ]
    a = 0x04F8
    for d in ("up", "down", "left", "right"):
        for v in ("", "_offset", "_invert"):
            n = "arrow_" + d + v
            s.append(_S(a, 8, 8, n, "icon", n))
            a += 16
    s += [
        _S(0x05B8, 8, 16, "menu_arrow_left", "icon", "menu_arrow_left"),
        _S(0x05D8, 8, 16, "menu_arrow_right", "icon", "menu_arrow_right"),
        _S(0x05F8, 8, 16, "menu_return", "icon", "menu_return"),
        _S(0x0638, 8, 8, "more_message", "icon", "more_message"),
        _S(0x0648, 4, 8, "more_messages_mask", "mask", "more_messages_mask"),
        _S(0x0650, 8, 8, "vial", "icon", "vial"),
        _S(0x0660, 8, 8, "low_battery", "icon", "low_battery"),
    ]
    a = 0x0670
    for n in ("exclaim", "heart", "music", "smile", "neutral", "ellipsis"):
        s.append(_S(a, 24, 16, "talk_" + n, "icon", "talk_" + n))
        a += 96
    s.append(_S(0x08B0, 24, 16, "talk_exclaim_left", "icon", "talk_exclaim_left"))
    a = 0x0910
    for n, t in (("radar", "Poké Radar"), ("dowsing", "Dowsing"), ("connect", "Connect"),
                 ("trainer_card", "Trainer Card"), ("inventory", "Pokémon & Items"),
                 ("settings", "Settings")):
        s.append(_S(a, 80, 16, "heading_" + n, "heading", t))
        a += 320
    a = 0x1090
    for n in ("radar", "dowsing", "connect", "trainer_card", "inventory", "settings"):
        s.append(_S(a, 16, 16, "icon_" + n, "icon", "icon_" + n))
        a += 64
    s += [
        _S(0x1210, 16, 16, "person", "icon", "person"),
        _S(0x1250, 80, 16, "trainer_name", "blank"),      # filled at run time
        _S(0x1390, 16, 16, "route_small", "icon", "route_small"),
        _S(0x13D0, 40, 16, "frame_steps", "frame", "Steps"),
        _S(0x1470, 32, 16, "frame_time", "frame", "Time"),
        _S(0x14F0, 40, 16, "frame_days", "frame", "Days"),
        _S(0x1590, 64, 16, "frame_total_days", "frame", "Total days:"),
        _S(0x1690, 40, 16, "frame_sound", "frame", "Sound"),
        _S(0x1730, 40, 16, "frame_shade", "frame", "Shade"),
        _S(0x17D0, 24, 16, "speaker_off", "icon", "speaker_off"),
        _S(0x1830, 24, 16, "speaker_low", "icon", "speaker_low"),
        _S(0x1890, 24, 16, "speaker_high", "icon", "speaker_high"),
        _S(0x18F0, 8, 16, "contrast", "icon", "contrast"),
        _S(0x1910, 32, 24, "chest", "icon", "chest"),
        _S(0x19D0, 32, 24, "map_scroll", "icon", "map_scroll"),
        _S(0x1A90, 32, 24, "present", "icon", "present"),
        _S(0x1B50, 16, 16, "bush_dark", "icon", "bush_dark"),
        _S(0x1B90, 16, 16, "bush_light", "icon", "bush_light"),
        _S(0x1BD0, 32, 16, "text_left", "text", "Left:"),
        _S(0x1C50, 16, 24, "blank_16x24", "blank"),
        _S(0x1CB0, 32, 24, "radar_bush", "icon", "radar_bush"),
        _S(0x1D70, 16, 16, "radar_bubble1", "icon", "radar_bubble1"),
        _S(0x1DB0, 16, 16, "radar_bubble2", "icon", "radar_bubble2"),
        _S(0x1DF0, 16, 16, "radar_bubble3", "icon", "radar_bubble3"),
        _S(0x1E30, 16, 16, "radar_click", "icon", "radar_click"),
        _S(0x1E70, 16, 32, "star_attack", "icon", "star_attack"),
        _S(0x1EF0, 16, 32, "star_critical", "icon", "star_critical"),
        _S(0x1F70, 32, 24, "cloud", "icon", "cloud"),
        _S(0x2030, 8, 8, "hp_blip", "icon", "hp_blip"),
        _S(0x2040, 8, 8, "catch_star", "icon", "catch_star"),
        _S(0x2050, 96, 32, "battle_placard", "placard"),
        _S(0x2350, 32, 32, "walker", "icon", "walker"),
        _S(0x2450, 8, 16, "ir_arcs", "icon", "ir_arcs"),
        _S(0x2470, 8, 8, "music_note", "icon", "music_note"),
        _S(0x2480, 8, 8, "blank_8x8", "blank"),
        _S(0x2490, 40, 16, "frame_hours", "frame", "Hours"),
    ]
    texts = [
        (0x2530, 16, "connecting", "Connecting..."),
        (0x26B0, 16, "no_trainer", "No trainer found"),
        (0x2830, 32, "cannot_complete", "Cannot complete\nthis connection."),
        (0x2B30, 16, "cannot_connect", "Cannot connect"),
        (0x2CB0, 32, "trainer_unavailable", "Other trainer\nis unavailable."),
        (0x2FB0, 32, "already_received", "Already received\nthis event."),
        (0x32B0, 32, "cannot_connect_again", "Cannot connect\nto trainer again."),
        (0x35B0, 32, "could_not_receive", "Could not\nreceive..."),
        (0x38B0, 16, "has_arrived", "has arrived!"),
        (0x3A30, 16, "has_left", "has left."),
        (0x3BB0, 16, "received", "received!"),
        (0x3D30, 16, "completed", "Completed!"),
        (0x3EB0, 16, "special_map", "Special Map"),
        (0x4030, 16, "stamp", "Stamp"),
        (0x41B0, 16, "special_route", "Special Route"),
        (0x4330, 16, "need_watts", "Need more Watts."),
        (0x44B0, 16, "no_pokemon_held", "No Pokémon held!"),
        (0x4630, 16, "nothing_held", "Nothing held!"),
        (0x47B0, 16, "discover_item", "Discover an item!"),
        (0x4930, 16, "found", "found!"),
        (0x4AB0, 16, "nothing_found", "Nothing found!"),
        (0x4C30, 16, "its_near", "It's near!"),
        (0x4DB0, 16, "far_away", "It's far away..."),
        (0x4F30, 16, "find_pokemon", "Find a Pokémon!"),
        (0x50B0, 16, "found_something", "Found something!"),
        (0x5230, 16, "got_away", "It got away..."),
        (0x53B0, 16, "appeared", "appeared!"),
        (0x5530, 16, "was_caught", "was caught!"),
        (0x56B0, 16, "fled", "fled..."),
        (0x5830, 16, "too_strong", "was too strong."),
        (0x59B0, 16, "attacked", "attacked!"),
        (0x5B30, 16, "evaded", "evaded!"),
        (0x5CB0, 16, "critical_hit", "A critical hit!"),
        (0x5E30, 16, "spaces", None),
        (0x5FB0, 16, "threw_ball", "Threw a Poké Ball."),
        (0x6130, 16, "almost_had", "Almost had it!"),
        (0x62B0, 16, "stare_down", "Stare down!"),
        (0x6430, 16, "lost", "lost!"),
        (0x65B0, 16, "peer_has_arrived", "has arrived!"),
        (0x6730, 16, "had_adventures", "Had adventures!"),
        (0x68B0, 16, "play_battled", "Play-battled."),
        (0x6A30, 16, "went_run", "Went for a run."),
        (0x6BB0, 16, "went_walk", "Went for a walk."),
        (0x6D30, 16, "played", "Played a bit."),
        (0x6EB0, 16, "gift", "Here's a gift..."),
        (0x7030, 16, "cheered", "cheered!"),
        (0x71B0, 16, "very_happy", "is very happy!"),
        (0x7330, 16, "having_fun", "is having fun!"),
        (0x74B0, 16, "feeling_good", "is feeling good!"),
        (0x7630, 16, "happy", "is happy."),
        (0x77B0, 16, "smiling", "is smiling."),
        (0x7930, 16, "cheerful", "is cheerful."),
        (0x7AB0, 16, "patient", "is being patient."),
        (0x7C30, 16, "sits_quietly", "sits quietly."),
        (0x7DB0, 16, "turned_to_look", "turned to look."),
        (0x7F30, 16, "looking_around", "is looking around."),
        (0x80B0, 16, "looking_this_way", "is looking this way."),
        (0x8230, 16, "daydreaming", "is daydreaming."),
        (0x83B0, 16, "found_something2", "found something."),
        (0x8530, 16, "what", "What?"),
        (0x86B0, 16, "joined", "joined you!"),
        (0x8830, 16, "reward", "Reward"),
        (0x89B0, 16, "good_job", "Good job!"),
    ]
    for addr, h, n, t in texts:
        if t is None:
            s.append(_S(addr, 96, h, "text_" + n, "blank"))
        else:
            s.append(_S(addr, 96, h, "text_" + n, "text", t))
    s.append(_S(0x8B30, 80, 16, "text_switch", "text", "Switch?"))
    return s


SLOTS = _build_slots()


def slot_size(sl):
    return img_size(sl["w"], sl["h"])


def check_slots():
    """Slots must be sorted, inside 0x0280-0x8C6F and must not overlap."""
    prev_end = ART_START
    for sl in SLOTS:
        a = sl["addr"]
        e = a + slot_size(sl)
        if a < prev_end:
            raise ValueError("slot %s at 0x%04X overlaps the one before" % (sl["name"], a))
        if e > ART_END:
            raise ValueError("slot %s ends past 0x8C6F" % sl["name"])
        prev_end = e
    names = [sl["name"] for sl in SLOTS]
    if len(set(names)) != len(names):
        raise ValueError("duplicate slot names")


def render_slot(sl):
    """Returns (shades or None, encoded bytes or None for white, fit flag)."""
    w, h, k, arg = sl["w"], sl["h"], sl["kind"], sl["arg"]
    if k == "blank":
        return None, None, True
    if k == "mask":
        return None, load_mask(arg), True
    if k == "icon":
        sh, fit = load_icon(arg, w, h), True
    elif k == "text":
        sh, fit = render_text(arg, w, h)
    elif k == "heading":
        sh, fit = render_heading(arg, w, h)
    elif k == "frame":
        sh, fit = render_frame(arg, w, h)
    elif k == "digit":
        sh, fit = render_digit(arg, w, h)
    elif k == "placard":
        sh, fit = render_placard(w, h)
    else:
        raise ValueError("unknown slot kind " + k)
    return sh, encode(sh, w, h), fit

# ----------------------------------------------------------------------- png

GREY = {0: 255, 1: 170, 2: 85, 3: 0}


def write_png(path, width, height, rgb_rows):
    """rgb_rows: list of bytes objects, each width*3 long."""
    raw = b"".join(b"\x00" + bytes(r) for r in rgb_rows)
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def shades_to_png(path, shades, w, h, scale=1):
    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            g = GREY[shades[y * w + x] & 3]
            row += bytes((g, g, g)) * scale
        for _ in range(scale):
            rows.append(bytes(row))
    write_png(path, w * scale, h * scale, rows)
