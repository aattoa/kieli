#include <libutl/common/utilities.hpp>
#include <libdesugar/ast.hpp>

auto ast::Qualified_name::is_upper() const noexcept -> bool
{
    return primary_name.is_upper.get();
}

auto ast::Qualified_name::is_unqualified() const noexcept -> bool
{
    return !root_qualifier.has_value() && middle_qualifiers.empty();
}
