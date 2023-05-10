add_library(libcompiler-pipeline STATIC)

target_sources(libcompiler-pipeline
    PRIVATE libcompiler-pipeline/compiler-pipeline.cpp
    PRIVATE libcompiler-pipeline/compiler-pipeline.hpp)

target_precompile_headers(libcompiler-pipeline
    REUSE_FROM libutl-common)

target_link_libraries(libcompiler-pipeline
    PRIVATE fmt::fmt)

