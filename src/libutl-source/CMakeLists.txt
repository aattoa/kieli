add_library(libutl-source STATIC)

target_sources(libutl-source
    PRIVATE libutl/source/source.cpp
    PRIVATE libutl/source/source.hpp)

target_precompile_headers(libutl-source
    REUSE_FROM libutl-common)

target_link_libraries(libutl-source
    PRIVATE fmt::fmt)

