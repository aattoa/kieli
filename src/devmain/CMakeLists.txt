add_executable(devmain)

target_sources(devmain
    PRIVATE devmain/history.cpp
    PRIVATE devmain/history.hpp
    PRIVATE devmain/main.cpp)

target_include_directories(devmain
    PRIVATE ${PROJECT_SOURCE_DIR}/src/devmain)

target_precompile_headers(devmain
    REUSE_FROM libutl)

target_link_libraries(devmain
    PUBLIC  libutl
    PRIVATE liblex
    PRIVATE libparse
    PRIVATE libformat
    PRIVATE libdesugar
    PRIVATE libresolve
    PRIVATE cpputil::util
    PRIVATE cpputil::input
    PRIVATE cpputil::io)
