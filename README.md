# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

## Dependencies

* https://github.com/ericniebler/range-v3
    - Required, until `std::ranges` is properly supported
* https://github.com/TartanLlama/expected
    - Required, until `std::expected` is properly supported
* https://github.com/catchorg/Catch2
    - Optional, only for tests

## Building Kieli

Requirements: make, CMake, and any C++23 compliant C++ toolchain.

Run the following commands to clone and build Kieli and its dependencies:

```Shell
git clone https://github.com/aattoa/kieli.git
cd kieli
make
```

On Linux, these build steps have been tested with the following:

* CMake `3.27.4`
* Build systems: Ninja `1.11.1`, GNU Make `4.4.1`
* Compilers: clang `16.0.6`, GCC `13.2.1`

On Windows, these build steps have been tested with Visual Studio 2022 `17.6.5`.

## Tests

Kieli tests are enabled with the CMake option `KIELI_BUILD_TESTS`, i.e. `-DKIELI_BUILD_TESTS=ON`.

This requires Catch2 `>=3.0` to be installed on your system.

To build and run tests with the default configuration, simply run `make test`.
