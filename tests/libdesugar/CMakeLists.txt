foreach(test desugar)
    kieli_test(libdesugar ${test})
    target_link_libraries(test-libdesugar-${test} PRIVATE libparse)
endforeach()
