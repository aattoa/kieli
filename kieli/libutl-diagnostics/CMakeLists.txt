add_library(libutl-diagnostics STATIC)

target_sources(libutl-diagnostics
    PRIVATE libutl/diagnostics/diagnostics.cpp
    PRIVATE libutl/diagnostics/diagnostics.hpp)

target_precompile_headers(libutl-diagnostics
    REUSE_FROM libutl-common)

target_link_libraries(libutl-diagnostics
    PRIVATE libutl-source
    PRIVATE libutl-color
    PRIVATE fmt::fmt)

