#!/bin/sh

valgrind --leak-check=full --show-leak-kinds=all -s --log-file=valgrind.out --suppressions="$(dirname "$0")/valgrind_readline_suppressions" "$@"

