add_library(libutl-readline STATIC)

target_sources(libutl-readline
    PRIVATE libutl/readline/readline.cpp
    PRIVATE libutl/readline/readline.hpp)

target_precompile_headers(libutl-readline
    REUSE_FROM libutl-common)

target_link_libraries(libutl-readline
    PRIVATE fmt::fmt)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(libutl-readline
        PRIVATE readline)
endif ()

