add_library(libformat STATIC)

target_include_directories(libformat PUBLIC .)

target_sources(libformat
    PRIVATE libformat/format.hpp
    PRIVATE libformat/format_expression.cpp
    PRIVATE libformat/format_internals.cpp
    PRIVATE libformat/format_internals.hpp
    PRIVATE libformat/format_module.cpp
    PRIVATE libformat/format_pattern.cpp
    PRIVATE libformat/format_type.cpp)

target_link_libraries(libformat
    PUBLIC libparse)

kieli_consume_libutl_common(libformat)
