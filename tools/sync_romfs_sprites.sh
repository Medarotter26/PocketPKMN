#!/bin/bash
# sync_romfs_sprites.sh - copies the walker sprite pack made by tools/walker_assets.py (sd_out/3ds/walkerbox/sprites,
# 807 files NNN.bin x 0x780 bytes) byte-identical into the app's romfs (romfs/sprites/pack), which ships inside the
# .3dsx and the .cia (user decision 2026-11-15). Run after walker_assets.py changes the pack; then delete
# pocketpkmn.3dsx / .cia before building (make does not notice romfs changes).
# Usage: bash tools/sync_romfs_sprites.sh          (copy)
#        bash tools/sync_romfs_sprites.sh --check  (only compare; exit 1 if they differ)
set -e
cd "$(dirname "$0")/.."
SRC=sd_out/3ds/walkerbox/sprites
DST=romfs/sprites/pack
n=$(ls "$SRC"/*.bin | wc -l)
if [ "$n" != 807 ]; then echo "expected 807 files in $SRC, found $n"; exit 1; fi
if [ "$1" = "--check" ]; then
    diff -rq "$SRC" "$DST" && echo "romfs pack = sd_out pack ($n files)"
    exit $?
fi
mkdir -p "$DST"
rm -f "$DST"/*.bin
cp "$SRC"/*.bin "$DST"/
diff -rq "$SRC" "$DST"
echo "copied $n files to $DST"
