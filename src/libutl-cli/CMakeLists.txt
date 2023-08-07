add_library(libutl-cli STATIC)

target_include_directories(libutl-cli PUBLIC .)

target_sources(libutl-cli
    PRIVATE libutl/cli/cli.cpp
    PRIVATE libutl/cli/cli.hpp)

target_link_libraries(libutl-cli
    PUBLIC libutl-diagnostics)

kieli_consume_libutl_common(libutl-cli)
