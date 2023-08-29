#!/bin/sh

cmake -S . -B out/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Og"
cmake -S . -B out/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O2" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=True
