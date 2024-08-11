kieli_library(libdesugar)

target_sources(libdesugar
    PRIVATE libdesugar/desugar.hpp
    PRIVATE libdesugar/desugar_definition.cpp
    PRIVATE libdesugar/desugar_expression.cpp
    PRIVATE libdesugar/desugar_pattern.cpp
    PRIVATE libdesugar/desugar_type.cpp
    PRIVATE libdesugar/desugaring_internals.cpp
    PRIVATE libdesugar/desugaring_internals.hpp)

target_link_libraries(libdesugar
    PUBLIC libcompiler)
