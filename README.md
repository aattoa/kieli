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
- A build system, such as GNU make or Ninja
- A C++23 compliant C++ toolchain

Run the following commands to clone and build Kieli:

```Shell
git clone https://github.com/aattoa/kieli.git
cd kieli
cmake -S . -B build
cmake --build build -j 8
```

These build steps have been tested with the following:

- CMake `3.27.8`
- Build systems: Ninja `1.11.1`, GNU Make `4.4.1`
- Compilers: clang `16.0.6`, GCC `13.2.1`

## Tests

Tests are enabled by default, but can disabled with the CMake option `KIELI_BUILD_TESTS`, i.e. `-DKIELI_BUILD_TESTS=OFF`.

To run all tests associated with a build, build the `test` target with your build system of choice, or use CMake abstraction: `cmake --build build --target test`
