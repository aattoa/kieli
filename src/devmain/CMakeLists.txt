add_executable(devmain devmain/main.cpp)

target_include_directories(devmain
    PRIVATE ${PROJECT_SOURCE_DIR}/src/devmain)

kieli_use_libutl(devmain)

target_sources(devmain
    PRIVATE devmain/history.cpp
    PRIVATE devmain/history.hpp
    PRIVATE devmain/main.cpp)

target_link_libraries(devmain
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libformat
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE cppargs
    PRIVATE cppdiag
    PRIVATE cpputil::util
    PRIVATE cpputil::input
    PRIVATE cpputil::io)
