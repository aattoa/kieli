add_library(liblex STATIC)

target_sources(liblex
    PRIVATE liblex/lex.cpp
    PRIVATE liblex/lex.hpp
    PRIVATE liblex/token.cpp
    PRIVATE liblex/token.hpp
    PRIVATE liblex/token_formatting.cpp)

target_link_libraries(liblex
    PRIVATE libutl-source
    PRIVATE libutl-diagnostics
    PRIVATE libcompiler-pipeline)

kieli_consume_libutl_common(liblex)
kieli_add_tests(liblex TESTS liblex)
