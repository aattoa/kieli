foreach(test expression pattern type)
    kieli_test(libparse ${test})
    target_sources(test-libparse-${test} PRIVATE test_interface.cpp PRIVATE test_interface.hpp)
endforeach()
