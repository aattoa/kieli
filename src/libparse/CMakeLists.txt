add_library(libparse STATIC)

target_sources(libparse
    PRIVATE libparse/internals.hpp
    PRIVATE libparse/parse.cpp
    PRIVATE libparse/parse.hpp
    PRIVATE libparse/parse_definition.cpp
    PRIVATE libparse/parse_expression.cpp
    PRIVATE libparse/parse_pattern.cpp
    PRIVATE libparse/parse_type.cpp)

target_include_directories(libparse
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libparse
    REUSE_FROM libutl)

target_link_libraries(libparse
    PUBLIC libutl
    PUBLIC liblex
    PUBLIC libformat
    PUBLIC libcompiler)
