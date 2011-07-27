#!/bin/bash
# Usage: CC=gcc ./build.sh | CC=clang ./build.sh etc, etc.

$CC -Wall -Wextra -Werror -pipe -pedantic -std=c99 -DFORTIFY_SOURCE=2 -O2 -c arena.c
