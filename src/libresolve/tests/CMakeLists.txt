foreach (test module project)
    kieli_test(libresolve ${test} WORKING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/test-project")
endforeach ()
