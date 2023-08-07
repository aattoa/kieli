add_library(libutl-source STATIC)

target_include_directories(libutl-source PUBLIC .)

target_sources(libutl-source
    PRIVATE libutl/source/source.cpp
    PRIVATE libutl/source/source.hpp)

kieli_consume_libutl_common(libutl-source)

kieli_add_tests(libutl-source TESTS libutl-source)
