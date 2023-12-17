add_library(libresolve STATIC)

target_include_directories(libresolve PUBLIC .)

target_sources(libresolve
    PRIVATE libresolve/hir.cpp
    PRIVATE libresolve/hir.hpp
    PRIVATE libresolve/module.cpp
    PRIVATE libresolve/module.hpp)

target_link_libraries(libresolve
    PUBLIC libphase
    PUBLIC libdesugar)

kieli_consume_libutl_common(libresolve)
