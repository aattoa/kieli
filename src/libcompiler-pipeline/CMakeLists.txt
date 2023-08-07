add_library(libcompiler-pipeline STATIC)

target_include_directories(libcompiler-pipeline PUBLIC .)

target_sources(libcompiler-pipeline
    PRIVATE libcompiler-pipeline/compiler-pipeline.cpp
    PRIVATE libcompiler-pipeline/compiler-pipeline.hpp)

target_link_libraries(libcompiler-pipeline
    PUBLIC libutl-source
    PUBLIC libutl-diagnostics)

kieli_consume_libutl_common(libcompiler-pipeline)
