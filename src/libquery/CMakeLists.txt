add_library(libquery STATIC)

target_sources(libquery
    PRIVATE libquery/query.cpp
    PRIVATE libquery/query.hpp)

target_include_directories(libquery
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libquery
    REUSE_FROM libutl)

target_link_libraries(libquery
    PUBLIC libutl
    PUBLIC libparse
    PUBLIC libdesugar
    PUBLIC libresolve
    PUBLIC libcompiler)
