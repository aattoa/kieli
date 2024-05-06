kieli_library(libutl-common)

target_sources(libutl-common
    PRIVATE libutl/common/common.cpp
    PRIVATE libutl/common/disjoint_set.cpp
    PRIVATE libutl/common/disjoint_set.hpp
    PRIVATE libutl/common/flatmap.hpp
    PRIVATE libutl/common/index_vector.hpp
    PRIVATE libutl/common/pooled_string.cpp
    PRIVATE libutl/common/pooled_string.hpp
    PRIVATE libutl/common/safe_integer.hpp
    PRIVATE libutl/common/source.cpp
    PRIVATE libutl/common/source.hpp
    PRIVATE libutl/common/timer.hpp
    PRIVATE libutl/common/utilities.cpp
    PRIVATE libutl/common/utilities.hpp
    PRIVATE libutl/common/wrapper.hpp)

target_link_libraries(libutl-common
    PUBLIC cpputil::util
    PRIVATE cpputil::io)

target_precompile_headers(libutl-common
    PUBLIC libutl/common/utilities.hpp)
