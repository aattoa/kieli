#include <libutl/common/utilities.hpp>
#include <libparse/ast.hpp>


auto ast::Mutability::was_explicitly_specified() const noexcept -> bool {
    auto const* const concrete = std::get_if<Concrete>(&value);
    return !concrete || concrete->is_mutable;
}

auto ast::Name::operator==(Name const& other) const noexcept -> bool {
    return identifier == other.identifier;
}
