add_library(libparse STATIC
    libparse/ast.cpp
    libparse/ast.hpp
    libparse/ast_formatting.cpp
    libparse/parse.cpp
    libparse/parse.hpp
    libparse/parse_expression.cpp
    libparse/parse_pattern.cpp
    libparse/parse_type.cpp
    libparse/parser_internals.cpp
    libparse/parser_internals.hpp)
target_precompile_headers(libparse
    REUSE_FROM libutl-common)
target_link_libraries(libparse
    PUBLIC libdesugar # FIXME
    PUBLIC liblex)

add_kieli_test(libparse)
