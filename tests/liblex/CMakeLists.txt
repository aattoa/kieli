foreach(test floating_literal identifier integer_literal numeric punctuation quoted_literal trivia)
    kieli_test(liblex ${test})
    target_sources(test-liblex-${test} PRIVATE test_liblex.cpp PRIVATE test_liblex.hpp)
endforeach()
