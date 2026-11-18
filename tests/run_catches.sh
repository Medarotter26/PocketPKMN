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
# v0.93 Omega Ruby / Alpha Sapphire: the trainer of the user's Alpha Sapphire save for both games (no OR save in hw/)
mkdir -p "$C/oras"
AS6="$HERE/hw/gen6/000400000011c500-20261118-205138.main"
"$OUT/test_catches" "$HERE/romfs/routes/catch_oras.bin" "$C/oras" "$AS6" "$AS6"
# v0.94 FireRed / LeafGreen: the owner has no FRLG save ("could you make one and test it all on your end?") -> PKHeX
# makes them (pkhex-check blank3: a test trainer, N badges, the Hall of Fame flag, legal box Pokemon): FR with 3
# badges, LeafGreen with 8 + Hall of Fame. Catches for FR with the FR save's trainer, for LG with the LG one.
G3=$HOME/wbx-frlg
mkdir -p "$G3" "$C/frlg"
"$CHK" blank3 "$G3/FR.sav" FR 3 0 16 19 21 > /dev/null
"$CHK" blank3 "$G3/LG_hof.sav" LG 8 1 16 > /dev/null
"$OUT/test_catches" "$HERE/romfs/routes/catch_frlg.bin" "$C/frlg" "$G3/FR.sav" "$G3/LG_hof.sav"
# v0.97 Ruby / Sapphire / Emerald: PKHeX-made saves too (R 3 badges, S 8 + Hall of Fame, E 5; E_hof for the save test)
mkdir -p "$C/rse"
"$CHK" blank3 "$G3/R.sav" R 3 0 263 261 > /dev/null
"$CHK" blank3 "$G3/S_hof.sav" S 8 1 263 > /dev/null
"$CHK" blank3 "$G3/E.sav" E 5 0 263 261 > /dev/null
"$CHK" blank3 "$G3/E_hof.sav" E 8 1 263 > /dev/null
"$OUT/test_catches" "$HERE/romfs/routes/catch_rse.bin" "$C/rse" "$G3/R.sav" "$G3/S_hof.sav" "$G3/E.sav"
set +e
"$CHK" pk 3 "$C"/frlg/*.pk3 > "$C/frlg.txt"; rf=$?
"$CHK" pk 3 "$C"/rse/*.pk3 > "$C/rse.txt"; rr=$? # v0.97
echo "RSE: $(grep -c '^OK' "$C/rse.txt") legal, $(grep -c '^ILLEGAL' "$C/rse.txt") illegal"
grep -A4 '^ILLEGAL' "$C/rse.txt" | head -30
echo "FRLG: $(grep -c '^OK' "$C/frlg.txt") legal, $(grep -c '^ILLEGAL' "$C/frlg.txt") illegal"
grep -A4 '^ILLEGAL' "$C/frlg.txt" | head -30
"$CHK" pk 6 "$C"/oras/*.pk6 > "$C/oras.txt"; ro=$?
echo "ORAS: $(grep -c '^OK' "$C/oras.txt") legal, $(grep -c '^ILLEGAL' "$C/oras.txt") illegal"
grep -A4 '^ILLEGAL' "$C/oras.txt" | head -30
"$CHK" pk 6 "$C"/xy/*.pk6 > "$C/xy.txt"; r6=$?
[ $ro -ne 0 ] && r6=1
[ $rf -ne 0 ] && r6=1 # v0.94
[ $rr -ne 0 ] && r6=1 # v0.97
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
ctx "Alpha Sapphire" 6 "$AS6" "$C"/oras/v1_*.pk6 # v0.93
ctx "FireRed" 3 "$G3/FR.sav" "$C"/frlg/v0_*.pk3 # v0.94
# v0.96 (the owner's first real FireRed catch, a 3-letter OT, had "ER" of the template's "PLAYER" left in its OT field):
# every game's catches again with a 2- and a 3-letter trainer name (CATCH_OT, tests/test_catches.cpp)
for ot in Al Kai; do
  for spec in "frlg 3 $G3/FR.sav $G3/LG_hof.sav" "rse 3 $G3/R.sav $G3/S_hof.sav $G3/E.sav" "dppt 4 $P4 $P4 $P4" \
              "xy 6 $HERE/hw/gen6/0004000000055d00-20261118-204520.main $HERE/hw/gen6/0004000000055d00-20261118-204520.main" \
              "oras 6 $AS6 $AS6" "sm 7 $B/SN.main $B/MN.main $B/US.main $B/UM.main" \
              "gsc 2 $HERE/hw/gen2/0004000000172700-20261118-185538.sav $HERE/hw/gen2/0004000000172700-20261118-185538.sav $HERE/hw/gen2/0004000000172800-20261118-190237.sav"; do
    set -- $spec; f=$1; g=$2; shift 2
    rm -rf "$C/ot-$ot-$f" && mkdir -p "$C/ot-$ot-$f"
    CATCH_OT=$ot "$OUT/test_catches" "$HERE/romfs/routes/catch_$f.bin" "$C/ot-$ot-$f" "$@" > /dev/null
    "$CHK" pk $g "$C/ot-$ot-$f"/*.pk$g > "$C/ot.txt"; r=$?
    echo "trainer name \"$ot\", $f: $(grep -c '^OK' "$C/ot.txt") legal, $(grep -c '^ILLEGAL' "$C/ot.txt") illegal"
    grep -A3 '^ILLEGAL' "$C/ot.txt" | head -6
    [ $r -ne 0 ] && fail=1
  done
done
ctx "LeafGreen" 3 "$G3/LG_hof.sav" "$C"/frlg/v1_*.pk3
ctx "Ruby" 3 "$G3/R.sav" "$C"/rse/v0_*.pk3 # v0.97
ctx "Sapphire" 3 "$G3/S_hof.sav" "$C"/rse/v1_*.pk3
ctx "Emerald" 3 "$G3/E.sav" "$C"/rse/v2_*.pk3
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
  for g in xy:6:pk6:0 sm:7:pk7:0 dppt:4:pk4:2 oras:6:pk6:1; do # v0.93: + ORAS (Alpha Sapphire trainer)
    IFS=: read file gen ext vi <<< "$g"
    [ $lang -ge 9 ] && [ $file != sm ] && continue # no Chinese X / Y or Platinum
    sv="$L/$file-$lang.sav"
    case $file in
      xy) "$CHK" relang "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" "$sv" $lang ;;
      oras) "$CHK" relang "$AS6" "$sv" $lang ;;
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
    "$HERE/hw/gen7/moon_before_160900.main" "$HERE/hw/gen4/pt_before_190651.sav" "$HERE/hw/gen4/d_before_190400.sav" \
    "$AS6" "$G3/FR.sav" "$G3/LG_hof.sav" "$G3/R.sav" "$G3/E.sav" # v0.93: + Alpha Sapphire; v0.94: + FR / LG; v0.97: + R / E (PC items)
# PKHeX must judge each changed save as it judges the untouched one (the user's Gold/Silver files already fail
# PKHeX's backup-copy check: that stays as it was; Crystal and X/Y must stay valid)
O=$HOME/wbx-items-orig
rm -rf "$O" && mkdir -p "$O"
for f in "$I"/*; do cp "$HERE"/hw/*/"$(basename "$f")" "$O/" 2>/dev/null || true; done # v0.94: FRLG saves are not in hw/
cp "$G3/FR.sav" "$G3/LG_hof.sav" "$G3/R.sav" "$G3/E.sav" "$O/" # v0.94: the PKHeX-made FRLG saves (v0.97: + R, E)
set +e
"$CHK" items "$O"/* | grep -v '^    ' | awk '{print $1, $2}' | sort > "$I.orig.txt"
"$CHK" items "$I"/* | tee "$I.txt" | grep -v '^    ' | awk '{print $1, $2}' | sort > "$I.new.txt"
set -e
cat "$I.txt"
if ! diff "$I.orig.txt" "$I.new.txt"; then echo "ITEM SAVES: PKHeX judges a changed save differently"; exit 1; fi
echo "item tests passed (PKHeX: each save as valid as before)"

# v0.94 FireRed / LeafGreen saves (PKHeX-made, see above): PKSM-Core reads them as FRLG, badges and the Hall of Fame
# flag the app reads; a box move saved the game's way reads back; PKHeX then opens the moved saves (valid checksums).
# (+ walk gains: walk_poke.cpp and the C parts it uses, built as tests/run_walk.sh builds them)
PACK="\"$HERE/romfs/sprites/pack\""
cobjs=()
for c in pw_image pw_font pw_proto wbx_sprites walk_compose; do
  gcc -std=c99 -O1 -Wall -Wextra -Werror -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" -c $HERE/source/$c.c -o "$OUT/obj/frlg_$c.o"
  cobjs+=("$OUT/obj/frlg_$c.o")
done
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include -DWBX_SPRITE_PC_DIR="$PACK" \
    "$HERE/tests/test_frlg.cpp" "$HERE/source/gamematch.cpp" "$HERE/source/walk_poke.cpp" "$HERE/source/gbavc.cpp" \
    "${cobjs[@]}" "${objs[@]}" -o "$OUT/test_frlg"
M=$HOME/wbx-frlg-moved
rm -rf "$M" && mkdir -p "$M"
"$OUT/test_frlg" "$M" "$G3/FR.sav" 3 0 "$G3/LG_hof.sav" 8 1 \
    "$G3/R.sav" 3 0 "$G3/S_hof.sav" 8 1 "$G3/E.sav" 5 0 "$G3/E_hof.sav" 8 1 # v0.97: + Ruby / Sapphire / Emerald
"$CHK" items "$M"/moved_* | grep -v '^    '
if "$CHK" items "$M"/moved_* | grep -v '^    ' | grep -q BADSUM; then echo "FRLG: PKHeX finds a bad checksum"; exit 1; fi
if ! "$CHK" pk 3 "$M"/walked_*.pk3; then echo "FRLG: a walked Pokemon is not legal"; exit 1; fi
# v0.95 (owner: "let's go ahead and do"): EV training walks give Gen 3/4 EVs only as far as the EXP explains -> legal too
if ! "$CHK" pk 3 "$M"/evwalk_*.pk3; then echo "FRLG: an EV-trained walked Pokemon is not legal"; exit 1; fi
echo "FRLG save tests passed (PKHeX: moved saves valid, walked Pokemon legal with and without EV training)"
