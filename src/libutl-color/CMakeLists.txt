add_library(libutl-color STATIC)

target_sources(libutl-color
    PRIVATE libutl/color/color.cpp
    PRIVATE libutl/color/color.hpp)

target_precompile_headers(libutl-color
    REUSE_FROM libutl-common)

target_link_libraries(libutl-color
    PRIVATE fmt::fmt)
