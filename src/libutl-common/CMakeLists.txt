add_library(libutl-common STATIC)

target_include_directories(libutl-common PUBLIC .)

target_sources(libutl-common
    PRIVATE libutl/common/flatmap.hpp
    PRIVATE libutl/common/libutl-common.cpp
    PRIVATE libutl/common/pooled_string.cpp
    PRIVATE libutl/common/pooled_string.hpp
    PRIVATE libutl/common/safe_integer.hpp
    PRIVATE libutl/common/timer.hpp
    PRIVATE libutl/common/utilities.cpp
    PRIVATE libutl/common/utilities.hpp
    PRIVATE libutl/common/wrapper.hpp)

target_link_libraries(libutl-common
    PUBLIC cpputil)

target_precompile_headers(libutl-common
    PUBLIC libutl/common/utilities.hpp)

kieli_add_tests(libutl-common
    TESTS utilities flatmap safe_integer pooled_string)
