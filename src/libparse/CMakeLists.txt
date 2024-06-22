kieli_library(libparse)

target_sources(libparse
    PRIVATE libparse/cst.cpp
    PRIVATE libparse/cst.hpp
    PRIVATE libparse/parse.cpp
    PRIVATE libparse/parse.hpp
    PRIVATE libparse/parse_definition.cpp
    PRIVATE libparse/parse_expression.cpp
    PRIVATE libparse/parse_pattern.cpp
    PRIVATE libparse/parse_type.cpp
    PRIVATE libparse/parser_internals.cpp
    PRIVATE libparse/parser_internals.hpp
    PRIVATE libparse/test_interface.cpp
    PRIVATE libparse/test_interface.hpp)

target_link_libraries(libparse
    PUBLIC liblex
    PUBLIC libformat
    PUBLIC libcompiler)
