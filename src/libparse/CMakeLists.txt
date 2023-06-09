add_library(libparse STATIC)

target_sources(libparse
    PRIVATE libparse/ast.cpp
    PRIVATE libparse/ast.hpp
    PRIVATE libparse/ast_formatting.cpp
    PRIVATE libparse/cst.cpp
    PRIVATE libparse/cst.hpp
    PRIVATE libparse/parse.cpp
    PRIVATE libparse/parse.hpp
    PRIVATE libparse/parse_expression.cpp
    PRIVATE libparse/parse_pattern.cpp
    PRIVATE libparse/parse_type.cpp
    PRIVATE libparse/parser_internals.cpp
    PRIVATE libparse/parser_internals.hpp)

target_link_libraries(libparse
    PRIVATE libutl-diagnostics
    PRIVATE libcompiler-pipeline
    PRIVATE liblex
    PRIVATE libdesugar) # FIXME

kieli_consume_libutl_common(libparse)

kieli_add_tests(libparse
    LINK liblex
    TESTS libparse)
