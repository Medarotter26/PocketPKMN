#!/bin/bash
# Build PKSM-Core natively (cached in /tmp/wbx-pksm) and run the storage round-trip
# test on a Gen 1 save. Usage: run_storage.sh [save.sav]
set -e
# v0.50 (audit S8): these checks use the developer's own saves / walker backups (hw/), which are not in the
# public repository. Without them: skipped, not failed.
if [ ! -d "$(cd "$(dirname "$0")/.." && pwd)/hw" ]; then
  echo "skipped: needs your own saves / walker backups in hw/ (see README)"
  exit 0
fi
REAL="$(cd "$(dirname "$0")/.." && pwd)"
# The project path has a space ("walker app"); compiler flags below are not quoted
# per path, so work through a space-free link.
ln -sfn "$REAL" /tmp/wbx-src
HERE=/tmp/wbx-src
PK="$HERE/lib/PKSM-Core"
# Cache in the WSL home: /tmp is cleared whenever WSL restarts.
OUT="$HOME/wbx-pksm"
mkdir -p "$OUT/obj"
# Default: the real saves kept in hw/ (Blue, Crystal, Silver).
if [ $# -eq 0 ]; then
  set -- "$HERE/hw/storage_s2/0004000000171100-20261118-164615.sav" \
         "$HERE/hw/gen2/0004000000172800-20261118-182625.sav" \
         "$HERE/hw/gen2/0004000000172700-20261118-182627.sav"
fi
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
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_storage.cpp" "$HERE/source/gamematch.cpp" "${objs[@]}" -o "$OUT/test_storage"
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_gbavc.cpp" "$HERE/source/gbavc.cpp" "${objs[@]}" -o "$OUT/test_gbavc"
"$OUT/test_gbavc"
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_gen3.cpp" "$HERE/source/gamematch.cpp" "$HERE/source/gbavc.cpp" "${objs[@]}" -o "$OUT/test_gen3"
for g in "$HERE"/hw/gen3/*.gbavc; do "$OUT/test_gen3" "$g"; done
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_gen6.cpp" "$HERE/source/gamematch.cpp" "${objs[@]}" -o "$OUT/test_gen6"
"$OUT/test_gen6" "$HERE"/hw/gen6/*.main
# v0.56: Gen 7 box changes against what Moon itself did (the user's Checkpoint pair), then PKHeX on the result
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_gen7.cpp" "$HERE/source/gamematch.cpp" "${objs[@]}" -o "$OUT/test_gen7"
"$OUT/test_gen7" "$HERE/hw/gen7/moon_before_160900.main" "$HERE/hw/gen7/moon_after_161029.main" "$OUT/gen7_result.main"
if [ -x ~/pkhex-check/bin/Release/net10.0/pkhex-check ]; then
  # PKHeX opens it, and the signature verifies (PKHeX's own re-signing pads differently, so no byte compare)
  DOTNET_ROOT=$HOME/.dotnet ~/pkhex-check/bin/Release/net10.0/pkhex-check memecheck "$OUT/gen7_result.main" | head -1 | cut -c1-70
  DOTNET_ROOT=$HOME/.dotnet ~/pkhex-check/bin/Release/net10.0/pkhex-check sav "$OUT/gen7_result.main" | head -1
fi
# v0.62: Gen 4 box changes against what Platinum / Diamond did (the user's Checkpoint pairs), then PKHeX on the results
g++ -std=gnu++20 -O1 -Wall -Wextra $DEFS $INC -I$HERE/include "$HERE/tests/test_gen4.cpp" "$HERE/source/gamematch.cpp" "${objs[@]}" -o "$OUT/test_gen4"
"$OUT/test_gen4" "$HERE/hw/gen4/pt_before_190651.sav" "$HERE/hw/gen4/pt_after_190903.sav" 4 1 3 1 "$OUT/gen4_pt_result.sav"
"$OUT/test_gen4" "$HERE/hw/gen4/d_before_190400.sav" "$HERE/hw/gen4/d_after_190549.sav" 1 1 18 1 "$OUT/gen4_d_result.sav"
if [ -x ~/pkhex-check/bin/Release/net10.0/pkhex-check ]; then
  DOTNET_ROOT=$HOME/.dotnet ~/pkhex-check/bin/Release/net10.0/pkhex-check slots "$HERE/hw/gen4/pt_after_190903.sav" "$OUT/gen4_pt_result.sav"
  DOTNET_ROOT=$HOME/.dotnet ~/pkhex-check/bin/Release/net10.0/pkhex-check slots "$HERE/hw/gen4/d_after_190549.sav" "$OUT/gen4_d_result.sav"
fi
"$OUT/test_storage" "$@"
