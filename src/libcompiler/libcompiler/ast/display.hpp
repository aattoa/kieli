#ifndef KIELI_LIBCOMPILER_AST_DISPLAY
#define KIELI_LIBCOMPILER_AST_DISPLAY

#include <libcompiler/ast/ast.hpp>

namespace ki::ast {

    auto display(Arena const&, utl::String_pool const&, Function const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Struct const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Enum const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Alias const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Concept const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Submodule const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Definition const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Expression const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Pattern const&) -> std::string;
    auto display(Arena const&, utl::String_pool const&, Type const&) -> std::string;

} // namespace ki::ast

#endif // KIELI_LIBCOMPILER_AST_DISPLAY
