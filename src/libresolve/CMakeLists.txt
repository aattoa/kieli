kieli_library(libresolve)

target_sources(libresolve
    PRIVATE libresolve/collection.cpp
    PRIVATE libresolve/fwd.hpp
    PRIVATE libresolve/hir.cpp
    PRIVATE libresolve/hir.hpp
    PRIVATE libresolve/hir_formatters.hpp
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
    PRIVATE libresolve/resolve_template.cpp
    PRIVATE libresolve/resolve_type.cpp
    PRIVATE libresolve/scope.cpp
    PRIVATE libresolve/unification.cpp)

target_link_libraries(libresolve
    PUBLIC libcompiler
    PUBLIC libdesugar)
