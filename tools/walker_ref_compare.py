#!/usr/bin/env python3
"""Fetch a few stock Pokewalker sprites and the matching HGSS front sprites
(Bulbagarden Archives) and put them side by side, to learn how the walker art
was made from the DS art. Usage: walker_ref_compare.py <cache_dir> <out.png> <dex...>"""
import os, sys, time
sys.path.insert(0, os.path.dirname(__file__))
from walker_sprites import bulba_url, get, first_frame


def fetch(cache, name):
    path = os.path.join(cache, name.replace(" ", "_"))
    if not os.path.exists(path):
        try:
            data = get(bulba_url(name))
        except Exception:
            return None
        finally:
            time.sleep(1.0)
        if not data.startswith(b"\x89PNG"):
            return None            # a page, not the file (no such name)
        open(path, "wb").write(data)
    elif not open(path, "rb").read(4).startswith(b"\x89PNG"):
        os.remove(path)
        return None
    return path


def main():
    from PIL import Image
    cache, out = sys.argv[1], sys.argv[2]
    os.makedirs(cache, exist_ok=True)
    rows = []
    for d in sys.argv[3:]:
        n = int(d)
        pw = fetch(cache, "Spr 4pw %03d.png" % n) or fetch(cache, "Spr 4pw %03d m.png" % n)
        hg = fetch(cache, "Spr 4h %03d.png" % n) or fetch(cache, "Spr 4h %03d m.png" % n)
        rows.append((n, pw, hg))
    S = 3
    sheet = Image.new("RGB", (80 * S + 64 * S + 30, 80 * S * len(rows)), "white")
    for i, (n, pw, hg) in enumerate(rows):
        if hg:
            im = first_frame(hg)
            bg = Image.new("RGB", im.size, "white")
            bg.paste(im, mask=im.getchannel("A"))
            sheet.paste(bg.resize((im.width * S, im.height * S), Image.NEAREST), (0, i * 80 * S))
        if pw:
            im = Image.open(pw).convert("RGB")
            sheet.paste(im.resize((im.width * S, im.height * S), Image.NEAREST), (80 * S + 20, i * 80 * S + 20))
    sheet.save(out)
    print(out)


main()
