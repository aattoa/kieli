add_library(libdisplay STATIC)

target_sources(libdisplay
    PRIVATE libdisplay/display.cpp
    PRIVATE libdisplay/display.hpp)

target_include_directories(libdisplay
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libdisplay
    REUSE_FROM libutl)

target_link_libraries(libdisplay
    PUBLIC libutl
    PUBLIC libparse
    PUBLIC libdesugar
    PUBLIC libcompiler)
