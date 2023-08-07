add_library(liblower STATIC)

target_include_directories(liblower PUBLIC .)

target_sources(liblower
    PRIVATE liblower/lir.hpp
    PRIVATE liblower/lower.cpp
    PRIVATE liblower/lower.hpp)

target_link_libraries(liblower
    PUBLIC libreify
    PUBLIC libcompiler-pipeline)

kieli_consume_libutl_common(liblower)
