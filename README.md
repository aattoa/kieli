# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

# Dependencies

The project currently depends on several external libraries. This is only temporary, as the features these libraries provide will be available in the C++23 standard library. The external dependencies are therefore only necessary until the major C++ standard library implementations start to provide support for these features.

In order to build the project, you will need to manually clone the following repositories to a directory of your choice (just not within the kieli directory):

* https://github.com/fmtlib/fmt
* https://github.com/ericniebler/range-v3
* https://github.com/TartanLlama/expected
* https://github.com/TartanLlama/optional

Then follow these simple steps:

* After cloning the kieli repository, create a directory `dependencies` inside the `kieli` directory.
* Copy each external library's respective `include` directory to `kieli/dependencies`.
* Finally copy fmtlib's `src` directory to `kieli/dependencies`.

After completing these steps, `kieli/dependencies` should have the following layout:

```
dependencies/
|-- include/
|   |-- concepts/
|   |-- fmt/
|   |-- meta/
|   |-- module.modulemap
|   |-- range/
|   |-- std/
|   \-- tl/
\-- src
    \- fmt/
```

# Building kieli on Linux

Requirements: CMake, make, any C++20 compliant C++ toolchain.

This assumes that you have already installed the dependencies as detailed in the Dependencies section.

```
$ cd kieli
$ mkdir build
$ cd build
$ cmake ..
$ make -j8
```

# Building kieli on Windows

Requirements: Microsoft Visual Studio C++ toolchain with CMake integration

This assumes that you have already installed the dependencies as detailed in the Dependencies section.

Open the kieli directory in Visual Studio. The directory should automatically be recognized as a CMake project. Press the "build" button.
