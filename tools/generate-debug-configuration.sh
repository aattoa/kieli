#!/bin/sh

[ "$#" = "1" ] && cmake -S "$1" -B "$1/out/debug" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Og"

