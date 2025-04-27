add_library(libserver STATIC)

target_sources(libserver
    PRIVATE language-server/documentation.cpp
    PRIVATE language-server/json.cpp
    PRIVATE language-server/json.hpp
    PRIVATE language-server/rpc.cpp
    PRIVATE language-server/rpc.hpp
    PRIVATE language-server/server.cpp
    PRIVATE language-server/server.hpp)

target_include_directories(libserver
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libserver
    REUSE_FROM libutl)

target_link_libraries(libserver
    PUBLIC libutl
    PUBLIC libcompiler
    PUBLIC libparse
    PUBLIC libformat
    PUBLIC libresolve
    PUBLIC cpputil::json)


add_executable(language-server)

target_sources(language-server
    PRIVATE language-server/main.cpp)

target_precompile_headers(language-server
    REUSE_FROM libutl)

target_link_libraries(language-server
    PRIVATE libutl
    PRIVATE libserver)
