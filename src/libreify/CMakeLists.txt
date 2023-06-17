add_library(libreify STATIC)

target_sources(libreify
    PRIVATE libreify/cir.hpp
    PRIVATE libreify/expression_reification.cpp
    PRIVATE libreify/format_cir.cpp
    PRIVATE libreify/pattern_reification.cpp
    PRIVATE libreify/reification_internals.cpp
    PRIVATE libreify/reification_internals.hpp
    PRIVATE libreify/reify.cpp
    PRIVATE libreify/reify.hpp
    PRIVATE libreify/type_reification.cpp)

kieli_consume_libutl_common(libreify)
