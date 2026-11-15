#!/usr/bin/env python3
"""walker_assets.py - build the walker sprite pack for the WalkerBox app (runs on the user's PC).

The pack is one file per species, sprites/NNN.bin (NNN = national dex number, 3 digits), 0x780 bytes:
small sprite 32x24 frame 1 + frame 2 (0x180), large sprite 64x48 frame 1 + frame 2 (0x600) - the same bytes,
in the same order, as the walker holds at EEPROM 0x91BE-0x993D for the walking Pokemon.

Sources (nothing from Nintendo or Smogon is part of the app or the repo; this tool reads the user's own files):
  #1-493    the user's own HeartGold / SoulSilver ROM: the stock walker sprites
            (NARC a/2/4/8 = small, index = species; NARC a/2/5/6 = large, index = species - 1;
            both LZ10-compressed; mapping checked 2026-11-14 against the user's walker backup and 110 references)
  #494-807  the reviewed conversions in art_src/sprites5to7/out (copied byte for byte)

Usage:
  walker_assets.py build --hgss <rom.nds> [--gen5to7 <out dir>] [--dest <folder>]
      writes <dest>/3ds/walkerbox/sprites/NNN.bin (default dest: sd_out/ in the project)
  walker_assets.py sheet <dest> <png>
      contact sheet of every file in the pack (large frame 1 + small frame 1)
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import pwimg  # noqa: E402

SMALL_SIZE = 0x180
LARGE_SIZE = 0x600
ENTRY_SIZE = SMALL_SIZE + LARGE_SIZE
LAST_ROM_SPECIES = 493
LAST_SPECIES = 807
NARC_SMALL = "a/2/4/8"
NARC_LARGE = "a/2/5/6"


# ------------------------------------------------------------------ NDS ROM reading

def rom_files(rom):
    """Path -> (start, end) for every file in the ROM's file system."""
    fnt_off, _fnt_size, fat_off, fat_size = struct.unpack_from("<IIII", rom, 0x40)
    fat = [struct.unpack_from("<II", rom, fat_off + 8 * i) for i in range(fat_size // 8)]
    files = {}

    def walk(dir_id, path):
        off, first, _ = struct.unpack_from("<IHH", rom, fnt_off + 8 * (dir_id & 0xFFF))
        p = fnt_off + off
        fid = first
        while True:
            b = rom[p]
            p += 1
            if b == 0:
                break
            name = rom[p:p + (b & 0x7F)].decode("latin1")
            p += b & 0x7F
            if b & 0x80:
                sub = struct.unpack_from("<H", rom, p)[0]
                p += 2
                walk(sub, path + name + "/")
            else:
                files[path + name] = fat[fid]
                fid += 1

    walk(0xF000, "")
    return files


def narc_members(data):
    if data[:4] != b"NARC":
        raise ValueError("not a NARC archive")
    blocks = {}
    p = 0x10
    while p + 8 <= len(data):
        tag = data[p:p + 4]
        size = struct.unpack_from("<I", data, p + 4)[0]
        if size < 8:
            break
        blocks[tag] = p
        p += size
    btaf, gmif = blocks[b"BTAF"], blocks[b"GMIF"]
    count = struct.unpack_from("<H", data, btaf + 8)[0]
    out = []
    for i in range(count):
        a, b = struct.unpack_from("<II", data, btaf + 12 + 8 * i)
        out.append(data[gmif + 8 + a:gmif + 8 + b])
    return out


def lz10(data):
    if data[0] != 0x10:
        raise ValueError("not LZ10 data")
    n = data[1] | data[2] << 8 | data[3] << 16
    out = bytearray()
    p = 4
    while len(out) < n:
        flags = data[p]
        p += 1
        for bit in range(8):
            if len(out) >= n:
                break
            if flags & (0x80 >> bit):
                b0, b1 = data[p], data[p + 1]
                p += 2
                length = (b0 >> 4) + 3
                disp = ((b0 & 0xF) << 8 | b1) + 1
                for _ in range(length):
                    out.append(out[-disp])
            else:
                out.append(data[p])
                p += 1
    return bytes(out[:n])


def hgss_sprites(rom_path):
    rom = open(rom_path, "rb").read()
    code = rom[0x0C:0x10].decode("latin1", "replace")
    if code[:3] not in ("IPK", "IPG"):
        raise SystemExit(f"{rom_path}: game code {code!r} is not HeartGold (IPK*) or SoulSilver (IPG*)")
    files = rom_files(rom)
    small = [lz10(m) for m in narc_members(rom[slice(*files[NARC_SMALL])])]
    large = [lz10(m) for m in narc_members(rom[slice(*files[NARC_LARGE])])]
    if len(small) <= LAST_ROM_SPECIES or len(large) < LAST_ROM_SPECIES:
        raise SystemExit("unexpected archive sizes - not a supported HGSS ROM")
    out = {}
    for dex in range(1, LAST_ROM_SPECIES + 1):
        s, l = small[dex], large[dex - 1]
        if len(s) != SMALL_SIZE or len(l) != LARGE_SIZE:
            raise SystemExit(f"#{dex}: unexpected sprite sizes {len(s):#x} / {len(l):#x}")
        out[dex] = s + l
    return code, out


# ------------------------------------------------------------------ commands

def write_if_changed(path, data):
    if os.path.exists(path) and open(path, "rb").read() == data:
        return False
    tmp = path + ".tmp"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)
    return True


def cmd_build(args):
    dest = os.path.join(args.dest, "3ds", "walkerbox", "sprites")
    os.makedirs(dest, exist_ok=True)
    code, rom_sprites = hgss_sprites(args.hgss)
    pack = dict(rom_sprites)
    missing = []
    for dex in range(LAST_ROM_SPECIES + 1, LAST_SPECIES + 1):
        src = os.path.join(args.gen5to7, f"{dex:03d}.bin")
        if not os.path.exists(src):
            missing.append(dex)
            continue
        data = open(src, "rb").read()
        if len(data) != ENTRY_SIZE:
            raise SystemExit(f"{src}: {len(data):#x} bytes, expected {ENTRY_SIZE:#x}")
        pack[dex] = data
    changed = 0
    for dex, data in sorted(pack.items()):
        path = os.path.join(dest, f"{dex:03d}.bin")
        changed += write_if_changed(path, data)
        if open(path, "rb").read() != data:
            raise SystemExit(f"{path}: read-back differs")
    print(f"ROM {code}: #1-{LAST_ROM_SPECIES} from the ROM; #{LAST_ROM_SPECIES + 1}-{LAST_SPECIES} from {args.gen5to7}")
    print(f"{len(pack)} sprite files in {dest} ({changed} written or changed, all read back and checked)")
    if missing:
        print(f"missing (the app shows its placeholder): {missing}")


def cmd_sheet(args):
    sprites = os.path.join(args.dest, "3ds", "walkerbox", "sprites")
    names = sorted(n for n in os.listdir(sprites) if n.endswith(".bin"))
    cols, cw, ch = 20, 64 + 32 + 6, 48 + 4
    rows = (len(names) + cols - 1) // cols
    w, h = cols * cw, rows * ch
    px = bytearray([2] * (w * h))  # 2 = gap colour index
    for i, n in enumerate(names):
        data = open(os.path.join(sprites, n), "rb").read()
        x0, y0 = (i % cols) * cw + 2, (i // cols) * ch + 2
        for img, iw, ih, dx in ((data[SMALL_SIZE:SMALL_SIZE + 0x300], 64, 48, 0), (data[:0xC0], 32, 24, 66)):
            sh = pwimg.decode(img, iw, ih)
            for y in range(ih):
                for x in range(iw):
                    px[(y0 + y) * w + x0 + dx + x] = 4 + sh[y * iw + x]
    palette = [(0, 0, 0), (0, 0, 0), (120, 160, 200), (0, 0, 0), (255, 255, 255), (168, 168, 168), (80, 80, 80),
               (0, 0, 0)]
    rows_rgb = [b"".join(bytes(palette[v]) for v in px[y * w:(y + 1) * w]) for y in range(h)]
    pwimg.write_png(args.png, w, h, rows_rgb)
    print(f"{len(names)} sprites -> {args.png}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build")
    b.add_argument("--hgss", required=True)
    b.add_argument("--gen5to7", default=os.path.join(ROOT, "art_src", "sprites5to7", "out"))
    b.add_argument("--dest", default=os.path.join(ROOT, "sd_out"))
    s = sub.add_parser("sheet")
    s.add_argument("dest")
    s.add_argument("png")
    args = ap.parse_args()
    {"build": cmd_build, "sheet": cmd_sheet}[args.cmd](args)


if __name__ == "__main__":
    main()
