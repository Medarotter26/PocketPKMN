#!/bin/bash
# routes.c against romfs/routes (gsc.txt, xy.txt, pictures/) with the real sprite sets.
set -e
REAL="$(cd "$(dirname "$0")/.." && pwd)"
ln -sfn "$REAL" /tmp/wbx-src
HERE=/tmp/wbx-src
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -I$HERE/include \
    -DWBX_SPRITE_PC_DIR="\"$HERE/sd_out/3ds/walkerbox/sprites\"" -DWBX_SPRITE12_PC_DIR="\"$HERE/romfs/sprites/gen12\"" \
    -DROUTES_PC_DIR="\"$HERE/romfs/routes\"" \
    $HERE/source/pw_image.c $HERE/source/pw_font.c $HERE/source/pw_proto.c $HERE/source/wbx_sprites.c \
    $HERE/source/walk_compose.c \
    $HERE/source/routes.c $HERE/tests/test_routes.c -o /tmp/pocketpkmn_test_routes
/tmp/pocketpkmn_test_routes
