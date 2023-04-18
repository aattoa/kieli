# Kieli
A general-purpose programming language inspired by C++, Rust, and Haskell.

This is a work in progress, and can not be used for anything yet.

# Dependencies

The following is a list of the required dependencies:

* https://github.com/fmtlib/fmt
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
git clone https://github.com/fmtlib/fmt.git
git clone https://github.com/ericniebler/range-v3.git
git clone https://github.com/TartanLlama/expected.git
git clone https://github.com/TartanLlama/optional.git
git clone https://github.com/catchorg/Catch2.git
cd ..
```

# Building Kieli on Linux

This assumes that you have already cloned Kieli and its dependencies as detailed above.

Run the following commands to build kieli:

```Shell
cd kieli
mkdir build
cd build
cmake ..
cmake --build . -j 8
```

These build steps have been tested with CMake `3.26.3`, GNU Make `4.4.1`, Ninja `1.11.1`, clang `16.0.0`, and GCC `12.2.1`.

# Building Kieli on Windows

In principle, similar build steps should work on Windows, but this has not been tested.

Alternatively, you may be able to build Kieli on Windows as if you were on Linux by using the [Windows Subsystem for Linux](https://learn.microsoft.com/en-us/windows/wsl/install) and following the Linux build steps.
