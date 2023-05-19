add_library(libdesugar STATIC
    libdesugar/desugar.cpp
    libdesugar/desugar.hpp
    libdesugar/desugar_definition.cpp
    libdesugar/desugar_expression.cpp
    libdesugar/desugar_pattern.cpp
    libdesugar/desugar_type.cpp
    libdesugar/desugaring_internals.hpp
    libdesugar/hir.hpp
    libdesugar/hir_formatting.cpp
    libdesugar/shared_formatting.cpp)
target_precompile_headers(libdesugar
    REUSE_FROM libutl-common)
target_link_libraries(libdesugar
    PUBLIC libparse)
