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

# Dependencies
include(FetchContent)
FetchContent_Declare(
    cppargs
    GIT_REPOSITORY https://github.com/aattoa/cppargs.git
    GIT_TAG        7499a98c9605256281f0d14101d2269e86541b32)
FetchContent_Declare(
    cppdiag
    GIT_REPOSITORY https://github.com/aattoa/cppdiag.git
    GIT_TAG        69a878ecf974290d3f10e90ece49433a374f2099)
FetchContent_Declare(
    cpputil
    GIT_REPOSITORY https://github.com/aattoa/cpputil.git
    GIT_TAG        dabf5399fa82be60d695f006058881b1d228cb4d)
FetchContent_Declare(
    cppunittest
    GIT_REPOSITORY https://github.com/aattoa/cppunittest.git
    GIT_TAG        9396bcafe5a65178acce4f3962f91fb17777892d)

set(CPPARGS_BUILD_TESTS OFF)
set(CPPDIAG_BUILD_TESTS OFF)
set(CPPUTIL_BUILD_TESTS OFF)

message(STATUS "<KIELI> Fetching dependencies...")
FetchContent_MakeAvailable(cppargs cppdiag cpputil cppunittest)
message(STATUS "<KIELI> Dependencies fetched.")

# Build kieli tests by default
option(KIELI_BUILD_TESTS "Build kieli tests" ON)
if (${KIELI_BUILD_TESTS})
    include(cppunittest)
endif ()

# Convenience function for registering Kieli component tests
function (kieli_add_tests component)
    cmake_parse_arguments(PARSE_ARGV 1 "" "" "" "EXTRA_SOURCES;LINK;WORKING_DIR;TESTS")
    if (NOT ${KIELI_BUILD_TESTS})
        return ()
    endif ()
    foreach (test IN LISTS _TESTS)
        set(executable ${component}-test-${test})
        add_executable(${executable} ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}.test.cpp)
        target_include_directories(${executable} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        foreach (source IN LISTS _EXTRA_SOURCES)
            target_sources(${executable} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/${source})
        endforeach ()
        kieli_consume_libutl_common(${executable})
        target_link_libraries(${executable} PRIVATE ${component})
        foreach (library IN LISTS _LINK)
            target_link_libraries(${executable} PRIVATE ${library})
        endforeach ()
        cppunittest_add_tests(${executable} WORKING_DIR "${_WORKING_DIR}")
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
