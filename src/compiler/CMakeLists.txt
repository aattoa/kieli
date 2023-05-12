add_executable(kieli)

target_sources(kieli
    PRIVATE compiler/compiler.cpp
    PRIVATE compiler/compiler.hpp
    PRIVATE compiler/main.cpp
    PRIVATE compiler/project.cpp
    PRIVATE compiler/project.hpp)

target_precompile_headers(kieli
    REUSE_FROM libutl-common)

target_link_libraries(kieli
    PRIVATE libutl-cli
    PRIVATE libutl-color
    PRIVATE libutl-readline
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE libreify
    PRIVATE liblower
    PRIVATE libcompiler-pipeline
    PRIVATE fmt::fmt)
