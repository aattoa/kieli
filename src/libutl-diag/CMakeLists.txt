add_library(libutl-diag STATIC)

target_include_directories(libutl-diag PUBLIC .)

target_sources(libutl-diag
    PRIVATE libutl/diag/internal/diag.hpp
    PRIVATE libutl/diag/diag.hpp
    PRIVATE libutl/diag/diag.cpp)

target_link_libraries(libutl-diag
    PUBLIC libutl-color)

kieli_consume_libutl_common(libutl-diag)

kieli_add_tests(libutl-diag
    TESTS internals interface)
