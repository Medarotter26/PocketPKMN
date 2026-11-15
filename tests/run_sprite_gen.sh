#!/bin/bash
# wbx_sprite_load_for (Gen 1/2 set for Gen 1/2 games) against romfs/sprites/gen12 and the romfs pack (romfs/sprites/pack).
set -e
REAL="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$REAL" /tmp/wbx-src
HERE=/tmp/wbx-src
gcc -std=c99 -O1 -Wall -Wextra -Werror -I$HERE/include \
    -DWBX_SPRITE_PC_DIR="\"$HERE/romfs/sprites/pack\"" -DWBX_SPRITE12_PC_DIR="\"$HERE/romfs/sprites/gen12\"" \
    $HERE/source/pw_image.c $HERE/source/pw_font.c $HERE/source/wbx_sprites.c $HERE/tests/test_sprite_gen.c \
    -o /tmp/pocketpkmn_test_sprite_gen
/tmp/pocketpkmn_test_sprite_gen
