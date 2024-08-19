add_library(libcompiler STATIC)

target_sources(libcompiler
    PRIVATE libcompiler/ast/ast.cpp
    PRIVATE libcompiler/ast/ast.hpp
    PRIVATE libcompiler/compiler.cpp
    PRIVATE libcompiler/compiler.hpp
    PRIVATE libcompiler/cst/cst.cpp
    PRIVATE libcompiler/cst/cst.hpp
    PRIVATE libcompiler/hir/formatters.hpp
    PRIVATE libcompiler/hir/hir.cpp
    PRIVATE libcompiler/hir/hir.hpp
    PRIVATE libcompiler/token/token.cpp
    PRIVATE libcompiler/token/token.hpp
    PRIVATE libcompiler/tree_fwd.hpp)

target_include_directories(libcompiler
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_precompile_headers(libcompiler
    REUSE_FROM libutl)

target_link_libraries(libcompiler
    PUBLIC  libutl
    PUBLIC  cppdiag
    PUBLIC  cpputil::mem
    PRIVATE cpputil::io)
