add_library(libresolve STATIC)

target_include_directories(libresolve PUBLIC .)

target_sources(libresolve
    PRIVATE libresolve/collection.cpp
    PRIVATE libresolve/constraint.cpp
    PRIVATE libresolve/fwd.hpp
    PRIVATE libresolve/hir.cpp
    PRIVATE libresolve/hir.hpp
    PRIVATE libresolve/lookup.cpp
    PRIVATE libresolve/module.hpp
    PRIVATE libresolve/occurs_check.cpp
    PRIVATE libresolve/resolution_internals.cpp
    PRIVATE libresolve/resolution_internals.hpp
    PRIVATE libresolve/resolve.cpp
    PRIVATE libresolve/resolve.hpp
    PRIVATE libresolve/resolve_definition.cpp
    PRIVATE libresolve/resolve_expression.cpp
    PRIVATE libresolve/resolve_module.cpp
    PRIVATE libresolve/resolve_mutability.cpp
    PRIVATE libresolve/resolve_pattern.cpp
    PRIVATE libresolve/resolve_type.cpp
    PRIVATE libresolve/scope.cpp)

target_link_libraries(libresolve
    PUBLIC libphase
    PUBLIC libdesugar)

kieli_consume_libutl_common(libresolve)

kieli_add_tests(libresolve
    WORKING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests/test-project
    TESTS resolve_module resolve_project)
