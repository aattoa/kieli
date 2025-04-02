# Kieli

A general-purpose programming language.

This is a work in progress, and can not be used for anything yet.

Some subdirectories under `src/` have individual READMEs.

## Dependencies

- [github.com/aattoa/cpputil](https://github.com/aattoa/cpputil)
- [github.com/aattoa/cppunittest](https://github.com/aattoa/cppunittest)

Dependencies are automatically fetched by CMake.

## Build

Prerequisites:

- CMake
- A build system, such as GNU Make or Ninja
- A C++23 compiler toolchain

NOTE: At the time of writing, LLVM`>=18` is the only toolchain that supports C++23 well enough to build Kieli.

Run the following commands to clone and build Kieli:

```sh
git clone https://github.com/aattoa/kieli
cd kieli
cmake -B build
cmake --build build --parallel 8
```

## Tests

Tests are enabled by default, but can be disabled with the CMake option `KIELI_BUILD_TESTS`, i.e. configure with `-DKIELI_BUILD_TESTS=OFF`.

To run all tests associated with a build, build the `test` target with your build system of choice, or use CMake abstraction: `cmake --build build --target test`
