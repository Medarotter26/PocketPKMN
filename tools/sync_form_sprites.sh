#!/bin/bash
# sync_form_sprites.sh - copies the Alolan form walker sprites (art_src/alola/out/NNN_alola.bin, 0x780 bytes each)
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
[ "$1" = "--check" ] && { [ $bad = 0 ] && echo "romfs forms = $SRC"; exit $bad; }
echo "copied $(ls "$DST"/*_f1.bin | wc -l) form sprites to $DST"
