#!/bin/bash
# PC checks of the walk data (W2): test_walk.c (round trip on every walker backup in hw/, the test route)
# and test_walk_poke.cpp (every Pokemon in the saves in hw/ -> walk data). Uses the sprite pack in romfs/sprites/pack (a copy of sd_out/,
# tools/sync_romfs_sprites.sh). PKSM-Core objects are shared with run_storage.sh (~/wbx-pksm/obj).
set -e
# v0.50 (audit S8): these checks use the developer's own saves / walker backups (hw/), which are not in the
# public repository. Without them: skipped, not failed.
if [ ! -d "$(cd "$(dirname "$0")/.." && pwd)/hw" ]; then
  echo "skipped: needs your own saves / walker backups in hw/ (see README)"
  exit 0
fi
REAL="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$REAL" /tmp/wbx-src
HERE=/tmp/wbx-src
PACK="\"$HERE/romfs/sprites/pack\""
CFLAGS="-O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all"

gcc -std=c99 $CFLAGS -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" \
    $HERE/source/pw_image.c $HERE/source/pw_font.c $HERE/source/pw_proto.c $HERE/source/wbx_sprites.c \
    $HERE/source/walk_compose.c $HERE/tests/test_walk.c -o /tmp/walkerbox_test_walk
/tmp/walkerbox_test_walk $HERE/hw/test*/eeprom_*.bin

gcc -std=c99 $CFLAGS -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" \
    $HERE/source/pw_image.c $HERE/source/pw_font.c $HERE/source/pw_proto.c $HERE/source/wbx_sprites.c \
    $HERE/source/walk_compose.c $HERE/source/wbx_trainer.c $HERE/source/safe_file.c $HERE/tests/test_trainer.c \
    -o /tmp/walkerbox_test_trainer # v0.81 (review finding 5 / E, owner: "Go 12" (all three)): + safe_file.c
/tmp/walkerbox_test_trainer $HERE/hw/w3/eeprom_021.bin

PK="$HERE/lib/PKSM-Core"
OUT="$HOME/wbx-pksm"
mkdir -p "$OUT/obj"
DEFS="-D_PKSMCORE_CONFIGURED -D_PKSMCORE_LANG_FOLDER=\"$HERE/romfs/i18n/\" -D_PKSMCORE_PERSONAL_FOLDER=\"$HERE/romfs/personal/\""
INC="-I$PK/include -I$PK/include/enums -I$PK/include/personal -I$PK/include/pkx -I$PK/include/sav -I$PK/include/utils -I$PK/include/wcx -I$PK/memecrypto -I$PK/pcg-cpp/include"
objs=()
for src in $(find "$PK/source" -name '*.cpp') $(find "$PK/memecrypto" -name '*.c'); do
  o="$OUT/obj/$(basename "$src").o"
  if [ ! -f "$o" ] || [ "$src" -nt "$o" ]; then
    case "$src" in
      *.c) gcc -O1 -w $DEFS $INC -c "$src" -o "$o" ;;
      *) g++ -std=gnu++20 -O1 -w $DEFS $INC -c "$src" -o "$o" ;;
    esac
  fi
  objs+=("$o")
done
cobjs=()
for c in pw_image pw_font pw_proto wbx_sprites walk_compose; do
  gcc -std=c99 -O1 -Wall -Wextra -Werror -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" -c $HERE/source/$c.c -o "$OUT/obj/wbx_$c.o"
  cobjs+=("$OUT/obj/wbx_$c.o")
done
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" \
    "$HERE/tests/test_walk_poke.cpp" "$HERE/source/walk_poke.cpp" "$HERE/source/gbavc.cpp" "${cobjs[@]}" "${objs[@]}" \
    -o "$OUT/test_walk_poke"
"$OUT/test_walk_poke" $HERE/hw/storage_s2/0004000000171100-20261118-165312.sav \
    $HERE/hw/gen2/0004000000172800-20261118-190237.sav $HERE/hw/gen2/0004000000172700-20261118-185538.sav \
    $HERE/hw/gen3/0004000003ea7a00-20261118-202244.gbavc \
    $HERE/hw/gen4/platinum_pksm_2026-7-24.sav $HERE/hw/gen4/d_before_190400.sav \
    $HERE/hw/gen6/0004000000055d00-20261118-204520.main $HERE/hw/gen6/000400000011c500-20261118-205138.main \
    --names $HERE/romfs/i18n/ $HERE/tests/names_eng_today.txt # v0.81 (names step 1, owner: "Go 12")

# Gen 6 end to end (user 2026-11-15): out of the box -> walk gains -> back into the same slot, on every hw/gen6 save.
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" \
    "$HERE/tests/test_gen6_walk.cpp" "$HERE/source/walk_poke.cpp" "$HERE/source/gamematch.cpp" "$HERE/source/gbavc.cpp" \
    "${cobjs[@]}" "${objs[@]}" -o "$OUT/test_gen6_walk"
"$OUT/test_gen6_walk" $HERE/hw/gen6/*.main
