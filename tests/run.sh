#!/bin/bash
# Build and run the PC-side protocol tests (inside WSL). Output goes to /tmp.
set -e
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -Iinclude source/pw_proto.c tests/test_proto.c -o /tmp/walkerbox_test_proto
/tmp/walkerbox_test_proto
