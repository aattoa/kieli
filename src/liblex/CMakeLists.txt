add_library(liblex STATIC)

target_include_directories(liblex PUBLIC .)

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

kieli_consume_libutl_common(liblex)

kieli_add_tests(liblex
    EXTRA_SOURCES test_liblex.hpp test_liblex.cpp
    TESTS floating_literal identifier integer_literal numeric punctuation quoted_literal trivia)
