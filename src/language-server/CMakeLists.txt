add_executable(language-server)

target_sources(language-server
    PRIVATE language-server/json.cpp
    PRIVATE language-server/json.hpp
    PRIVATE language-server/lsp.cpp
    PRIVATE language-server/main.cpp
    PRIVATE language-server/rpc.cpp
    PRIVATE language-server/rpc.hpp
    PRIVATE language-server/server.cpp
    PRIVATE language-server/server.hpp)

target_include_directories(language-server
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(language-server
    REUSE_FROM libutl)

target_link_libraries(language-server
    PUBLIC  libutl
    PRIVATE libquery
    PRIVATE cpputil::json)
