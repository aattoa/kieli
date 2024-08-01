kieli_library(libutl)

target_sources(libutl
    PRIVATE libutl/disjoint_set.cpp
    PRIVATE libutl/disjoint_set.hpp
    PRIVATE libutl/index_vector.hpp
    PRIVATE libutl/safe_integer.hpp
    PRIVATE libutl/utilities.cpp
    PRIVATE libutl/utilities.hpp
    PRIVATE libutl/wrapper.hpp)

target_link_libraries(libutl PUBLIC cpputil::util PRIVATE cpputil::io)
target_precompile_headers(libutl PUBLIC libutl/utilities.hpp)
