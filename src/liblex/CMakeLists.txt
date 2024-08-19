add_library(liblex STATIC)

target_sources(liblex
    PRIVATE liblex/lex.cpp
    PRIVATE liblex/lex.hpp
    PRIVATE liblex/numeric.cpp
    PRIVATE liblex/numeric.hpp
    PRIVATE liblex/state.cpp
    PRIVATE liblex/state.hpp)

target_include_directories(liblex
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(liblex
    REUSE_FROM libutl)

target_link_libraries(liblex
    PUBLIC libutl
    PUBLIC libcompiler)
