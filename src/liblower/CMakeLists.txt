add_library(liblower STATIC)

target_sources(liblower
    PRIVATE liblower/lir.hpp
    PRIVATE liblower/lower.cpp
    PRIVATE liblower/lower.hpp)

target_link_libraries(liblower
    PRIVATE libreify)

kieli_consume_libutl_common(liblower)
