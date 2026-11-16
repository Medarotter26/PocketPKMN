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
# v0.61 Diamond / Pearl / Platinum: the trainer of the user's Platinum save (a PKSM backup) for all three games
mkdir -p "$C/dppt"
P4="$HERE/hw/gen4/platinum_pksm_2026-7-24.sav"
"$OUT/test_catches" "$HERE/romfs/routes/catch_dppt.bin" "$C/dppt" "$P4" "$P4" "$P4"
set +e
"$CHK" pk 6 "$C"/xy/*.pk6 > "$C/xy.txt"; r6=$?
"$CHK" pk 2 "$C"/gsc/*.pk2 > "$C/gsc.txt"; r2=$?
"$CHK" pk 7 "$C"/sm/*.pk7 > "$C/sm.txt"; r7=$?
"$CHK" pk 4 "$C"/dppt/*.pk4 > "$C/dppt.txt"; r4=$?
echo "DPPt: $(grep -c '^OK' "$C/dppt.txt") legal, $(grep -c '^ILLEGAL' "$C/dppt.txt") illegal"
grep -A4 '^ILLEGAL' "$C/dppt.txt" | head -30
[ $r4 -ne 0 ] && r7=1
echo "X/Y: $(grep -c '^OK' "$C/xy.txt") legal, $(grep -c '^ILLEGAL' "$C/xy.txt") illegal"
echo "GSC: $(grep -c '^OK' "$C/gsc.txt") legal, $(grep -c '^ILLEGAL' "$C/gsc.txt") illegal"
echo "SM/USUM: $(grep -c '^OK' "$C/sm.txt") legal, $(grep -c '^ILLEGAL' "$C/sm.txt") illegal"
grep -A4 '^ILLEGAL' "$C/xy.txt" "$C/gsc.txt" "$C/sm.txt" | head -60
if [ $r6 -ne 0 ] || [ $r2 -ne 0 ] || [ $r7 -ne 0 ]; then echo "CATCH TESTS FAILED"; exit 1; fi

# v0.64 (review 2026-11, finding 2): the same catches judged WITH THE SAVE LOADED, as PKHeX's own window does - a
# catch from another game than the player's must show the player as its handler. Each version's catches with that
# version's trainer (X: the X save; Sun / Moon / Ultra: the blank test saves; Moon also for the real Moon save's
# trainer; Platinum: the Platinum save; Diamond: the Diamond save; Silver / Crystal: their saves).
fail=0
ctx() { # <label> <gen> <save or version> <files...>
  local label="$1"; shift
  "$CHK" pkctx "$@" > "$C/ctx.txt"; local r=$?
  echo "with the save loaded, $label: $(grep -c '^OK' "$C/ctx.txt") legal, $(grep -c '^ILLEGAL' "$C/ctx.txt") illegal"
  grep -A4 '^ILLEGAL' "$C/ctx.txt" | head -8
  [ $r -ne 0 ] && fail=1
}
ctx "X" 6 "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" "$C"/xy/v0_*.pk6
i=0; for v in SN MN US UM; do ctx "$v" 7 $v "$C"/sm/v${i}_*.pk7; i=$((i+1)); done
MOON="$HERE/hw/gen7/moon_before_160900.main"
mkdir -p "$C/moon" && "$OUT/test_catches" "$HERE/romfs/routes/catch_sm.bin" "$C/moon" "$MOON" "$MOON" "$MOON" "$MOON" > /dev/null
ctx "Moon (real save)" 7 "$MOON" "$C"/moon/v1_*.pk7
ctx "Platinum" 4 "$P4" "$C"/dppt/v2_*.pk4
D4="$HERE/hw/gen4/d_before_190400.sav"
mkdir -p "$C/dia" && "$OUT/test_catches" "$HERE/romfs/routes/catch_dppt.bin" "$C/dia" "$D4" "$D4" "$D4" > /dev/null
ctx "Diamond" 4 "$D4" "$C"/dia/v0_*.pk4
ctx "Silver" 2 "$HERE/hw/gen2/0004000000172700-20261118-185538.sav" "$C"/gsc/v1_*.pk2
ctx "Crystal" 2 "$HERE/hw/gen2/0004000000172800-20261118-190237.sav" "$C"/gsc/v2_*.pk2

# v0.64 (review 2026-11, finding 1): games in other languages - the catch must carry the species name of the game's
# own language (romfs i18n/<lang>/species*.txt, made by "pkhex-check names"). Trainers: copies of the X and Platinum
# saves and blank Sun saves with the language changed (LanguageID: 1 JPN 3 FRE 4 ITA 5 GER 7 SPA 8 KOR 9 CHS 10 CHT).
L=$HOME/wbx-lang
rm -rf "$L" && mkdir -p "$L"
for lang in 1 3 4 5 7 8 9 10; do
  for g in xy:6:pk6:0 sm:7:pk7:0 dppt:4:pk4:2; do
    IFS=: read file gen ext vi <<< "$g"
    [ $lang -ge 9 ] && [ $file != sm ] && continue # no Chinese X / Y or Platinum
    sv="$L/$file-$lang.sav"
    case $file in
      xy) "$CHK" relang "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" "$sv" $lang ;;
      sm) "$CHK" blank7 "$sv" SN $lang > /dev/null ;;
      dppt) "$CHK" relang "$P4" "$sv" $lang ;;
    esac
    mkdir -p "$L/$file-$lang"
    "$OUT/test_catches" "$HERE/romfs/routes/catch_$file.bin" "$L/$file-$lang" "$sv" "$sv" "$sv" "$sv" > /dev/null
    "$CHK" pk $gen "$L/$file-$lang"/v${vi}_*.$ext > "$L/$file-$lang.txt"; r=$?
    echo "language $lang, $file: $(grep -c '^OK' "$L/$file-$lang.txt") legal, $(grep -c '^ILLEGAL' "$L/$file-$lang.txt") illegal"
    grep -A4 '^ILLEGAL' "$L/$file-$lang.txt" | head -6
    [ $r -ne 0 ] && fail=1
  done
done
if [ $fail -ne 0 ]; then echo "CATCH TESTS FAILED (save loaded / other languages)"; exit 1; fi
set -e
echo "catch tests passed"

# v0.53 items: into copies of the saves in hw/ (Gen 2 PC, X/Y bag), then opened by PKHeX
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include \
    "$HERE/tests/test_items.cpp" "$HERE/source/gamematch.cpp" "${objs[@]}" -o "$OUT/test_items"
I=$HOME/wbx-items
rm -rf "$I" && mkdir -p "$I"
"$OUT/test_items" "$I" "$HERE/hw/gen2/0004000000172800-20261118-190237.sav" \
    "$HERE/hw/gen2/0004000000172700-20261118-185538.sav" "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" \
    "$HERE/hw/gen7/moon_before_160900.main" "$HERE/hw/gen4/pt_before_190651.sav" "$HERE/hw/gen4/d_before_190400.sav"
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
