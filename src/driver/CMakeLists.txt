add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE driver/main.cpp)

target_precompile_headers(${PROJECT_NAME} REUSE_FROM libutl)

target_link_libraries(${PROJECT_NAME}
    PUBLIC  libutl
    PRIVATE libformat
    PRIVATE libdisplay
    PRIVATE libresolve
    PRIVATE cpputil::io
    PRIVATE cpputil::util)
