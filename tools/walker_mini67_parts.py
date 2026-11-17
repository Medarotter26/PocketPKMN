"""Gen 6/7 mini sprite frame 2 - TEST (2026-11-16).

Stock Gen 1-5 walker minis: frame 2 = the body dips 1 px with the feet planted, plus a small per-Pokemon change
(tail / ears / leaf / flame / arm moves on its own). The second part is an artist's choice, so it is given per
Pokemon in tools/walker_mini67_parts.txt (body base + part boxes with their own move).

Reads romfs/sprites/pack/NNN.bin (read only), writes art_src/mini67test/NNN.bin (only bytes 0xC0-0x17F = small
frame 2 differ from pack), sheet_parts.png and anim_parts.gif.
Run from the app folder: python3 tools/walker_mini67_parts.py
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import walker_sprites as ws
from PIL import Image, ImageDraw

W, H = 32, 24
PACK = 'romfs/sprites/pack'
OUT = 'art_src/mini67test'
SPEC = os.path.join(os.path.dirname(__file__), 'walker_mini67_parts.txt')
GREY = [(255, 255, 255), (168, 168, 168), (80, 80, 80), (0, 0, 0)]


def read_spec():
    spec = {}
    for line in open(SPEC, encoding='utf-8'):
        line = line.split('#')[0].strip()
        if not line:
            continue
        fields = [f.split() for f in line.split('|')]
        head = fields[0]
        num = int(head[0])
        base = head[1]
        cut = int(head[2]) if base == 'squat' else None
        parts = [tuple(int(v) for v in f) for f in fields[1:] if f]
        spec[num] = (base, cut, parts)
    return spec


def frame2(a, base, cut, parts):
    # which part each pixel belongs to (-1 = body)
    owner = [[-1] * W for _ in range(H)]
    for i, (x0, y0, x1, y1, dx, dy) in enumerate(parts):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if 0 <= x < W and 0 <= y < H and a[y][x]:
                    owner[y][x] = i

    def body_shift(x, y):
        if base == 'bob':
            return (0, 1)
        return (0, 0) if y >= cut else (0, 1)

    def shift(x, y):
        o = owner[y][x]
        return body_shift(x, y) if o < 0 else (parts[o][4], parts[o][5])

    out = [[0] * W for _ in range(H)]

    def put(x, y, v, only_white=False):
        if 0 <= x < W and 0 <= y < H and (not only_white or out[y][x] == 0):
            out[y][x] = v

    # 1. body: feet first, then upper rows bottom-up (upper parts win where the dip overlaps the feet)
    body = [(x, y) for y in range(H) for x in range(W) if a[y][x] and owner[y][x] < 0]
    body.sort(key=lambda p: (body_shift(*p) != (0, 0), -p[1]))
    for x, y in body:
        dx, dy = body_shift(x, y)
        put(x + dx, y + dy, a[y][x])
    # 2. joints: where a part touches the body (or another part) and the two moved apart, stretch the part pixel
    for y in range(H):
        for x in range(W):
            if not a[y][x] or owner[y][x] < 0:
                continue
            px, py = shift(x, y)
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if 0 <= nx < W and 0 <= ny < H and a[ny][nx] and owner[ny][nx] != owner[y][x]:
                    qx, qy = shift(nx, ny)
                    steps = max(abs(qx - px), abs(qy - py))
                    for s in range(1, steps + 1):
                        put(x + px + round((qx - px) * s / steps), y + py + round((qy - py) * s / steps), a[y][x],
                            only_white=True)
    # 3. parts on top
    for y in range(H):
        for x in range(W):
            if a[y][x] and owner[y][x] >= 0:
                dx, dy = shift(x, y)
                put(x + dx, y + dy, a[y][x])
    # 4. body gaps left by a dip (squat): a white pixel between two body pixels of the same column that were
    #    neighbours in frame 1 -> close it with the lower one's shade
    return out


def main():
    spec = read_spec()
    os.makedirs(OUT, exist_ok=True)
    cells = []
    for num, (base, cut, parts) in sorted(spec.items()):
        data = open('%s/%03d.bin' % (PACK, num), 'rb').read()
        a = ws.decode(data[:0xC0], W, H)
        f2 = frame2(a, base, cut, parts)
        new = data[:0xC0] + ws.encode(f2, W, H) + data[0x180:]
        assert len(new) == 0x780 and new[:0xC0] == data[:0xC0] and new[0x180:] == data[0x180:]
        open('%s/%03d.bin' % (OUT, num), 'wb').write(new)
        cells.append((num, a, f2, parts))
    # sheet: frame 1 | frame 2 | frame 2 with the moved parts outlined
    sc, cols = 4, 3
    cw, ch = 2 * (W * sc + 6) + 14, H * sc + 18
    im = Image.new('RGB', (cw * cols, ch * ((len(cells) + cols - 1) // cols) + 16), (228, 228, 228))
    d = ImageDraw.Draw(im)
    d.text((4, 2), 'frame 1 | new frame 2', fill=(0, 0, 0))
    for i, (num, a, f2, parts) in enumerate(cells):
        ox, oy = (i % cols) * cw + 4, 16 + (i // cols) * ch
        d.text((ox, oy), '%03d' % num, fill=(200, 0, 0))
        for k, f in enumerate((a, f2)):
            for y in range(H):
                for x in range(W):
                    X, Y = ox + k * (W * sc + 6) + x * sc, oy + 12 + y * sc
                    d.rectangle([X, Y, X + sc - 1, Y + sc - 1], fill=GREY[f[y][x]])
    im.save('%s/sheet_parts.png' % OUT)
    frames = []
    sc = 3
    for t in (0, 1):
        g = Image.new('RGB', (4 * (W * sc + 16) + 8, 3 * (H * sc + 20) + 8), (240, 240, 240))
        gd = ImageDraw.Draw(g)
        for i, (num, a, f2, parts) in enumerate(cells):
            ox, oy = 8 + (i % 4) * (W * sc + 16), 4 + (i // 4) * (H * sc + 20)
            gd.text((ox, oy), '%03d' % num, fill=(200, 0, 0))
            f = a if t == 0 else f2
            for y in range(H):
                for x in range(W):
                    X, Y = ox + x * sc, oy + 12 + y * sc
                    gd.rectangle([X, Y, X + sc - 1, Y + sc - 1], fill=GREY[f[y][x]])
        frames.append(g)
    frames[0].save('%s/anim_parts.gif' % OUT, save_all=True, append_images=frames[1:], duration=500, loop=0)
    print('wrote', len(cells))


if __name__ == '__main__':
    main()
