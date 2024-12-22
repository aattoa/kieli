cmake_minimum_required(VERSION 3.29)
project(kieli CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(cppdiag
    GIT_REPOSITORY https://github.com/aattoa/cppdiag.git
    GIT_TAG        c037929a4b8064870f3deda15e4dc1b3effa64f5)
FetchContent_Declare(cpputil
    GIT_REPOSITORY https://github.com/aattoa/cpputil.git
    GIT_TAG        6bc154d6ba019ee9be25f9666fcb2dcb7214b8dd)

set(CPPDIAG_BUILD_TESTS OFF)
set(CPPUTIL_BUILD_TESTS OFF)

message(STATUS "[kieli] Fetching dependencies...")
FetchContent_MakeAvailable(cppdiag cpputil)
message(STATUS "[kieli] Dependencies fetched.")

if(NOT MSVC)
    add_compile_options(-Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
endif()

add_subdirectory(src/libutl)
add_subdirectory(src/libcompiler)
add_subdirectory(src/liblex)
add_subdirectory(src/libparse)
add_subdirectory(src/libdesugar)
add_subdirectory(src/libformat)
add_subdirectory(src/libresolve)
add_subdirectory(src/devmain)
add_subdirectory(src/language-server)

# Build tests by default
option(KIELI_BUILD_TESTS "Build kieli tests" ON)

if(${KIELI_BUILD_TESTS})
    enable_testing()
    add_subdirectory(tests)
endif()
