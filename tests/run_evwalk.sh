#!/bin/bash
# 2026-11-17 (owner: "I want you to thoroughly check the evs"): walk EV training on every box Pokemon of the saves in
# hw/ (+ PKHeX-made FireRed saves), judged by PKHeX before / after (tests/test_evwalk.cpp, tests/evwalk_report.py).
# Not part of the regular test run (a few minutes). Needs run_catches.sh to have run once (~/wbx-frlg, objects).
export DOTNET_ROOT=$HOME/.dotnet
A="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$A" /tmp/wbx-src
HERE=/tmp/wbx-src; PK="$HERE/lib/PKSM-Core"; OUT="$HOME/wbx-pksm"
DEFS="-D_PKSMCORE_CONFIGURED -D_PKSMCORE_LANG_FOLDER=\"$HERE/romfs/i18n/\" -D_PKSMCORE_PERSONAL_FOLDER=\"$HERE/romfs/personal/\""
INC="-I$PK/include -I$PK/include/enums -I$PK/include/personal -I$PK/include/pkx -I$PK/include/sav -I$PK/include/utils -I$PK/include/wcx -I$PK/memecrypto -I$PK/pcg-cpp/include"
g++ -std=gnu++20 -O1 -w $DEFS $INC -I$HERE/include "$HERE/tests/test_evwalk.cpp" "$HERE/source/walk_poke.cpp" "$HERE/source/gbavc.cpp" \
    $OUT/obj/frlg_*.o $(ls $OUT/obj/*.o | grep -v "/wbx_\|/frlg_") -o $OUT/test_evwalk
E=$HOME/wbx-evwalk; rm -rf $E; mkdir -p $E
~/pkhex-check/bin/Release/net10.0/pkhex-check blank3 ~/wbx-frlg/FR_many.sav FR 8 1 10 16 19 21 23 27 29 32 41 43 46 48 50 52 54 56 58 60 63 66 69 72 74 77 79 81 84 86 88 90 92 95 96 98 100 102 > /dev/null
$OUT/test_evwalk $E "$HERE/hw/gen3/0004000003ea7a00-20261118-202244.gbavc" ~/wbx-frlg/FR.sav ~/wbx-frlg/FR_many.sav \
    "$HERE/hw/gen4/platinum_pksm_2026-7-24.sav" "$HERE/hw/gen4/d_before_190400.sav" \
    "$HERE/hw/gen6/0004000000055d00-20261118-204520.main" "$HERE/hw/gen6/000400000011c500-20261118-205138.main" \
    "$HERE/hw/gen7/moon_before_160900.main" "$HERE/hw/gen7/usun_20261121.main"
export DOTNET_ROOT=$HOME/.dotnet
CHK=~/pkhex-check/bin/Release/net10.0/pkhex-check
set +e
for g in 3 4 6 7; do ls $E/*.pk$g >/dev/null 2>&1 && "$CHK" pk $g $E/*.pk$g > $E.g$g.txt; done
python3 -I $HERE/tests/evwalk_report.py $E
