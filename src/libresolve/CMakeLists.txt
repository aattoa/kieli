add_library(libresolve STATIC)

target_include_directories(libresolve PUBLIC .)

target_sources(libresolve
    PRIVATE libresolve/constraint.cpp
    PRIVATE libresolve/definition_resolution.cpp
    PRIVATE libresolve/expression_resolution.cpp
    PRIVATE libresolve/format_hir.cpp
    PRIVATE libresolve/generalization.cpp
    PRIVATE libresolve/hir.cpp
    PRIVATE libresolve/hir.hpp
    PRIVATE libresolve/method.cpp
    PRIVATE libresolve/namespace_lookup.cpp
    PRIVATE libresolve/pattern_resolution.cpp
    PRIVATE libresolve/resolution_internals.cpp
    PRIVATE libresolve/resolution_internals.hpp
    PRIVATE libresolve/resolve.cpp
    PRIVATE libresolve/resolve.hpp
    PRIVATE libresolve/scope.cpp
    PRIVATE libresolve/template_instantiation.cpp
    PRIVATE libresolve/type_resolution.cpp
    PRIVATE libresolve/unification.cpp)

target_link_libraries(libresolve
    PUBLIC libdesugar
    PUBLIC libcompiler-pipeline)

kieli_consume_libutl_common(libresolve)

kieli_add_tests(libresolve
    EXTRA_SOURCES resolution_test.hpp resolution_test.cpp
    LINK libcompiler-pipeline liblex libparse libdesugar
    TESTS libresolve)
