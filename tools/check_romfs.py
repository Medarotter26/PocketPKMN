#!/usr/bin/env python3
"""check_romfs.py <pocketpkmn.3dsx|pocketpkmn.cia> <romfs dir> - checks that the romfs packed into a .3dsx or an
(unencrypted, as cia/pocketpkmn.rsf makes it) .cia holds exactly the files of the romfs folder, byte for byte.
Prints the file count and the walker pack count (sprites/pack); exit 1 on any difference.
"""
import os
import struct
import sys


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def level3_files(b, base):
    """Files of a RomFS level-3 image starting at b[base:], as {path: bytes}."""
    hdr = struct.unpack_from("<10I", b, base)
    dir_meta_off, file_meta_off, file_meta_len, data_off = hdr[3], hdr[7], hdr[8], hdr[9]

    def dir_path(off):
        parts = []
        while off != 0:
            p = base + dir_meta_off + off
            parent, nlen = u32(b, p), u32(b, p + 0x14)
            parts.append(b[p + 0x18:p + 0x18 + nlen].decode("utf-16-le"))
            off = parent
        return "/".join(reversed(parts))

    out = {}
    o = 0
    while o < file_meta_len:
        p = base + file_meta_off + o
        parent = u32(b, p)
        doff, dsize = u64(b, p + 8), u64(b, p + 0x10)
        nlen = u32(b, p + 0x1C)
        name = b[p + 0x20:p + 0x20 + nlen].decode("utf-16-le")
        d = dir_path(parent)
        start = base + data_off + doff
        out[(d + "/" if d else "") + name] = b[start:start + dsize]
        o += (0x20 + nlen + 3) & ~3
    return out


def romfs_of_3dsx(b):
    assert b[:4] == b"3DSX"
    hsize = struct.unpack_from("<H", b, 4)[0]
    assert hsize > 0x20, "no extended header (no romfs)"
    return level3_files(b, u32(b, 0x20 + 8))


def romfs_of_cia(b):
    al = lambda x: (x + 63) & ~63
    hsz, _, _, cert, tik, tmd, meta = struct.unpack_from("<IHHIIII", b, 0)
    content = al(hsz) + al(cert) + al(tik) + al(tmd)
    n = content
    assert b[n + 0x100:n + 0x104] == b"NCCH", "content 0 is not an NCCH"
    flags7 = b[n + 0x188 + 7]
    assert flags7 & 4, "NCCH is encrypted - this checker reads unencrypted CIAs only"
    romfs = n + u32(b, n + 0x1B0) * 0x200
    assert b[romfs:romfs + 4] == b"IVFC"
    master = u32(b, romfs + 8)
    l3_block = 1 << u32(b, romfs + 0x4C)
    l3 = romfs + ((0x60 + master + l3_block - 1) & ~(l3_block - 1))
    return level3_files(b, l3)


def main():
    path, folder = sys.argv[1], sys.argv[2]
    b = open(path, "rb").read()
    files = romfs_of_cia(b) if path.lower().endswith(".cia") else romfs_of_3dsx(b)
    want = {}
    for root, _, names in os.walk(folder):
        for nm in names:
            full = os.path.join(root, nm)
            want[os.path.relpath(full, folder).replace(os.sep, "/")] = open(full, "rb").read()
    bad = sorted(k for k in want if files.get(k) != want[k]) + sorted(k for k in files if k not in want)
    pack = [k for k in files if k.startswith("sprites/pack/")]
    print(f"{os.path.basename(path)}: {len(files)} files in its romfs, folder has {len(want)}; "
          f"sprites/pack: {len(pack)}; differences: {len(bad)}")
    for k in bad[:10]:
        print("  differs/missing:", k)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
