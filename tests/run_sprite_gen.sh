#!/bin/bash
# wbx_sprite_load_for (Gen 1/2 + Gen 3 sets, v0.90 Gen 5-7 forms + female) against romfs/sprites/gen12, gen3, forms and the pack.
set -e
REAL="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$REAL" /tmp/wbx-src
HERE=/tmp/wbx-src
gcc -std=c99 -O1 -Wall -Wextra -Werror -I$HERE/include \
    -DWBX_SPRITE_PC_DIR="\"$HERE/romfs/sprites/pack\"" -DWBX_SPRITE12_PC_DIR="\"$HERE/romfs/sprites/gen12\"" \
    -DWBX_SPRITE3_PC_DIR="\"$HERE/romfs/sprites/gen3\"" -DWBX_SPRITE_FORMS_PC_DIR="\"$HERE/romfs/sprites/forms\"" \
    $HERE/source/pw_image.c $HERE/source/pw_font.c $HERE/source/wbx_sprites.c $HERE/tests/test_sprite_gen.c \
    -o /tmp/pocketpkmn_test_sprite_gen
/tmp/pocketpkmn_test_sprite_gen
