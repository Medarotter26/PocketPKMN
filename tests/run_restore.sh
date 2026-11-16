#!/bin/bash
# v0.68 (review addendum finding B): the Walker -> Restore plan against every walker backup in hw/.
set -e
cd "$(dirname "$0")/.."
gcc -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
    -Iinclude source/restore_plan.c tests/test_restore.c -o /tmp/pocketpkmn_test_restore
/tmp/pocketpkmn_test_restore $(find hw -name 'eeprom_*.bin' -size 65536c | sort)
