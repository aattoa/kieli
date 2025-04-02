#include <libutl/utilities.hpp>
#include <libcompiler/ast/ast.hpp>

auto ki::ast::Path::head() const -> Path_segment const&
{
    cpputil::always_assert(not segments.empty());
    return segments.back();
}

auto ki::ast::Path::is_unqualified() const noexcept -> bool
{
    return std::holds_alternative<std::monostate>(root) and segments.size() == 1;
}

auto ki::ast::describe_loop_source(Loop_source const source) -> std::string_view
{
    switch (source) {
    case Loop_source::Plain_loop: return "plain loop";
    case Loop_source::While_loop: return "while loop";
    case Loop_source::For_loop:   return "for loop";
    default:                      cpputil::unreachable();
    }
}

auto ki::ast::describe_conditional_source(Conditional_source const source) -> std::string_view
{
    switch (source) {
    case Conditional_source::If:    return "if expression";
    case Conditional_source::Elif:  return "elif expression";
    case Conditional_source::While: return "while loop body";
    default:                        cpputil::unreachable();
    }
}
