add_library(liblex2 STATIC)

target_include_directories(liblex2 PUBLIC .)

target_sources(liblex2
    PRIVATE liblex2/lex.cpp
    PRIVATE liblex2/lex.hpp
    PRIVATE liblex2/numeric.cpp
    PRIVATE liblex2/numeric.hpp
    PRIVATE liblex2/token.cpp
    PRIVATE liblex2/token.hpp
    PRIVATE liblex2/state.cpp
    PRIVATE liblex2/state.hpp)

target_link_libraries(liblex2
    PUBLIC libphase)

kieli_consume_libutl_common(liblex2)

kieli_add_tests(liblex2
    EXTRA_SOURCES test_liblex.hpp test_liblex.cpp
    TESTS floating_literal identifier integer_literal numeric punctuation quoted_literal trivia)
