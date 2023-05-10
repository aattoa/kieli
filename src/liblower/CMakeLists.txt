add_library(liblower STATIC)

target_sources(liblower
    PRIVATE liblower/lir.hpp
    PRIVATE liblower/lir_formatting.cpp
    PRIVATE liblower/lower.cpp
    PRIVATE liblower/lower.hpp)

target_precompile_headers(liblower
    REUSE_FROM libutl-common)

target_link_libraries(liblower
    PRIVATE libreify
    PRIVATE fmt::fmt)

