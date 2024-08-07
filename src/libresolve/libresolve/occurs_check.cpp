#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    struct Occurs_check_visitor {
        hir::Arena const&     arena;
        hir::Type_variable_id id;

        [[nodiscard]] auto recurse(hir::Type_id const type_id) const -> bool
        {
            return occurs_check(arena, id, arena.types[type_id]);
        }

        [[nodiscard]] auto recurse(hir::Type const type) const -> bool
        {
            return recurse(type.id);
        }

        [[nodiscard]] auto recurse() const
        {
            return [*this](auto const type) -> bool { return recurse(type); };
        }

        auto operator()(hir::type::Variable const& variable) const -> bool
        {
            return id == variable.id;
        }

        auto operator()(hir::type::Array const& array) const -> bool
        {
            return recurse(array.element_type) || recurse(arena.expressions[array.length].type);
        }

        auto operator()(hir::type::Slice const& slice) const -> bool
        {
            return recurse(slice.element_type);
        }

        auto operator()(hir::type::Reference const& reference) const -> bool
        {
            return recurse(reference.referenced_type);
        }

        auto operator()(hir::type::Pointer const& pointer) const -> bool
        {
            return recurse(pointer.pointee_type);
        }

        auto operator()(hir::type::Function const& function) const -> bool
        {
            return recurse(function.return_type)
                || std::ranges::any_of(function.parameter_types, recurse());
        }

        auto operator()(hir::type::Tuple const& tuple) const -> bool
        {
            return std::ranges::any_of(tuple.types, recurse());
        }

        auto operator()(hir::type::Enumeration const& enumeration) const -> bool
        {
            (void)enumeration;
            cpputil::todo(); // recurse on template arguments
        }

        auto operator()(utl::one_of<
                        kieli::type::Integer,
                        kieli::type::Floating,
                        kieli::type::Character,
                        kieli::type::Boolean,
                        kieli::type::String,
                        hir::type::Parameterized,
                        hir::type::Error> auto const&) const -> bool
        {
            return false;
        }
    };
} // namespace

auto libresolve::occurs_check(
    hir::Arena const& arena, hir::Type_variable_id const id, hir::Type_variant const& type) -> bool
{
    return std::visit(Occurs_check_visitor { arena, id }, type);
}
