#!/usr/bin/env python3
"""Writes romfs/walker/hgss_menus.bin = EEPROM 0x0280-0x8ECF (menu pictures + checksummed sound block, as
HeartGold/SoulSilver writes it) from a walker backup made by the app. User 2026-11-15: "include the hgss pokewalker
menu sprites in our release app too". Checks the 16 checksummed blocks (table at 0x8CB0) first.
usage: python3 -I tools/extract_hgss_menus.py [backup] (default hw/test1/eeprom_001.bin = the user's HGSS walker)"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "hw", "test1", "eeprom_001.bin")
ee = open(src, "rb").read()
assert len(ee) == 0x10000, "not a 64 KiB walker backup"
valid = 0
for i in range(16):
    d = ee[0x8CB0 + 4 * i:0x8CB0 + 4 * i + 4]
    off, ln = d[0] | d[1] << 8, d[2]
    if 0x8CF0 + off + ln > 0x8ED0 or (sum(ee[0x8CF0 + off:0x8CF0 + off + ln]) & 255) != d[3]:
        raise SystemExit("checked block %d is not valid - pick a backup of a walker the game set up" % i)
    valid += ln > 0
out = os.path.join(ROOT, "romfs", "walker", "hgss_menus.bin")
os.makedirs(os.path.dirname(out), exist_ok=True)
blk = bytearray(ee[0x0280:0x8ED0])
# the owner's trainer-name picture (0x1250, 80x16 = 0x140 bytes) is personal and is written again by "Set up a walker"
# anyway -> blanked (white) so no player's name ships inside the app
blk[0x1250 - 0x0280:0x1250 - 0x0280 + 0x140] = bytes(0x140)
open(out, "wb").write(bytes(blk))
print("%d checked blocks ok -> %s (%d bytes)" % (valid, out, 0x8ED0 - 0x0280))
