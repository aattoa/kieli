add_library(libresolve STATIC)

target_sources(libresolve
    PRIVATE libresolve/constraint.cpp
    PRIVATE libresolve/definition_resolution.cpp
    PRIVATE libresolve/expression_resolution.cpp
    PRIVATE libresolve/generalization.cpp
    PRIVATE libresolve/method.cpp
    PRIVATE libresolve/mir.cpp
    PRIVATE libresolve/mir_formatting.cpp
    PRIVATE libresolve/mir.hpp
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
    PRIVATE libutl-diagnostics)

kieli_consume_libutl_common(libresolve)

kieli_add_tests(libresolve
    LINK libcompiler-pipeline liblex libparse libdesugar
    TESTS libresolve)
