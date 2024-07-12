kieli_library(libquery)

target_sources(libquery
    PRIVATE libquery/query.cpp
    PRIVATE libquery/query.hpp)

target_link_libraries(libquery
    PUBLIC libcompiler
    PUBLIC libparse
    PUBLIC libdesugar
    PUBLIC libresolve)
