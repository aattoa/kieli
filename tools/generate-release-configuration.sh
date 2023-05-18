#!/bin/sh

[ "$#" = "1" ] && cmake -S "$1" -B "$1/out/release" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=True

