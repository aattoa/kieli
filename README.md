# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

# A note about the dependencies

The project currently depends on several external libraries. This is only temporary, as the features these libraries provide will be available in the C++23 standard library. The external dependencies are therefore only necessary until the major C++ standard library implementations start to provide support for these features.

The following is a list of the required dependencies, and what they will be replaced by in the future:

* https://github.com/fmtlib/fmt
    * Will be replaced by the standard library header `<format>`.
* https://github.com/ericniebler/range-v3
    * Will be replaced by the standard library header `<ranges>`.
* https://github.com/TartanLlama/expected
    * Will be replaced by the standard library header `<expected>`.
* https://github.com/TartanLlama/optional
    * Will be replaced by the standard library header `<optional>`.

The major 3 standard library implementations already provide some/all of these headers, but they do not yet support some of the C++23 features kieli needs, such as `std::format_string` and `std::ranges::to`.

# Cloning kieli and its dependencies

To get all of the source code needed to build kieli, simply run these commands in a directory of your choosing:

```
$ git clone https://github.com/aattoa/kieli.git
$ cd kieli
$ mkdir dependencies
$ cd dependencies
$ git clone https://github.com/fmtlib/fmt.git
$ git clone https://github.com/ericniebler/range-v3.git
$ git clone https://github.com/TartanLlama/expected.git
$ git clone https://github.com/TartanLlama/optional.git
$ cd ..
```

# Building kieli on Linux

Requirements: CMake, make, any C++20 compliant C++ toolchain.

This assumes that you have already cloned kieli and its dependencies as detailed above.

```
$ cd kieli
$ mkdir build
$ cd build
$ cmake ..
$ make -j8
```

# Building kieli on Windows

Requirements: Microsoft Visual Studio C++ toolchain with CMake integration.

This assumes that you have already cloned kieli and its dependencies as detailed above.

Open the kieli directory in Visual Studio. The directory should automatically be recognized as a CMake project. Press the "build" button.
