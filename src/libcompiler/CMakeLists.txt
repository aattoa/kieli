kieli_library(libcompiler)

target_sources(libcompiler
    PRIVATE libcompiler/compiler.cpp
    PRIVATE libcompiler/compiler.hpp)

target_link_libraries(libcompiler
    PUBLIC cppdiag)
