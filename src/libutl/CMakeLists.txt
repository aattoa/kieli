kieli_library(libutl)

target_sources(libutl
    PRIVATE libutl/disjoint_set.cpp
    PRIVATE libutl/disjoint_set.hpp
    PRIVATE libutl/flatmap.hpp
    PRIVATE libutl/index_vector.hpp
    PRIVATE libutl/pooled_string.cpp
    PRIVATE libutl/pooled_string.hpp
    PRIVATE libutl/safe_integer.hpp
    PRIVATE libutl/source.cpp
    PRIVATE libutl/source.hpp
    PRIVATE libutl/timer.hpp
    PRIVATE libutl/utilities.cpp
    PRIVATE libutl/utilities.hpp
    PRIVATE libutl/wrapper.hpp)

target_link_libraries(libutl PUBLIC cpputil::util PRIVATE cpputil::io)
target_precompile_headers(libutl PUBLIC libutl/utilities.hpp)
