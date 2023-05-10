add_library(libutl-cli STATIC)

target_sources(libutl-cli
    PRIVATE libutl/cli/cli.cpp
    PRIVATE libutl/cli/cli.hpp)

target_precompile_headers(libutl-cli
    REUSE_FROM libutl-common)

target_link_libraries(libutl-cli
    PRIVATE libutl-diagnostics
    PRIVATE fmt::fmt)

