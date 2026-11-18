#!/bin/bash
# consolePlain (include/console_text.hpp, v0.92): the console screens print accented names as plain ASCII.
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude tests/test_console_text.cpp \
    -o /tmp/pocketpkmn_test_console_text
/tmp/pocketpkmn_test_console_text
