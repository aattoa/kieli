#include <libutl/utilities.hpp>
#include <libcompiler/ast/ast.hpp>

auto kieli::ast::Path::is_simple_name() const noexcept -> bool
{
    return !root.has_value() && segments.empty();
}
