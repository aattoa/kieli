kieli_library(liblex)

target_sources(liblex
    PRIVATE liblex/lex.cpp
    PRIVATE liblex/lex.hpp
    PRIVATE liblex/numeric.cpp
    PRIVATE liblex/numeric.hpp
    PRIVATE liblex/token.cpp
    PRIVATE liblex/token.hpp
    PRIVATE liblex/state.cpp
    PRIVATE liblex/state.hpp)

target_link_libraries(liblex
    PUBLIC libphase)
