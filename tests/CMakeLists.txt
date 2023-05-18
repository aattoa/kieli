add_executable(tests
    libutl-common/test.cpp
    liblex/test.cpp
    libparse/test.cpp
    libresolve/test.cpp
    libvm/test.cpp)
target_precompile_headers(tests
    REUSE_FROM libutl-common)
target_link_libraries(tests
    PRIVATE libutl-common
    PRIVATE libutl-source
    PRIVATE libutl-color
    PRIVATE libutl-diagnostics
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE libvm
    PRIVATE libcompiler-pipeline
    PRIVATE fmt::fmt
    PRIVATE Catch2::Catch2WithMain)

catch_discover_tests(tests)

