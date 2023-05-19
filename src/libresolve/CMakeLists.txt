add_library(libresolve STATIC
    libresolve/constraint.cpp
    libresolve/definition_resolution.cpp
    libresolve/expression_resolution.cpp
    libresolve/generalization.cpp
    libresolve/method.cpp
    libresolve/mir.cpp
    libresolve/mir_formatting.cpp
    libresolve/mir.hpp
    libresolve/namespace_lookup.cpp
    libresolve/pattern_resolution.cpp
    libresolve/resolution_internals.cpp
    libresolve/resolution_internals.hpp
    libresolve/resolve.cpp
    libresolve/resolve.hpp
    libresolve/scope.cpp
    libresolve/template_instantiation.cpp
    libresolve/type_resolution.cpp
    libresolve/unification.cpp)
target_precompile_headers(libresolve
    REUSE_FROM libutl-common)
target_link_libraries(libresolve
    PUBLIC libdesugar)

add_kieli_test(libresolve)
