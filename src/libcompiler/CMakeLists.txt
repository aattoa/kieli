add_library(libcompiler STATIC)

target_sources(libcompiler
    PRIVATE libcompiler/ast/ast.cpp
    PRIVATE libcompiler/ast/ast.hpp
    PRIVATE libcompiler/ast/display.cpp
    PRIVATE libcompiler/ast/display.hpp
    PRIVATE libcompiler/compiler.cpp
    PRIVATE libcompiler/compiler.hpp
    PRIVATE libcompiler/cst/cst.cpp
    PRIVATE libcompiler/cst/cst.hpp
    PRIVATE libcompiler/db.hpp
    PRIVATE libcompiler/fwd.hpp
    PRIVATE libcompiler/hir/formatters.hpp
    PRIVATE libcompiler/hir/hir.cpp
    PRIVATE libcompiler/hir/hir.hpp
    PRIVATE libcompiler/lsp.hpp)

target_include_directories(libcompiler
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libcompiler
    REUSE_FROM libutl)

target_link_libraries(libcompiler
    PUBLIC  libutl
    PRIVATE cpputil::io)
