#include <libutl/common/utilities.hpp>
#include <libparse/cst.hpp>


auto cst::Qualified_name::is_unqualified() const noexcept -> bool {
    return !root_qualifier.has_value() && middle_qualifiers.empty();
}

auto cst::Self_parameter::is_reference() const noexcept -> bool {
    return ampersand_token.has_value();
}
