#!/bin/bash
# PC test of the version guard (v0.89, docs/MOVE_RULES_PLAN.md V1): tests/test_movecheck.cpp on the owner's own saves
# in hw/ (not in the public repository: without them, skipped). Builds PKSM-Core natively like run_storage.sh (shares
# its cache ~/wbx-pksm). Like run_storage.sh / run_catches.sh it links /tmp/wbx-src: do not run it in two project
# copies at the same time. Usage: bash tests/run_movecheck.sh
set -e
REAL="$(cd "$(dirname "$0")/.." && pwd)"
if [ ! -d "$REAL/hw" ]; then
  echo "skipped: needs your own saves in hw/ (see README)"
  exit 0
fi
ln -sfn "$REAL" /tmp/wbx-src
HERE=/tmp/wbx-src
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
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_movecheck.cpp" "$HERE/source/movecheck.cpp" \
    "$HERE/source/gbavc.cpp" "${objs[@]}" -o "$OUT/test_movecheck"
"$OUT/test_movecheck" "$HERE/hw"
