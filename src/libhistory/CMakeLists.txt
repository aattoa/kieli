kieli_library(libhistory)

target_sources(libhistory
    PRIVATE libhistory/history.cpp
    PRIVATE libhistory/history.hpp)

target_link_libraries(libhistory PRIVATE cpputil::io PRIVATE cpputil::input)
