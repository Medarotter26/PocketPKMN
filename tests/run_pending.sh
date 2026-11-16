#!/bin/bash
# v0.70 (review finding 4 + addendum C): the pending-walk file, stopped at every point of a delivery.
set -e
cd "$(dirname "$0")/.."
gcc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -Iinclude source/walk_pending.c tests/test_pending.c -o /tmp/pocketpkmn_test_pending
/tmp/pocketpkmn_test_pending
