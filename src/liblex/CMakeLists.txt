add_library(liblex STATIC)

target_sources(liblex
    PRIVATE liblex/lex.hpp
    PRIVATE liblex/lex.cpp
    PRIVATE liblex/token.hpp
    PRIVATE liblex/token_formatting.cpp)

target_precompile_headers(liblex
    REUSE_FROM libutl-common)

target_link_libraries(liblex
    PRIVATE fmt::fmt)

