add_library(liblex STATIC
    liblex/lex.cpp
    liblex/lex.hpp
    liblex/token.cpp
    liblex/token.hpp
    liblex/token_formatting.cpp)
target_precompile_headers(liblex
    REUSE_FROM libutl-common)
target_link_libraries(liblex
    PUBLIC libutl-common
    PUBLIC libutl-source
    PUBLIC libutl-diagnostics
    PUBLIC libcompiler-pipeline)

add_kieli_test(liblex)
