#!/bin/bash
# v0.53: every walk catch the app can make, checked by PKHeX's own legality analysis (user: "Legality is very
# important"). Builds tests/test_catches.cpp, makes 4 catches per course Pokemon and game version from
# romfs/routes/catch_*.bin for the trainers of the saves in hw/, then runs ~/pkhex-check (PKHeX.Core, see
# tools/pkhex_check_Program.cs) on all of them. Every catch must be legal.
set -e
if [ ! -d "$(cd "$(dirname "$0")/.." && pwd)/hw" ]; then
  echo "skipped: needs your own saves in hw/ and the PKHeX checker in ~/pkhex-check (see README)"
  exit 0
fi
REAL="$(cd "$(dirname "$0")/.." && pwd)"
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
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include \
    "$HERE/tests/test_catches.cpp" "$HERE/source/walk_catch.cpp" "${objs[@]}" -o "$OUT/test_catches"
C=$HOME/wbx-catches
rm -rf "$C" && mkdir -p "$C/xy" "$C/gsc" "$C/sm"
"$OUT/test_catches" "$HERE/romfs/routes/catch_xy.bin" "$C/xy" \
    "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" "$HERE/hw/gen6/0004000000055d00-20261118-204520.main"
"$OUT/test_catches" "$HERE/romfs/routes/catch_gsc.bin" "$C/gsc" \
    "$HERE/hw/gen2/0004000000172700-20261118-185538.sav" "$HERE/hw/gen2/0004000000172700-20261118-185538.sav" \
    "$HERE/hw/gen2/0004000000172800-20261118-190237.sav"
export DOTNET_ROOT=$HOME/.dotnet DOTNET_CLI_TELEMETRY_OPTOUT=1
CHK=~/pkhex-check/bin/Release/net10.0/pkhex-check
# v0.55 Sun / Moon / Ultra: no Gen 7 save in hw/ yet, so PKHeX makes blank saves with a test trainer (blank7)
B=$HOME/wbx-blank7
mkdir -p "$B" "$C/sm"
for v in SN MN US UM; do "$CHK" blank7 "$B/$v.main" $v > /dev/null; done
"$OUT/test_catches" "$HERE/romfs/routes/catch_sm.bin" "$C/sm" "$B/SN.main" "$B/MN.main" "$B/US.main" "$B/UM.main"
set +e
"$CHK" pk 6 "$C"/xy/*.pk6 > "$C/xy.txt"; r6=$?
"$CHK" pk 2 "$C"/gsc/*.pk2 > "$C/gsc.txt"; r2=$?
"$CHK" pk 7 "$C"/sm/*.pk7 > "$C/sm.txt"; r7=$?
echo "X/Y: $(grep -c '^OK' "$C/xy.txt") legal, $(grep -c '^ILLEGAL' "$C/xy.txt") illegal"
echo "GSC: $(grep -c '^OK' "$C/gsc.txt") legal, $(grep -c '^ILLEGAL' "$C/gsc.txt") illegal"
echo "SM/USUM: $(grep -c '^OK' "$C/sm.txt") legal, $(grep -c '^ILLEGAL' "$C/sm.txt") illegal"
grep -A4 '^ILLEGAL' "$C/xy.txt" "$C/gsc.txt" "$C/sm.txt" | head -60
if [ $r6 -ne 0 ] || [ $r2 -ne 0 ] || [ $r7 -ne 0 ]; then echo "CATCH TESTS FAILED"; exit 1; fi
set -e
echo "catch tests passed"

# v0.53 items: into copies of the saves in hw/ (Gen 2 PC, X/Y bag), then opened by PKHeX
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include \
    "$HERE/tests/test_items.cpp" "$HERE/source/gamematch.cpp" "${objs[@]}" -o "$OUT/test_items"
I=$HOME/wbx-items
rm -rf "$I" && mkdir -p "$I"
"$OUT/test_items" "$I" "$HERE/hw/gen2/0004000000172800-20261118-190237.sav" \
    "$HERE/hw/gen2/0004000000172700-20261118-185538.sav" "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" \
    "$HERE/hw/gen7/moon_before_160900.main"
# PKHeX must judge each changed save as it judges the untouched one (the user's Gold/Silver files already fail
# PKHeX's backup-copy check: that stays as it was; Crystal and X/Y must stay valid)
O=$HOME/wbx-items-orig
rm -rf "$O" && mkdir -p "$O"
for f in "$I"/*; do cp "$HERE"/hw/*/"$(basename "$f")" "$O/" 2>/dev/null; done
set +e
"$CHK" items "$O"/* | grep -v '^    ' | awk '{print $1, $2}' | sort > "$I.orig.txt"
"$CHK" items "$I"/* | tee "$I.txt" | grep -v '^    ' | awk '{print $1, $2}' | sort > "$I.new.txt"
set -e
cat "$I.txt"
if ! diff "$I.orig.txt" "$I.new.txt"; then echo "ITEM SAVES: PKHeX judges a changed save differently"; exit 1; fi
echo "item tests passed (PKHeX: each save as valid as before)"
