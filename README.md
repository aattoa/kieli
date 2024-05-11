# Kieli

A general-purpose programming language.

This is a work in progress, and can not be used for anything yet.

## Dependencies

- [github.com/aattoa/cppargs](https://github.com/aattoa/cppargs)
- [github.com/aattoa/cppdiag](https://github.com/aattoa/cppdiag)
- [github.com/aattoa/cpputil](https://github.com/aattoa/cpputil)
- [github.com/aattoa/cppunittest](https://github.com/aattoa/cppunittest)

Dependencies are automatically fetched by CMake.

## Build

Prerequisites:

- CMake
- A build system, such as GNU Make or Ninja
- A C++23 compiler toolchain

At the time of writing, LLVM (18 and later) is the only toolchain that supports C++23 well enough to build Kieli.

Run the following commands to clone and build Kieli:

```sh
git clone https://github.com/aattoa/kieli
cd kieli
cmake -B build
cmake --build build --parallel 8
```

These build steps are tested with CMake `3.29.3`, Ninja `1.12.0`, and clang `19.0.0git`.

## Tests

Tests are enabled by default, but can be disabled with the CMake option `KIELI_BUILD_TESTS`, i.e. configure with `-DKIELI_BUILD_TESTS=OFF`.

To run all tests associated with a build, build the `test` target with your build system of choice, or use CMake abstraction: `cmake --build build --target test`
