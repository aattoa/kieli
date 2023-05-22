add_library(libutl-cli STATIC)

target_sources(libutl-cli
    PRIVATE libutl/cli/cli.cpp
    PRIVATE libutl/cli/cli.hpp)

target_link_libraries(libutl-cli
    PRIVATE libutl-diagnostics)

kieli_consume_libutl_common(libutl-cli)
