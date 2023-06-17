add_library(libdesugar STATIC)

target_sources(libdesugar
    PRIVATE libdesugar/desugar.cpp
    PRIVATE libdesugar/desugar.hpp
    PRIVATE libdesugar/desugar_definition.cpp
    PRIVATE libdesugar/desugar_expression.cpp
    PRIVATE libdesugar/desugar_pattern.cpp
    PRIVATE libdesugar/desugar_type.cpp
    PRIVATE libdesugar/desugaring_internals.hpp
    PRIVATE libdesugar/format_hir.cpp
    PRIVATE libdesugar/hir.cpp
    PRIVATE libdesugar/hir.hpp)

target_link_libraries(libdesugar
    PRIVATE libparse)

kieli_consume_libutl_common(libdesugar)

kieli_add_tests(libdesugar
    LINK libcompiler-pipeline liblex libparse
    TESTS libdesugar)
