# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

# Dependencies

The following is a list of the required dependencies:

* https://github.com/ericniebler/range-v3
* https://github.com/TartanLlama/expected
* https://github.com/TartanLlama/optional
* https://github.com/catchorg/Catch2

# Cloning Kieli and its dependencies

Run the following commands to get all of the source code needed to build kieli:

```Shell
git clone https://github.com/aattoa/kieli.git
cd kieli
mkdir dependencies
cd dependencies
git clone https://github.com/ericniebler/range-v3.git
git clone https://github.com/TartanLlama/expected.git
git clone https://github.com/TartanLlama/optional.git
git clone https://github.com/catchorg/Catch2.git
cd ..
```

# Building Kieli

This assumes that you have already cloned Kieli and its dependencies as detailed above.

Requirements: CMake and any C++20 compliant C++ toolchain.

Run the following commands to build Kieli:

```Shell
cd kieli
cmake -S . -B build
cmake --build build -j 8
```

On Linux, these build steps have been tested with the following:

* CMake `3.26.4`
* Build systems: Ninja `1.11.1`, GNU Make `4.4.1`
* Compilers: clang `15.0.7`, GCC `13.1.1`

On Windows, these build steps have been tested with Visual Studio 2022 `17.5.4`.
