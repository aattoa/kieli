add_library(libutl-common STATIC)

target_sources(libutl-common
    PRIVATE libutl/common/bytestack.hpp
    PRIVATE libutl/common/flatmap.hpp
    PRIVATE libutl/common/libutl-common.cpp
    PRIVATE libutl/common/noisy.hpp
    PRIVATE libutl/common/pooled_string.hpp
    PRIVATE libutl/common/safe_integer.hpp
    PRIVATE libutl/common/timer.hpp
    PRIVATE libutl/common/utilities.hpp
    PRIVATE libutl/common/wrapper.hpp)

target_precompile_headers(libutl-common
    PUBLIC libutl/common/utilities.hpp)
target_link_libraries(libutl-common
    PUBLIC fmt::fmt)

kieli_add_tests(libutl-common
    TESTS utilities flatmap safe_integer pooled_string)
