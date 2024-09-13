#include <libutl/utilities.hpp>
#include <libcompiler/ast/ast.hpp>

auto kieli::ast::describe_loop_source(Loop_source const source) -> std::string_view
{
    switch (source) {
    case Loop_source::plain_loop: return "plain loop";
    case Loop_source::while_loop: return "while loop";
    case Loop_source::for_loop:   return "for loop";
    default:                      cpputil::unreachable();
    }
}

auto kieli::ast::describe_conditional_source(Conditional_source const source) -> std::string_view
{
    switch (source) {
    case Conditional_source::if_:             return "if expression";
    case Conditional_source::elif:            return "elif expression";
    case Conditional_source::while_loop_body: return "while loop body";
    default:                                  cpputil::unreachable();
    }
}
