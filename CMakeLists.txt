cmake_minimum_required(VERSION 3.24)
project(kieli CXX)

set(CMAKE_CXX_STANDARD            23)
set(CMAKE_CXX_STANDARD_REQUIRED   ON)
set(CMAKE_CXX_EXTENSIONS          OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT MSVC)
    add_compile_options(
        "-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Werror"
        "-Wno-missing-field-initializers"
        "-Wno-switch")
endif ()

include(FetchContent)
FetchContent_Declare(
    cppargs
    GIT_REPOSITORY https://github.com/aattoa/cppargs.git
    GIT_TAG        7499a98c9605256281f0d14101d2269e86541b32)
FetchContent_Declare(
    cppdiag
    GIT_REPOSITORY https://github.com/aattoa/cppdiag.git
    GIT_TAG        302d6212f5b4313e285a0e703d4b4dd6155cc6d9)
FetchContent_Declare(
    cpputil
    GIT_REPOSITORY https://github.com/aattoa/cpputil.git
    GIT_TAG        0ee8e43852cb5aa20803f59529ff7e1a4cbc09ae)
FetchContent_Declare(
    cppunittest
    GIT_REPOSITORY https://github.com/aattoa/cppunittest.git
    GIT_TAG        12e4bf4c101ae7c90be2cd68055d495d2b76a653)

set(CPPARGS_BUILD_TESTS OFF)
set(CPPDIAG_BUILD_TESTS OFF)
set(CPPUTIL_BUILD_TESTS OFF)

# TODO: fix cpputil::input readline availability detection
set(READLINE readline)

message(STATUS "<KIELI> Fetching dependencies...")
FetchContent_MakeAvailable(cppargs cppdiag cpputil cppunittest)
message(STATUS "<KIELI> Dependencies fetched.")

# Build kieli tests by default
option(KIELI_BUILD_TESTS "Build kieli tests" ON)
if (${KIELI_BUILD_TESTS})
    include(cppunittest)
endif ()

function (kieli_use_libutl target)
    target_precompile_headers(${target} REUSE_FROM libutl)
    target_link_libraries(${target} PUBLIC libutl)
endfunction ()

# Create a Kieli library
function (kieli_library name)
    add_library(${name})
    target_include_directories(${name} PUBLIC ${PROJECT_SOURCE_DIR}/src/${name})
    if (NOT "${name}" STREQUAL "libutl")
        kieli_use_libutl(${name})
    endif ()
    set(tests ${PROJECT_SOURCE_DIR}/src/${name}/tests)
    if (${KIELI_BUILD_TESTS} AND EXISTS ${tests})
        add_subdirectory(${tests})
    endif ()
endfunction ()

# Create a Kieli library test executable
function (kieli_test library name)
    cmake_parse_arguments(PARSE_ARGV 2 "" "" "" "WORKING_DIR")
    set(target test-${library}-${name})
    add_executable(${target} ${name}.test.cpp)
    target_link_libraries(${target} PRIVATE ${library})
    kieli_use_libutl(${target})
    cppunittest_add_tests(${target} WORKING_DIR "${_WORKING_DIR}")
endfunction ()

add_subdirectory(src/libutl)
add_subdirectory(src/libphase)
add_subdirectory(src/liblex)
add_subdirectory(src/libparse)
add_subdirectory(src/libdesugar)
add_subdirectory(src/libformat)
add_subdirectory(src/libresolve)
add_subdirectory(src/libhistory)

add_executable(debug-kieli src/main.cpp)
kieli_use_libutl(debug-kieli)
target_link_libraries(debug-kieli
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libformat
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE libhistory
    PRIVATE cppargs
    PRIVATE cppdiag
    PRIVATE cpputil::util
    PRIVATE cpputil::input)
