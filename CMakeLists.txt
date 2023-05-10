cmake_minimum_required(VERSION 3.24)
project(kieli)

# Set the global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Set global warning settings
if (MSVC)
    add_compile_options("/W4")
else ()
    add_compile_options("-Wall" "-Wextra" "-Wpedantic")
    add_compile_options("-Wno-missing-field-initializers"
                        "-Wno-switch")
endif ()


# Enable sized deallocation functions on Clang
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fsized-deallocation)
endif ()


# Set globally visible include directories
include_directories(
    src/libutl-common
    src/libutl-readline
    src/libutl-source
    src/libutl-color
    src/libutl-cli
    src/libutl-diagnostics
    src/liblex
    src/libparse
    src/libdesugar
    src/libresolve
    src/libreify
    src/liblower
    src/libvm
    src/libcompiler-pipeline
    src/compiler
    dependencies/range-v3/include
    dependencies/optional/include
    dependencies/expected/include)


add_subdirectory(dependencies/fmt)
add_subdirectory(dependencies/Catch2)
add_subdirectory(src/libutl-common)
add_subdirectory(src/libutl-readline)
add_subdirectory(src/libutl-source)
add_subdirectory(src/libutl-color)
add_subdirectory(src/libutl-cli)
add_subdirectory(src/libutl-diagnostics)
add_subdirectory(src/liblex)
add_subdirectory(src/libparse)
add_subdirectory(src/libdesugar)
add_subdirectory(src/libresolve)
add_subdirectory(src/libreify)
add_subdirectory(src/liblower)
add_subdirectory(src/libvm)
add_subdirectory(src/libcompiler-pipeline)
add_subdirectory(src/compiler)


# Configure the testing executable
add_executable(tests
    tests/utl_test.cpp
    tests/lexer_test.cpp
    tests/parser_test.cpp
    tests/vm_test.cpp
    tests/resolution_test.cpp)

target_precompile_headers(tests
    REUSE_FROM libutl-common)

target_link_libraries(tests
    PRIVATE libutl-common
    PRIVATE libutl-source
    PRIVATE libutl-color
    PRIVATE libutl-diagnostics
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE libvm
    PRIVATE libcompiler-pipeline
    PRIVATE fmt::fmt
    PRIVATE Catch2::Catch2WithMain)

# Enable CTest registration
list(APPEND CMAKE_MODULE_PATH dependencies/Catch2/extras)
include(CTest)
include(Catch)
catch_discover_tests(tests)

