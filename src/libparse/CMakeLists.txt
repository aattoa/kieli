add_library(libparse STATIC)

target_sources(libparse
    PRIVATE libparse/ast.cpp
    PRIVATE libparse/ast.hpp
    PRIVATE libparse/ast_formatting.cpp
    PRIVATE libparse/parse.cpp
    PRIVATE libparse/parse.hpp
    PRIVATE libparse/parse_expression.cpp
    PRIVATE libparse/parse_pattern.cpp
    PRIVATE libparse/parse_type.cpp
    PRIVATE libparse/parser_internals.cpp
    PRIVATE libparse/parser_internals.hpp)

target_precompile_headers(libparse
    REUSE_FROM libutl-common)

target_link_libraries(libparse
    PRIVATE liblex
    PRIVATE fmt::fmt)
