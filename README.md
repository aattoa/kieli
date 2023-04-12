# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

# A note about the dependencies

The project currently depends on several external libraries. This is only temporary, as the features these libraries provide will be available in the C++23 standard library. The external dependencies are therefore only necessary until the major C++ standard library implementations start to provide support for these features.

The following is a list of the required dependencies:

* https://github.com/fmtlib/fmt
    * Will be replaced by the standard library header `<format>`.
* https://github.com/ericniebler/range-v3
    * Will be replaced by the standard library header `<ranges>`.
* https://github.com/TartanLlama/expected
    * Will be replaced by the standard library header `<expected>`.
* https://github.com/TartanLlama/optional
    * Will be replaced by the standard library header `<optional>`.

The major 3 standard library implementations already provide some/all of these headers, but they do not yet support some of the features Kieli needs.

# Cloning Kieli and its dependencies

Requirements: git

```Shell
git clone https://github.com/aattoa/kieli.git
cd kieli
mkdir dependencies
cd dependencies
git clone https://github.com/fmtlib/fmt.git
git clone https://github.com/ericniebler/range-v3.git
git clone https://github.com/TartanLlama/expected.git
git clone https://github.com/TartanLlama/optional.git
cd ..
```

# Building Kieli on Linux

Requirements: CMake, make, any C++20 compliant C++ toolchain.

This assumes that you have already cloned Kieli and its dependencies as detailed above.

```Shell
cd kieli
mkdir build
cd build
cmake ..
make -j8
```

These build steps have been tested with CMake `3.26.3`, GNU Make `4.4.1`, clang `16.0.0`, and GCC `12.2.1`.

# Building Kieli on Windows

In principle, similar build steps should work on Windows, but this has not been tested.

Alternatively, you may be able to build Kieli on Windows as if you were on Linux by using the [Windows Subsystem for Linux](https://learn.microsoft.com/en-us/windows/wsl/install) and following the Linux build steps.
