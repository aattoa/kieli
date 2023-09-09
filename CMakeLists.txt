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
        "-Wall" "-Wextra" "-Wpedantic"
        "-Wno-missing-field-initializers"
        "-Wno-switch")
endif ()

# Enable sized deallocation functions on Clang
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fsized-deallocation)
endif ()

# Make default build easier by not building tests by default
option(KIELI_BUILD_TESTS "Build kieli tests" OFF)

if (${KIELI_BUILD_TESTS})
    find_package(Catch2 3 REQUIRED)
    include(CTest)
    include(Catch)
endif ()

# Convenience function for registering Kieli component tests
function (kieli_add_tests component)
    cmake_parse_arguments(PARSE_ARGV 1 "" "" "" "EXTRA_SOURCES;LINK;TESTS")
    if (NOT ${KIELI_BUILD_TESTS})
        return ()
    endif ()
    foreach (test IN LISTS _TESTS)
        set(executable ${component}-test-${test})
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
        catch_discover_tests(${executable})
        message(STATUS "<KIELI> Added test ${executable}")
    endforeach ()
endfunction ()

function (kieli_consume_libutl_common component)
    target_precompile_headers(${component} REUSE_FROM libutl-common)
    target_link_libraries(${component} PUBLIC libutl-common)
endfunction ()

add_subdirectory(src/libutl-common)
add_subdirectory(src/libutl-readline)
add_subdirectory(src/libutl-source)
add_subdirectory(src/libutl-color)
add_subdirectory(src/libutl-cli)
add_subdirectory(src/libutl-diagnostics)
add_subdirectory(src/libphase)
add_subdirectory(src/liblex)
add_subdirectory(src/libparse)
add_subdirectory(src/libdesugar)
add_subdirectory(src/libresolve)
add_subdirectory(src/libreify)
add_subdirectory(src/liblower)
add_subdirectory(src/libformat)
add_subdirectory(src/compiler)
