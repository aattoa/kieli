add_library(libformat STATIC)

target_sources(libformat
    PRIVATE libformat/format.hpp
    PRIVATE libformat/format_expression.cpp
    PRIVATE libformat/format_module.cpp
    PRIVATE libformat/format_pattern.cpp
    PRIVATE libformat/format_type.cpp
    PRIVATE libformat/internals.cpp
    PRIVATE libformat/internals.hpp)

target_include_directories(libformat
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libformat
    REUSE_FROM libutl)

target_link_libraries(libformat
    PUBLIC libutl
    PUBLIC libcompiler)
