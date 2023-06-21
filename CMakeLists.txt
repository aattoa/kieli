cmake_minimum_required(VERSION 3.24)
project(kieli)

# Set the global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set global warning settings
if (MSVC)
    add_compile_options(
        "/W4"
        "/wd4127"  # Conditional expression is constant
        "/wd4459"  # Declaration hides global declaration
        "/wd4063") # Invalid value for switch
else ()
    add_compile_options(
        "-Wall" "-Wextra" "-Wpedantic"
        "-Wno-missing-field-initializers"
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
    src/libformat
    src/compiler
    dependencies/range-v3/include
    dependencies/optional/include
    dependencies/expected/include)

add_subdirectory(dependencies/Catch2)

# Enable CTest registration
list(APPEND CMAKE_MODULE_PATH dependencies/Catch2/extras)
include(CTest)
include(Catch)

# Convenience function for registering Kieli component tests
function (kieli_add_tests component)
    cmake_parse_arguments(PARSE_ARGV 1 "" "" "" "EXTRA_SOURCES;LINK;TESTS")
    foreach (test IN LISTS _TESTS)
        set(executable ${component}-test-${test})
        add_executable(${executable}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test}.test.cpp)
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
        catch_discover_tests(${executable})
        message(STATUS "<KIELI> Added test ${executable}")
    endforeach ()
endfunction ()

function (kieli_consume_libutl_common component)
    target_precompile_headers(${component} REUSE_FROM libutl-common)
    target_link_libraries(${component} PRIVATE libutl-common)
endfunction ()

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
add_subdirectory(src/libcompiler-pipeline)
add_subdirectory(src/libformat)
add_subdirectory(src/compiler)
