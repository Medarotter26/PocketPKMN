#!/usr/bin/env python3
"""walk_preview.py <walk.bin | eeprom backup> <out.png> - every picture of a walk upload (0x2900 bytes, or a
64 KiB backup: its walk at 0x8F00) on one sheet, at 3x, walker greys."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pwimg  # noqa: E402

data = open(sys.argv[1], "rb").read()
walk = data[0x8F00:0xB800] if len(data) == 0x10000 else data
parts = [  # (offset, w, h, column, row) on a 100 x 16 px grid
    (0x00BE, 32, 24, 0, 0), (0x017E, 80, 16, 40, 0),                      # route image, route name
    (0x02BE, 32, 24, 0, 30), (0x037E, 32, 24, 40, 30),                    # walking: small f1 f2
    (0x043E, 64, 48, 0, 60), (0x073E, 64, 48, 70, 60), (0x0A3E, 80, 16, 140, 60),  # large f1 f2, name
    (0x0B7E, 32, 24, 0, 115), (0x0CFE, 32, 24, 40, 115), (0x0E7E, 32, 24, 80, 115),  # route smalls f1
    (0x15FE, 80, 16, 120, 115), (0x173E, 80, 16, 120, 133), (0x187E, 80, 16, 120, 151),  # route names
    (0x0FFE, 64, 48, 0, 145),                                             # route Pokemon C large f1
]
for i in range(10):
    parts.append((0x19BE + i * 0x180, 96, 16, (i % 2) * 100, 200 + (i // 2) * 18))
W, H, S = 230, 290, 3
px = [[(120, 160, 200)] * (W * S) for _ in range(H * S)]
grey = [(255, 255, 255), (168, 168, 168), (80, 80, 80), (0, 0, 0)]
for off, w, h, cx, cy in parts:
    sh = pwimg.decode(walk[off:off + w * h // 4], w, h)
    for y in range(h):
        for x in range(w):
            c = grey[sh[y * w + x]]
            for dy in range(S):
                row = px[(cy + y) * S + dy]
                for dx in range(S):
                    row[(cx + x) * S + dx] = c
pwimg.write_png(sys.argv[2], W * S, H * S, [b"".join(bytes(c) for c in r) for r in px])
print("->", sys.argv[2])
