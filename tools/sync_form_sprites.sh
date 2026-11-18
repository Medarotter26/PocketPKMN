#!/bin/bash
# sync_form_sprites.sh - copies the Alolan form walker sprites (and, at the end, the Gen 6/7 forms) (art_src/alola/out/NNN_alola.bin, 0x780 bytes each)
# into the app's romfs as romfs/sprites/forms/NNN_f1.bin (Alolan = form 1 in Gen 7), where wbx_sprite_load_for looks
# for form sprites. Delete pocketpkmn.3dsx / .cia before building afterwards (make does not notice romfs changes).
# Usage: bash tools/sync_form_sprites.sh          (copy)
#        bash tools/sync_form_sprites.sh --check  (only compare; exit 1 if they differ)
set -e
cd "$(dirname "$0")/.."
SRC=art_src/alola/out
DST=romfs/sprites/forms
bad=0
for f in "$SRC"/*_alola.bin; do
    n=$(basename "$f" _alola.bin)
    size=$(stat -c %s "$f")
    if [ "$size" != 1920 ]; then echo "wrong size ($size): $f"; exit 1; fi
    if [ "$1" = "--check" ]; then
        cmp -s "$f" "$DST/${n}_f1.bin" || { echo "differs: ${n}_f1.bin"; bad=1; }
    else
        mkdir -p "$DST"
        cp "$f" "$DST/${n}_f1.bin"
    fi
done
# v0.90 (2026-11-17): the Gen 6/7 forms + Pyroar female of the sprite session (art_src/forms67/out, tool
# tools/walker_forms67.py), files only, same names (the games' form numbers, PKSM-Core i18n_form.cpp), except:
# 668_f.bin (Pyroar female) -> 668_female.bin, and 718_f1.bin (Zygarde 10%) also as 718_f2.bin (10% Power Construct).
SRC67=art_src/forms67/out
n67=0
one67() { # one67 <source file> <destination name>
    local size
    size=$(stat -c %s "$1")
    if [ "$size" != 1920 ]; then echo "wrong size ($size): $1"; exit 1; fi
    if [ "$MODE" = "--check" ]; then
        cmp -s "$1" "$DST/$2" || { echo "differs: $2"; bad=1; }
    else
        cp "$1" "$DST/$2"
    fi
    n67=$((n67 + 1))
}
MODE="$1"
for f in "$SRC67"/[0-9][0-9][0-9]_f[0-9]*.bin; do one67 "$f" "$(basename "$f")"; done
one67 "$SRC67/718_f1.bin" 718_f2.bin
one67 "$SRC67/668_f.bin" 668_female.bin
if [ "$n67" != 72 ]; then echo "expected 72 Gen 6/7 form files, found $n67"; exit 1; fi
[ "$1" = "--check" ] && { [ $bad = 0 ] && echo "romfs forms = $SRC + $SRC67 ($n67 files)"; exit $bad; }
echo "copied the Alolan form sprites and $n67 Gen 6/7 files to $DST"
