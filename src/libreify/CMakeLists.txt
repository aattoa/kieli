add_library(libreify STATIC)

target_sources(libreify
    PRIVATE libreify/cir.hpp
    PRIVATE libreify/cir_formatting.cpp
    PRIVATE libreify/expression_reification.cpp
    PRIVATE libreify/pattern_reification.cpp
    PRIVATE libreify/reification_internals.cpp
    PRIVATE libreify/reification_internals.hpp
    PRIVATE libreify/reify.cpp
    PRIVATE libreify/reify.hpp
    PRIVATE libreify/type_reification.cpp)

target_precompile_headers(libreify
    REUSE_FROM libutl-common)

target_link_libraries(libreify
    PRIVATE fmt::fmt)
