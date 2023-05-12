add_library(libdesugar STATIC)

target_sources(libdesugar
    PRIVATE libdesugar/desugar.cpp
    PRIVATE libdesugar/desugar.hpp
    PRIVATE libdesugar/desugar_definition.cpp
    PRIVATE libdesugar/desugar_expression.cpp
    PRIVATE libdesugar/desugar_pattern.cpp
    PRIVATE libdesugar/desugar_type.cpp
    PRIVATE libdesugar/desugaring_internals.hpp
    PRIVATE libdesugar/hir.hpp
    PRIVATE libdesugar/hir_formatting.cpp
    PRIVATE libdesugar/shared_formatting.cpp)

target_precompile_headers(libdesugar
    REUSE_FROM libutl-common)

target_link_libraries(libdesugar
    PRIVATE fmt::fmt)
