add_library(libutl-diagnostics STATIC)

target_include_directories(libutl-diagnostics PUBLIC .)

target_sources(libutl-diagnostics
    PRIVATE libutl/diagnostics/diagnostics.cpp
    PRIVATE libutl/diagnostics/diagnostics.hpp)

target_link_libraries(libutl-diagnostics
    PUBLIC libutl-source
    PUBLIC libutl-color)

kieli_consume_libutl_common(libutl-diagnostics)
