#!/bin/bash
# Build and run the image tests on PC (WSL gcc). Binary goes to /tmp.
# Usage: bash tests/run_image.sh [--regen]   (--regen re-runs tools/gen_art.py first)
set -e
# v0.50 (audit S8): these checks use the developer's own saves / walker backups (hw/), which are not in the
# public repository. Without them: skipped, not failed.
if [ ! -d "$(cd "$(dirname "$0")/.." && pwd)/hw" ]; then
  echo "skipped: needs your own saves / walker backups in hw/ (see README)"
  exit 0
fi
cd "$(dirname "$0")/.."
if [ "$1" = "--regen" ]; then
    python3 tools/gen_art.py
fi
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all \
    -Iinclude source/pw_image.c source/pw_font.c source/pw_menu_art.c tests/test_image.c \
    -o /tmp/walkerbox_test_image
/tmp/walkerbox_test_image hw/test1/eeprom_001.bin hw/test3/eeprom_002.bin
