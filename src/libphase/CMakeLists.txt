kieli_library(libphase)

target_sources(libphase
    PRIVATE libphase/phase.cpp
    PRIVATE libphase/phase.hpp)

target_link_libraries(libphase
    PUBLIC cppdiag)
