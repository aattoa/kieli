add_library(libdesugar STATIC)

target_sources(libdesugar
    PRIVATE libdesugar/desugar.hpp
    PRIVATE libdesugar/desugar_definition.cpp
    PRIVATE libdesugar/desugar_expression.cpp
    PRIVATE libdesugar/desugar_pattern.cpp
    PRIVATE libdesugar/desugar_type.cpp
    PRIVATE libdesugar/desugaring_internals.cpp
    PRIVATE libdesugar/desugaring_internals.hpp)

target_include_directories(libdesugar
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libdesugar
    REUSE_FROM libutl)

target_link_libraries(libdesugar
    PUBLIC libutl
    PUBLIC libcompiler)
