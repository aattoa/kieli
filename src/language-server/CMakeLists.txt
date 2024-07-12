add_executable(language-server)

target_include_directories(language-server
    PRIVATE ${PROJECT_SOURCE_DIR}/src/language-server)

kieli_use_libutl(language-server)

target_sources(language-server
    PRIVATE language-server/lsp.cpp
    PRIVATE language-server/lsp.hpp
    PRIVATE language-server/main.cpp
    PRIVATE language-server/rpc.cpp
    PRIVATE language-server/rpc.hpp)

target_link_libraries(language-server
    PRIVATE libquery
    PRIVATE cpputil::json)
