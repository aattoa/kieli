add_library(libphase STATIC)

target_include_directories(libphase PUBLIC .)

target_sources(libphase
    PRIVATE libphase/phase.cpp
    PRIVATE libphase/phase.hpp)

target_link_libraries(libphase
    PUBLIC libutl-source
    PUBLIC cppdiag)

kieli_consume_libutl_common(libphase)
