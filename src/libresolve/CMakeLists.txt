add_library(libresolve STATIC)

target_sources(libresolve
    PRIVATE libresolve/collect.cpp
    PRIVATE libresolve/occurs_check.cpp
    PRIVATE libresolve/resolve.cpp
    PRIVATE libresolve/resolve.hpp
    PRIVATE libresolve/resolve_definition.cpp
    PRIVATE libresolve/resolve_expression.cpp
    PRIVATE libresolve/resolve_mutability.cpp
    PRIVATE libresolve/resolve_path.cpp
    PRIVATE libresolve/resolve_pattern.cpp
    PRIVATE libresolve/resolve_template.cpp
    PRIVATE libresolve/resolve_type.cpp
    PRIVATE libresolve/scope.cpp
    PRIVATE libresolve/unification.cpp)

target_include_directories(libresolve
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libresolve
    REUSE_FROM libutl)

target_link_libraries(libresolve
    PUBLIC libutl
    PUBLIC libparse
    PUBLIC libdesugar
    PUBLIC libcompiler)
