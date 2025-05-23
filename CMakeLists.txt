cmake_minimum_required(VERSION 3.29)
project(kieli CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(cpputil
    GIT_REPOSITORY https://github.com/aattoa/cpputil.git
    GIT_TAG        c39d236308e4857a6590ab3449dbdebfc606cf5d)
set(CPPUTIL_BUILD_TESTS OFF)

message(STATUS "[kieli] Fetching dependencies...")
FetchContent_MakeAvailable(cpputil)
message(STATUS "[kieli] Dependencies fetched.")

if(NOT MSVC)
    add_compile_options(-Werror -Wall -Wextra -Wpedantic)
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

option(KIELI_BUILD_TESTS "Build kieli tests" ON)

if(${KIELI_BUILD_TESTS})
    enable_testing()
    add_subdirectory(tests)
endif()
