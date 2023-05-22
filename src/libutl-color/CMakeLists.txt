add_library(libutl-color STATIC)

target_sources(libutl-color
    PRIVATE libutl/color/color.cpp
    PRIVATE libutl/color/color.hpp)

kieli_consume_libutl_common(libutl-color)
