add_library(libutl-readline STATIC)

target_sources(libutl-readline
    PRIVATE libutl/readline/readline.cpp
    PRIVATE libutl/readline/readline.hpp)

kieli_consume_libutl_common(libutl-readline)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(libutl-readline
        PRIVATE readline)
endif ()
