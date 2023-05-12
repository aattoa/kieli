add_library(libparse STATIC)

target_sources(libparse
    PRIVATE libparse/parse.cpp
    PRIVATE libparse/parser_internals.hpp
    PRIVATE libparse/parser_internals.cpp
    PRIVATE libparse/parse_expression.cpp
    PRIVATE libparse/parse_type.cpp
    PRIVATE libparse/parse_pattern.cpp
    PRIVATE libparse/ast_formatting.cpp)

target_precompile_headers(libparse
    REUSE_FROM libutl-common)

target_link_libraries(libparse
    PRIVATE fmt::fmt)
