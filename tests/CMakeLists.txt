FetchContent_Declare(cppunittest
    GIT_REPOSITORY https://github.com/aattoa/cppunittest.git
    GIT_TAG        12e4bf4c101ae7c90be2cd68055d495d2b76a653)

FetchContent_MakeAvailable(cppunittest)
include(cppunittest)

# Create a test executable
function(kieli_test library name)
    cmake_parse_arguments(PARSE_ARGV 2 "" "" "" "WORKING_DIR")
    set(target test-${library}-${name})
    add_executable(${target} ${name}.test.cpp)
    target_link_libraries(${target} PRIVATE libutl PRIVATE ${library})
    target_precompile_headers(${target} REUSE_FROM libutl)
    cppunittest_add_tests(${target} WORKING_DIR "${_WORKING_DIR}")
endfunction()

add_subdirectory(libutl)
add_subdirectory(liblex)
add_subdirectory(libparse)
add_subdirectory(libdesugar)
add_subdirectory(libcompiler)
