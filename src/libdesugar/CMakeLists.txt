add_library(libdesugar STATIC)

target_include_directories(libdesugar PUBLIC .)

target_sources(libdesugar
    PRIVATE libdesugar/ast.cpp
    PRIVATE libdesugar/ast.hpp
    PRIVATE libdesugar/ast_formatters.hpp
    PRIVATE libdesugar/desugar.cpp
    PRIVATE libdesugar/desugar.hpp
    PRIVATE libdesugar/desugar_definition.cpp
    PRIVATE libdesugar/desugar_expression.cpp
    PRIVATE libdesugar/desugar_pattern.cpp
    PRIVATE libdesugar/desugar_type.cpp
    PRIVATE libdesugar/desugaring_internals.hpp)

target_link_libraries(libdesugar
    PUBLIC libparse
    PUBLIC libphase)

kieli_consume_libutl_common(libdesugar)

kieli_add_tests(libdesugar
    LINK libphase libparse
    TESTS libdesugar)
