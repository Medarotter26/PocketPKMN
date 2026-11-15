#!/usr/bin/env python3
"""Complete walker sprite files for Gen 1-2: small (minisprites, real size) + large
(Crystal), same layout as art_src/sprites5to7/out/NNN.bin: small frame 1, small frame
2, large frame 1, large frame 2 (0x780 bytes). Unown letters (201B..201Z) use the one
Unown minisprite. Usage: walker_gen12_combine.py <minisprites_out> <crystal_out> <out>"""
import os, sys
mini, cry, out = sys.argv[1:4]
os.makedirs(out, exist_ok=True)
n = 0
for f in sorted(os.listdir(cry)):
    if not f.endswith(".large.bin"):
        continue
    name = f[:-len(".large.bin")]
    small = os.path.join(mini, "%s.small.bin" % name[:3])
    if not os.path.exists(small):
        print("no small sprite for", name)
        continue
    s = open(small, "rb").read()
    l = open(os.path.join(cry, f), "rb").read()
    assert len(s) == 0x180 and len(l) == 0x600
    open(os.path.join(out, name + ".bin"), "wb").write(s + l)
    n += 1
print("wrote %d complete files to %s" % (n, out))
