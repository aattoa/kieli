add_library(libresolve STATIC)

target_include_directories(libresolve PUBLIC .)

target_sources(libresolve
    PRIVATE libresolve/context.cpp
    PRIVATE libresolve/context.hpp
    PRIVATE libresolve/environment.cpp
    PRIVATE libresolve/hir.cpp
    PRIVATE libresolve/hir.hpp
    PRIVATE libresolve/module.cpp
    PRIVATE libresolve/module.hpp
    PRIVATE libresolve/type.hpp)

target_link_libraries(libresolve
    PUBLIC libphase
    PUBLIC libdesugar)

kieli_consume_libutl_common(libresolve)

kieli_add_tests(libresolve
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-project
    TESTS modulemap)
