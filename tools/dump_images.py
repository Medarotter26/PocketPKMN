#!/usr/bin/env python3
"""dump_images.py - decode walker images from a 64 KiB EEPROM dump to PNG.

  python3 tools/dump_images.py EEPROM.bin [OUTDIR] [--scale N] [ADDR:WxH[xFRAMES][:name] ...]

Without image specs it dumps a default set (route picture/name, walking
Pokemon sprites/name, a menu heading and a message string). Each image is
also round-tripped (encode(decode(x)) == x) and the result printed.
Output default: hw/dump/<eeprom file name>/. Dumps are the user's data and
Nintendo artwork: keep them in hw/dump only.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pwimg  # noqa: E402

DEFAULT = [
    (0x0910, 80, 16, 1, "heading_0910"),
    (0x2530, 96, 16, 1, "text_2530_connecting"),
    (0x8FBE, 32, 24, 1, "route_image"),
    (0x907E, 80, 16, 1, "route_name"),
    (0x91BE, 32, 24, 2, "pokemon_small"),
    (0x933E, 64, 48, 2, "pokemon_large"),
    (0x993E, 80, 16, 1, "pokemon_name"),
]


def parse_spec(s):
    parts = s.split(":")
    addr = int(parts[0], 16)
    dims = parts[1].lower().split("x")
    w, h = int(dims[0]), int(dims[1])
    frames = int(dims[2]) if len(dims) > 2 else 1
    name = parts[2] if len(parts) > 2 else "img_%04X" % addr
    return addr, w, h, frames, name


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(2)
    scale = 3
    if "--scale" in args:
        i = args.index("--scale")
        scale = int(args[i + 1])
        del args[i:i + 2]
    src = args[0]
    rest = args[1:]
    outdir = None
    if rest and ":" not in rest[0]:
        outdir = rest[0]
        rest = rest[1:]
    if outdir is None:
        base = os.path.splitext(os.path.basename(src))[0]
        outdir = os.path.join(pwimg.ROOT, "hw", "dump", base)
    os.makedirs(outdir, exist_ok=True)
    specs = [parse_spec(s) for s in rest] or DEFAULT

    with open(src, "rb") as f:
        eep = f.read()
    if len(eep) != 65536:
        print("warning: %s is %d bytes, not 65536" % (src, len(eep)))
    bad = 0
    for addr, w, h, frames, name in specs:
        size = pwimg.img_size(w, h)
        for fr in range(frames):
            a = addr + fr * size
            data = eep[a:a + size]
            sh = pwimg.decode(data, w, h)
            ok = pwimg.encode(sh, w, h) == data
            bad += 0 if ok else 1
            counts = [sh.count(v) for v in range(4)]
            fn = name + ("_f%d" % fr if frames > 1 else "") + ".png"
            pwimg.shades_to_png(os.path.join(outdir, fn), sh, w, h, scale)
            print("0x%04X %3dx%-3d %-28s round-trip %s  shades 0/1/2/3 = %s" %
                  (a, w, h, fn, "OK" if ok else "FAIL", counts))
    print("wrote %d image(s) to %s" % (sum(s[3] for s in specs), outdir))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
