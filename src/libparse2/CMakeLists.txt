add_library(libparse2 STATIC)

target_include_directories(libparse2 PUBLIC .)

target_sources(libparse2
    PRIVATE libparse2/cst.cpp
    PRIVATE libparse2/cst.hpp
    PRIVATE libparse2/parse.cpp
    PRIVATE libparse2/parse.hpp
    PRIVATE libparse2/parse_definition.cpp
    PRIVATE libparse2/parse_expression.cpp
    PRIVATE libparse2/parse_pattern.cpp
    PRIVATE libparse2/parse_type.cpp
    PRIVATE libparse2/parser_internals.cpp
    PRIVATE libparse2/parser_internals.hpp
    PRIVATE libparse2/test_interface.cpp
    PRIVATE libparse2/test_interface.hpp)

target_link_libraries(libparse2
    PUBLIC liblex2
    PUBLIC libformat
    PUBLIC libphase)

kieli_consume_libutl_common(libparse2)

kieli_add_tests(libparse2
    LINK liblex2 libformat
    TESTS expression pattern type definition)
