add_library(libutl-diagnostics STATIC)

target_sources(libutl-diagnostics
    PRIVATE libutl/diagnostics/diagnostics.cpp
    PRIVATE libutl/diagnostics/diagnostics.hpp)

target_link_libraries(libutl-diagnostics
    PRIVATE libutl-source
    PRIVATE libutl-color)

kieli_consume_libutl_common(libutl-diagnostics)
