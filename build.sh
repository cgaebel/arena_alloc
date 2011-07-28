#!/bin/bash
# Usage: CC=gcc ./build.sh | CC=clang ./build.sh etc, etc.

$CC -Wall -Wextra -Werror -pipe -pedantic -std=c99 -DFORTIFY_SOURCE=2 -O3 -march=native -c arena.c
clang --analyze -std=c99 -Wall -Wextra -Werror -pipe -pedantic arena.c
