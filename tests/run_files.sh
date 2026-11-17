#!/bin/bash
# v0.81 (review finding 5 / E, owner: "Go 12" (all three)): trainer.txt, watts.txt, settings.txt and a TWiLight Menu
# .sav saved with a stop (power cut) at every file step - tests/test_files.c.
# tests/run_files.sh --old DIR: the same test built on an older source tree DIR (its old code; expected to FAIL).
set -e
cd "$(dirname "$0")/.."
SRC=.
DEFS=
EXTRA=source/safe_file.c
if [ "$1" = "--old" ]; then
  SRC="$2"
  DEFS=-DOLD_CODE
  EXTRA=
fi
WRAP="-Wl,--wrap=fopen,--wrap=fclose,--wrap=fwrite,--wrap=fprintf,--wrap=remove,--wrap=rename"
OUT=$(mktemp -d /tmp/pocketpkmn_files.XXXXXX)
# -fno-builtin and no _FORTIFY_SOURCE: every fprintf / fwrite really goes through the stop wrappers
gcc -std=c11 -D_DEFAULT_SOURCE $DEFS -U_FORTIFY_SOURCE -fno-builtin -Wall -Wextra -Werror -O1 -g \
    -fsanitize=address,undefined -I"$SRC/include" "$SRC/source/wbx_trainer.c" $EXTRA tests/test_files.c $WRAP \
    -o "$OUT/test_files"
r=0
"$OUT/test_files" || r=$?
rm -rf "$OUT"
exit $r
