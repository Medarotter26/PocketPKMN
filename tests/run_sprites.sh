#!/bin/bash
# PC checks of the sprite pack: builds the pack from the user's HGSS ROM into sd_out/ (if a ROM is in the
# project root), checks the app's romfs copy (romfs/sprites/pack) is still byte-identical to it, and runs
# tests/test_sprites.c against the romfs copy - once directly, once with the romfs dir missing so every sprite
# comes from the SD fallback dir. Usage: bash tests/run_sprites.sh
set -e
# v0.50 (audit S8): these checks use the developer's own saves / walker backups (hw/), which are not in the
# public repository. Without them: skipped, not failed.
if [ ! -d "$(cd "$(dirname "$0")/.." && pwd)/hw" ]; then
  echo "skipped: needs your own saves / walker backups in hw/ (see README)"
  exit 0
fi
cd "$(dirname "$0")/.."
ROM=$(ls *.nds 2>/dev/null | head -1)
if [ -n "$ROM" ]; then
    python3 -I tools/walker_assets.py build --hgss "$ROM"
fi
# v0.80 (owner: "go ahead and fix everything"): #494-649 in the romfs pack are the Gen 5 imports of tools/sync_gen5_sprites.sh
# (521 / 592 / 593 from their _m files), not the ROM-built pack: #1-493 and #650-807 are compared with sd_out, the
# Gen 5 range with the sprite session's files (sync_gen5_sprites.sh --check).
SRC=sd_out/3ds/walkerbox/sprites
DST=romfs/sprites/pack
n=$(ls "$SRC"/*.bin | wc -l)
if [ "$n" != 807 ]; then echo "expected 807 files in $SRC, found $n"; exit 1; fi
bad=0
for i in $(seq 1 493) $(seq 650 807); do
    f=$(printf %03d.bin "$i")
    cmp -s "$SRC/$f" "$DST/$f" || { echo "differs: $DST/$f"; bad=1; }
done
[ $bad = 0 ] || exit 1
echo "romfs pack #1-493 and #650-807 = sd_out pack (644 files)"
bash tools/sync_gen5_sprites.sh --check
SRCS="source/pw_image.c source/pw_font.c source/wbx_sprites.c tests/test_sprites.c"
FLAGS="-std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Iinclude"
gcc $FLAGS $SRCS -o /tmp/walkerbox_test_sprites \
    "-DWBX_SPRITE_PC_DIR=\"$PWD/romfs/sprites/pack\""
/tmp/walkerbox_test_sprites hw/test1/eeprom_001.bin
echo "-- SD fallback (romfs dir missing):"
gcc $FLAGS $SRCS -o /tmp/walkerbox_test_sprites_fb \
    "-DWBX_SPRITE_PC_DIR=\"$PWD/romfs/sprites/no_such_dir\"" "-DWBX_SPRITE_SD_PC_DIR=\"$PWD/sd_out/3ds/walkerbox/sprites\""
/tmp/walkerbox_test_sprites_fb hw/test1/eeprom_001.bin
