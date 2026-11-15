#!/usr/bin/env python3
"""walker_flat_textures.py <walker model dir> <out dir> - flattened copies of the walker model's colour textures
(user 2026-11-15: "the pokewalker model looks too detailed compared to the 3ds model ... tone that one down"):
median filter (removes painted scuffs/shading), then every pixel snapped to a few flat colours. The originals are
not changed; tools/render_3ds.py uses the copies when they exist."""
import os
import sys

from PIL import Image, ImageFilter

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
PALETTES = {
    "Base_baseColor.png": [(244, 244, 244), (206, 36, 48), (36, 36, 40), (150, 152, 156)],
    "Buttons_baseColor.png": [(226, 228, 232), (150, 152, 156), (36, 36, 40)],
    "Strap_baseColor.png": [(60, 62, 68), (36, 36, 40)],
}
for name, pal in PALETTES.items():
    im = Image.open(os.path.join(src, "textures", name)).convert("RGB")
    im = im.filter(ImageFilter.MedianFilter(7))
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b = px[x, y]
            px[x, y] = min(pal, key=lambda c: (c[0] - r) ** 2 + (c[1] - g) ** 2 + (c[2] - b) ** 2)
    im.save(os.path.join(out, name))
    print("->", os.path.join(out, name))
