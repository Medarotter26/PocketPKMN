#!/usr/bin/env python3
"""Walker sprites for the Gen 6/7 Pokemon's other forms and female sprites (user, 2026-11-17: "Create pokewalker
sprites for the gens 6/7 pokemon with different forms or gender differences" + the Smogon Gen 6 and Gen 7 Sprite
Project sheets). Same sources and rules as the base Gen 6/7 sprites and the Alolan forms:
  large: the sheet's "Front" picture (one picture -> both large frames), mirrored, full size, face-centred crop;
  small: the Sun/Moon menu icon from Bulbagarden ("666PolMS6.png" ...), icon_frames.

Only forms a Pokemon keeps in a box are made (battle-only forms are left out: Aegislash Blade, Wishiwashi School,
Mimikyu Busted, Zygarde Complete, Ultra Necrozma, Ash-Greninja; Minior's Meteor form is battle-only, its boxed form
is the core). File names use the GAME's form number (PKHeX / PKSM order), not the sheet's:
  NNN_fK.bin = form K; NNN_f.bin = female (gender difference only, form 0).
  _f0 files exist where the pack's default picture is NOT form 0: Vivillon (pack = Meadow = form 6, form 0 = Icy
  Snow) and Xerneas (pack = Active = form 1, form 0 = Neutral, the one shown outside battle).

  walker_forms67.py fetch <dir>          -> <dir>/large/<key>.png, <dir>/small/<key>.png, <dir>/credits.csv
  walker_forms67.py convert <dir> <out>  -> <out>/<key>.bin (0x780) + preview_N.png
Face points: tools/walker_forms67_heads.txt ("<key> x y"), else the base Pokemon's walker_heads.txt point.
All the base Pokemon's settings apply (walker_review.txt mode, walker_colours.txt lines, walker_crops.txt).
"""
import csv, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import walker_sprites as ws

HERE = os.path.dirname(os.path.abspath(__file__))
SM = "https://www.smogon.com/forums/attachments/"

# key, form name, large URL, Bulbagarden icon, spriter (from the sheets' "Spriter" column, filled in by fetch)
FORMS = [
    # Vivillon: game forms 0 Icy Snow .. 19 Poke Ball; 6 Meadow = the pack's 666.bin
    ("666_f0", "Vivillon Icy Snow", SM + "666_7-png.273592/", "666IcyMS6.png"),
    ("666_f1", "Vivillon Polar", SM + "666_2-png.273582/", "666PolMS6.png"),
    ("666_f2", "Vivillon Tundra", SM + "666_3-png.273584/", "666TunMS6.png"),
    ("666_f3", "Vivillon Continental", SM + "666_4-png.273586/", "666ConMS6.png"),
    ("666_f4", "Vivillon Garden", SM + "666_5-png.273588/", "666GarMS6.png"),
    ("666_f5", "Vivillon Elegant", SM + "666_6-png.273590/", "666EleMS6.png"),
    ("666_f7", "Vivillon Modern", SM + "666_8-png.273594/", "666ModMS6.png"),
    ("666_f8", "Vivillon Marine", SM + "666_9-png.273596/", "666MarMS6.png"),
    ("666_f9", "Vivillon Archipelago", SM + "666_10-png.273598/", "666ArcMS6.png"),
    ("666_f10", "Vivillon High Plains", SM + "666_11-png.273600/", "666HigMS6.png"),
    ("666_f11", "Vivillon Sandstorm", SM + "666_12-png.273602/", "666SanMS6.png"),
    ("666_f12", "Vivillon River", SM + "666_13-png.273604/", "666RivMS6.png"),
    ("666_f13", "Vivillon Monsoon", SM + "666_14-png.273606/", "666MonMS6.png"),
    ("666_f14", "Vivillon Savanna", SM + "666_15-png.273608/", "666SavMS6.png"),
    ("666_f15", "Vivillon Sun", SM + "666_16-png.273610/", "666SunMS6.png"),
    ("666_f16", "Vivillon Ocean", SM + "666_17-png.273612/", "666OceMS6.png"),
    ("666_f17", "Vivillon Jungle", SM + "666_18-png.273614/", "666JunMS6.png"),
    ("666_f18", "Vivillon Fancy", SM + "666_19-png.273616/", "666FanMS6.png"),
    ("666_f19", "Vivillon Poke Ball", SM + "666_20-png.273618/", "666PokMS6.png"),
    # Flabebe line: 0 Red (= pack), 1 Yellow, 2 Orange, 3 Blue, 4 White, Floette 5 Eternal
    ("669_f1", "Flabebe Yellow", SM + "669_2-png.276628/", "669YMS6.png"),
    ("669_f2", "Flabebe Orange", SM + "669_4-png.276636/", "669OMS6.png"),
    ("669_f3", "Flabebe Blue", SM + "669_3-png.276632/", "669BMS6.png"),
    ("669_f4", "Flabebe White", SM + "669_5-png.276640/", "669WMS6.png"),
    ("670_f1", "Floette Yellow", SM + "670_2-png.276648/", "670YMS6.png"),
    ("670_f2", "Floette Orange", SM + "670_4-png.276656/", "670OMS6.png"),
    ("670_f3", "Floette Blue", SM + "670_3-png.276652/", "670BMS6.png"),
    ("670_f4", "Floette White", SM + "670_5-png.276660/", "670WMS6.png"),
    ("670_f5", "Floette Eternal Flower", SM + "670_6-png.276664/", "670EMS6.png"),
    ("671_f1", "Florges Yellow", SM + "671_2-png.276672/", "671YMS6.png"),
    ("671_f2", "Florges Orange", SM + "671_4-png.276680/", "671OMS6.png"),
    ("671_f3", "Florges Blue", SM + "671_3-png.276676/", "671BMS6.png"),
    ("671_f4", "Florges White", SM + "671_5-png.276684/", "671WMS6.png"),
    # gender: Pyroar female (form 0, female); Meowstic female is form 1 in Gen 6/7 saves
    ("668_f", "Pyroar female", SM + "668f-png.653039/", "668FMS6.png"),
    ("678_f1", "Meowstic female", SM + "678f-png.273647/", "678FMS6.png"),
    # Pumpkaboo / Gourgeist: 0 Average (= pack), 1 Small; Large and Super Size have no sheet picture
    ("710_f1", "Pumpkaboo Small", "https://i.imgur.com/r141O5r.png", "710MS6.png"),
    ("711_f1", "Gourgeist Small", "https://i.imgur.com/u5fExB0.png", "711MS6.png"),
    ("716_f0", "Xerneas Neutral", SM + "716_2-png.274485/", "716NMS6.png"),
    ("718_f1", "Zygarde 10%", SM + "718-d-png.113263/", "718TMS6.png"),
    ("720_f1", "Hoopa Unbound", SM + "720_2-png.282257/", "720UMS6.png"),
    # Gen 7
    ("741_f1", "Oricorio Pom-Pom", SM + "741_2-png.127364/", "741PoMS6.png"),
    ("741_f2", "Oricorio Pa'u", SM + "741_3-png.127365/", "741PaMS6.png"),
    ("741_f3", "Oricorio Sensu", SM + "741_4-png.127366/", "741SeMS6.png"),
    ("745_f1", "Lycanroc Midnight", SM + "745_2-png.102288/", "745MnMS6.png"),
    ("745_f2", "Lycanroc Dusk", SM + "745_3-png.102290/", "745DMS6.png"),
    # Silvally: form = type number (0 Normal = pack); one menu icon for all
    ("773_f1", "Silvally Fighting", SM + "773_15-png.125076/", "773MS6.png"),
    ("773_f2", "Silvally Flying", SM + "773_5-png.125066/", "773MS6.png"),
    ("773_f3", "Silvally Poison", SM + "773_7-png.125068/", "773MS6.png"),
    ("773_f4", "Silvally Ground", SM + "773_11-png.125072/", "773MS6.png"),
    ("773_f5", "Silvally Rock", SM + "773_10-png.125071/", "773MS6.png"),
    ("773_f6", "Silvally Bug", SM + "773_6-png.125067/", "773MS6.png"),
    ("773_f7", "Silvally Ghost", SM + "773_13-png.125074/", "773MS6.png"),
    ("773_f8", "Silvally Steel", SM + "773_14-png.125075/", "773MS6.png"),
    ("773_f9", "Silvally Fire", SM + "773_3-png.125064/", "773MS6.png"),
    ("773_f10", "Silvally Water", SM + "773_4-png.125065/", "773MS6.png"),
    ("773_f11", "Silvally Grass", SM + "773_2-png.125063/", "773MS6.png"),
    ("773_f12", "Silvally Electric", SM + "773_8-png.125069/", "773MS6.png"),
    ("773_f13", "Silvally Psychic", SM + "773_9-png.125070/", "773MS6.png"),
    ("773_f14", "Silvally Ice", SM + "773_16-png.125077/", "773MS6.png"),
    ("773_f15", "Silvally Dragon", SM + "773_17-png.125078/", "773MS6.png"),
    ("773_f16", "Silvally Dark", SM + "773_12-png.125073/", "773MS6.png"),
    ("773_f17", "Silvally Fairy", SM + "773_18-png.125079/", "773MS6.png"),
    # Minior: 0-6 Meteor (battle only), 7-13 cores Red Orange Yellow Green Blue Indigo Violet (the boxed form)
    ("774_f7", "Minior Red core", SM + "774_2-png.102501/", "774RMS6.png"),
    ("774_f8", "Minior Orange core", SM + "774_5-png.114222/", "774OMS6.png"),
    ("774_f9", "Minior Yellow core", SM + "774_8-png.114224/", "774YMS6.png"),
    ("774_f10", "Minior Green core", SM + "774_3-png.114220/", "774GMS6.png"),
    ("774_f11", "Minior Blue core", SM + "774_2-png.114219/", "774BMS6.png"),
    ("774_f12", "Minior Indigo core", SM + "774_4-png.114221/", "774IMS6.png"),
    ("774_f13", "Minior Violet core", SM + "774_7-png.114223/", "774VMS6.png"),
    ("800_f1", "Necrozma Dusk Mane", SM + "800_2-png.272376/", "800DMMS6.png"),
    ("800_f2", "Necrozma Dawn Wings", SM + "800_3-png.169358/", "800DWMS6.png"),
    ("801_f1", "Magearna Original Color", SM + "801_2-png.114011/", "801OMS6.png"),
]


# window shifts that rebuild the frozen pack files exactly (666 Vivillon, 678 Meowstic, 773 Silvally); used for
# their forms, same body. 800 Necrozma (-10, 6) is not used: Dusk Mane / Dawn Wings have other bodies.
SAME_BODY_SHIFT = {"666": (0, 16), "678": (0, 1), "773": (0, 1)}


def fetch(d):
    for sub in ("large", "small"):
        os.makedirs(os.path.join(d, sub), exist_ok=True)
    cr = os.path.join(d, "credits.csv")
    for key, name, large, icon in FORMS:
        got = []
        for sub, url in (("large", large), ("small", ws.bulba_url(icon))):
            out = os.path.join(d, sub, key + ".png")
            if os.path.exists(out):
                continue
            ok = ws.fetch_one([url], out)
            got.append("%s %s" % (sub, "ok" if ok else "MISSING"))
            if ok:
                with open(cr, "a", encoding="utf-8") as c:
                    c.write("%s,%s,%s,%s\n" % (key, sub, ok, "Smogon Sprite Project" if sub == "large" else "Game Freak"))
        print("%s %s: %s" % (key, name, ", ".join(got) or "cached"), flush=True)


_OFFSETS = {}


def keep_base(key, src, big, pack):
    """Forms drawn on the base's exact outline (Vivillon patterns, Silvally types, flower colours, Magearna): every
    walker pixel whose source pixel has the SAME colour as in the base picture takes the app's approved base shade
    (user: "A lot of vivillion's eyes don't turn out right" - eyes, head and body stay as approved); only the
    recoloured parts come from the conversion. Returns (big, kept pixels) or (big, None) when the outlines differ."""
    from PIL import ImageOps
    b = ws.first_frame("art_src/sprites5to7/large/%s.png" % key[:3]).convert("RGBA")
    f = src.convert("RGBA")
    if b.size != f.size or any((p[3] > 0) != (q[3] > 0) for p, q in zip(b.getdata(), f.getdata())):
        return big, None
    pk = ws.decode(open(os.path.join(pack, key[:3] + ".bin"), "rb").read()[0x180:0x480], 64, 48)
    import numpy as np
    mb, mf = ImageOps.mirror(b), ImageOps.mirror(f)
    W, H = mb.size
    if key[:3] not in _OFFSETS:
        A = np.zeros((H + 96, W + 128), bool)
        A[48:48 + H, 64:64 + W] = np.array(mb)[:, :, 3] > 0
        P = np.array(pk)
        best = None
        for oy in range(-48, H):
            for ox in range(-64, W):
                a = A[oy + 48:oy + 96, ox + 64:ox + 128]
                bad = int(((~a) & (P > 0)).sum())
                good = int((a & (P == 3)).sum())
                if best is None or good - 10 * bad > best[0]:
                    best = (good - 10 * bad, ox, oy, bad)
        _OFFSETS[key[:3]] = best
    _, ox, oy, bad = _OFFSETS[key[:3]]
    assert bad == 0, (key, _OFFSETS[key[:3]])
    out = [row[:] for row in big[0]]
    kept = 0
    for y in range(48):
        for x in range(64):
            sx, sy = x + ox, y + oy
            if 0 <= sx < W and 0 <= sy < H:
                p, q = mb.getpixel((sx, sy)), mf.getpixel((sx, sy))
                if p[3] > 0 and p == q:
                    out[y][x] = pk[y][x]
                    kept += 1
    return [out, [row[:] for row in out]], kept


def heads():
    out = {}
    p = os.path.join(HERE, "walker_forms67_heads.txt")
    if os.path.exists(p):
        for line in open(p, encoding="utf-8"):
            q = line.split("#")[0].split()
            if len(q) == 3:
                out[q[0]] = (int(q[1]), int(q[2]))
    return out


def convert(d, out, only=None):
    from PIL import Image, ImageDraw
    # The base Pokemon's own settings (review mode, colour lines, crops) are used: with them, a plain convert of the
    # base picture gives the app's pack file byte for byte for 12 of the 17 bases. Four packs are frozen older
    # versions whose window sits elsewhere; the shift below reproduces them exactly (checked) and is used for the
    # forms with the same body. Pyroar's pack file is a hand edit (not reproducible).
    hd = heads()
    cur = {"key": None}
    base_head = ws.head_point
    ws.head_point = lambda num: hd.get(cur["key"]) or base_head(num)
    os.makedirs(out, exist_ok=True)
    done = []
    for key, name, _, _ in FORMS:
        if only and key not in only:
            continue
        lp = os.path.join(d, "large", key + ".png")
        if not os.path.exists(lp):
            print(key, "no large picture")
            continue
        num = int(key[:3])
        cur["key"] = key
        src = ws.first_frame(lp)
        shift = SAME_BODY_SHIFT.get(key[:3], (0, 0)) if key not in hd else (0, 0)
        big = ws.to_walker_frames([src, src], 64, 48, num, window_shift=shift)
        big, kept = keep_base(key, src, big, "romfs/sprites/pack")
        if kept is not None:
            print(key, "base pixels kept:", kept, flush=True)
        ic = ws.all_frames(os.path.join(d, "small", key + ".png"))
        if len(ic) == 1:
            ic = ic * 2
        small = ws.icon_frames(ic[:2], num)
        data = ws.encode(small[0], 32, 24) + ws.encode(small[1], 32, 24) + ws.encode(big[0], 64, 48) + ws.encode(big[1], 64, 48)
        assert len(data) == 0x780
        open(os.path.join(out, key + ".bin"), "wb").write(data)
        done.append((key, name, src, big, small))
    print("converted %d" % len(done))
    return done


def sheet(out, pack, per=24):
    """Review pages: source | base (pack) | new large | new small, at 3x."""
    from PIL import Image, ImageDraw
    S, gap = 3, 10
    rows = []
    for key, name, _, _ in FORMS:
        p = os.path.join(out, key + ".bin")
        if os.path.exists(p):
            rows.append((key, name, open(p, "rb").read()))
    W = gap * 5 + 64 * S * 2 + 32 * S + 150
    for pg in range(0, len(rows), per):
        chunk = rows[pg:pg + per]
        im = Image.new("RGB", (W, 22 + len(chunk) * (48 * S + gap)), "white")
        dr = ImageDraw.Draw(im)
        dr.text((gap, 5), "form                 base Pokemon (app now)        NEW large                      NEW small", fill="black")
        for i, (key, name, b) in enumerate(chunk):
            y = 22 + i * (48 * S + gap)
            dr.text((gap, y + 4), name, fill=(190, 0, 0))
            dr.text((gap, y + 20), key + ".bin", fill="black")
            x = gap + 150
            bp = os.path.join(pack, key[:3] + ".bin")
            if os.path.exists(bp):
                im.paste(ws.preview(ws.decode(open(bp, "rb").read()[0x180:0x480], 64, 48), S).convert("RGB"), (x, y))
            x += 64 * S + gap
            im.paste(ws.preview(ws.decode(b[0x180:0x480], 64, 48), S).convert("RGB"), (x, y))
            x += 64 * S + gap
            im.paste(ws.preview(ws.decode(b[0:0xC0], 32, 24), S).convert("RGB"), (x, y))
        fn = os.path.join(out, "preview_%d.png" % (pg // per + 1))
        im.save(fn)
        print(fn)


if __name__ == "__main__":
    if sys.argv[1] == "fetch":
        fetch(sys.argv[2])
    elif sys.argv[1] == "convert":
        convert(sys.argv[2], sys.argv[3], set(sys.argv[4:]) or None)
        sheet(sys.argv[3], "romfs/sprites/pack")
