# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

## Dependencies

Dependencies are installed automatically with CMake's `FetchContent` functionality.

- https://github.com/aattoa/cppargs
    - Required, for the command line interface
- https://github.com/aattoa/cppdiag
    - Required, for diagnostic messages
- https://github.com/aattoa/cpputil
    - Required, for general utilities
- https://github.com/catchorg/Catch2
    - Optional, only for tests

## Building Kieli

Prerequisites:

- CMake
- A build system, such as GNU Make or Ninja
- A C++23 compliant compiler toolchain

At the time of writing, LLVM 18 is the only toolchain that supports C++23 well enough to build Kieli.

Run the following commands to clone and build Kieli:

```
git clone https://github.com/aattoa/kieli
cd kieli
cmake -S . -B build
cmake --build build -j 8
```

These build steps have been tested with the following:

- CMake `3.28.1`
- Build systems: Ninja `1.11.1`, GNU Make `4.4.1`
- Compilers: clang `18.0.0`

## Tests

Tests are enabled by default, but can be disabled with the CMake option `KIELI_BUILD_TESTS`, i.e. configure with `-DKIELI_BUILD_TESTS=OFF`.

To run all tests associated with a build, build the `test` target with your build system of choice, or use CMake abstraction: `cmake --build build --target test`
