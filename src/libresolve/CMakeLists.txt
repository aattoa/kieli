add_library(libresolve STATIC)

target_include_directories(libresolve PUBLIC .)

target_sources(libresolve
    PRIVATE libresolve/collection.cpp
    PRIVATE libresolve/context.cpp
    PRIVATE libresolve/environment.cpp
    PRIVATE libresolve/fwd.hpp
    PRIVATE libresolve/hir.cpp
    PRIVATE libresolve/hir.hpp
    PRIVATE libresolve/module.hpp
    PRIVATE libresolve/resolution_internals.cpp
    PRIVATE libresolve/resolution_internals.hpp
    PRIVATE libresolve/resolve_module.cpp
    PRIVATE libresolve/unification.cpp
    PRIVATE libresolve/unification.hpp)

target_link_libraries(libresolve
    PUBLIC libphase
    PUBLIC libdesugar)

kieli_consume_libutl_common(libresolve)

kieli_add_tests(libresolve
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-project
    TESTS resolve_module)
