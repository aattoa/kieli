add_library(libutl-common STATIC
    libutl/common/bytestack.hpp
    libutl/common/flatmap.hpp
    libutl/common/libutl-common.cpp
    libutl/common/noisy.hpp
    libutl/common/pooled_string.hpp
    libutl/common/safe_integer.hpp
    libutl/common/timer.hpp
    libutl/common/utilities.hpp
    libutl/common/wrapper.hpp)
target_precompile_headers(libutl-common
    PUBLIC libutl/common/utilities.hpp)
target_link_libraries(libutl-common
    PUBLIC fmt::fmt)

add_kieli_test(libutl-common)
