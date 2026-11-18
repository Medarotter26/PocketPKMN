#!/bin/bash
# PC test of review finding 6 (v0.86): the "move in progress" mark (tests/test_moves.c - a model of storage.cpp's
# order of writes with a power cut before every step). Usage: bash tests/run_moves.sh
set -e
cd "$(dirname "$0")/.."
gcc -std=c99 -O1 -g -Wall -Wextra -Werror -Wno-parentheses -fsanitize=address,undefined -fno-sanitize-recover=all \
    tests/test_moves.c -o /tmp/pocketpkmn_test_moves
/tmp/pocketpkmn_test_moves
