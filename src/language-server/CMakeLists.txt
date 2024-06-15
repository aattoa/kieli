add_executable(language-server language-server/main.cpp)

target_include_directories(language-server
    PRIVATE ${PROJECT_SOURCE_DIR}/src/language-server)

kieli_use_libutl(language-server)

target_sources(language-server
    PRIVATE language-server/main.cpp)

target_link_libraries(language-server
    PRIVATE libutl)
