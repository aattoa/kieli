kieli_library(libutl-readline)

target_sources(libutl-readline
    PRIVATE libutl/readline/readline.cpp
    PRIVATE libutl/readline/readline.hpp)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(libutl-readline
        PRIVATE readline)
endif ()
