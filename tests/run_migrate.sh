#!/bin/bash
# app_migrate (v0.33 rename WalkerBox -> PocketPKMN) on temp folders.
set -e
cd "$(dirname "$0")/.."
gcc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Iinclude source/app_migrate.c tests/test_migrate.c -o /tmp/pocketpkmn_test_migrate
/tmp/pocketpkmn_test_migrate
