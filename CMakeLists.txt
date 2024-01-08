cmake_minimum_required(VERSION 3.24)
project(kieli CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Produce compile_commands.json for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(
        "/W4"
        "/wd4127"  # Conditional expression is constant
        "/wd4459"  # Declaration hides global declaration
        "/wd4063") # Invalid value for switch
else ()
    add_compile_options(
        "-Wall"
        "-Wextra"
        "-Wpedantic"
        "-Werror"
        "-Wno-missing-field-initializers"
        "-Wno-switch")
endif ()

function (kieli_message message)
    message(STATUS "<KIELI> ${message}")
endfunction ()

# External dependencies
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        766541d12d64845f5232a1ce4e34a85e83506b09)
FetchContent_Declare(
    cppargs
    GIT_REPOSITORY https://github.com/aattoa/cppargs.git
    GIT_TAG        7499a98c9605256281f0d14101d2269e86541b32)
FetchContent_Declare(
    cppdiag
    GIT_REPOSITORY https://github.com/aattoa/cppdiag.git
    GIT_TAG        383bc5ac34c7a7ec986ef8c8243bd2457638cb4d)
FetchContent_Declare(
    cpputil
    GIT_REPOSITORY https://github.com/aattoa/cpputil.git
    GIT_TAG        dabf5399fa82be60d695f006058881b1d228cb4d)

set(CPPARGS_BUILD_TESTS OFF)
set(CPPDIAG_BUILD_TESTS OFF)
set(CPPUTIL_BUILD_TESTS OFF)

function (fetch package)
    kieli_message("Fetching ${package}")
    FetchContent_MakeAvailable(${package})
endfunction ()

fetch(cppargs)
fetch(cppdiag)
fetch(cpputil)

# Build kieli tests by default
option(KIELI_BUILD_TESTS "Build kieli tests" ON)

if (${KIELI_BUILD_TESTS})
    kieli_message("Enabling tests and fetching Catch2")
    FetchContent_MakeAvailable(Catch2)
    include(CTest)
    include(Catch)
endif ()

# Convenience function for registering Kieli component tests
function (kieli_add_tests component)
    cmake_parse_arguments(PARSE_ARGV 1 "" "" "" "EXTRA_SOURCES;LINK;WORKING_DIR;TESTS")
    if (NOT ${KIELI_BUILD_TESTS})
        return ()
    endif ()
    foreach (test IN LISTS _TESTS)
        set(executable ${component}-test-${test})
        kieli_message("Adding test ${executable}")
        add_executable(${executable}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}.test.cpp)
        target_include_directories(${executable}
            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        foreach (source IN LISTS _EXTRA_SOURCES)
            target_sources(${executable}
                PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/${source})
        endforeach ()
        kieli_consume_libutl_common(${executable})
        target_link_libraries(${executable}
            PRIVATE ${component}
            PRIVATE Catch2::Catch2WithMain)
        foreach (library IN LISTS _LINK)
            target_link_libraries(${executable}
                PRIVATE ${library})
        endforeach ()
        if (_WORKING_DIR)
            catch_discover_tests(${executable} WORKING_DIRECTORY ${_WORKING_DIR})
        else ()
            catch_discover_tests(${executable})
        endif ()
    endforeach ()
endfunction ()

function (kieli_consume_libutl_common component)
    target_precompile_headers(${component} REUSE_FROM libutl-common)
    target_link_libraries(${component} PUBLIC libutl-common)
endfunction ()

add_subdirectory(src/libutl-common)
add_subdirectory(src/libutl-readline)
add_subdirectory(src/libutl-source)
add_subdirectory(src/libphase)
add_subdirectory(src/liblex)
add_subdirectory(src/libparse)
add_subdirectory(src/libdesugar)
add_subdirectory(src/libformat)
add_subdirectory(src/libresolve)

add_executable(debug-kieli src/main.cpp)
target_link_libraries(debug-kieli
    PRIVATE libutl-readline
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE libformat
    PRIVATE cppargs
    PRIVATE cppdiag
    PRIVATE cpputil)
kieli_consume_libutl_common(debug-kieli)
