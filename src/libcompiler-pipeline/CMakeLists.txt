add_library(libcompiler-pipeline STATIC)

target_sources(libcompiler-pipeline
    PRIVATE libcompiler-pipeline/compiler-pipeline.cpp
    PRIVATE libcompiler-pipeline/compiler-pipeline.hpp)

kieli_consume_libutl_common(libcompiler-pipeline)
