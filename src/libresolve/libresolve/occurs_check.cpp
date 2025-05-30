#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        hir::Arena const&     arena;
        hir::Type_variable_id id;

        [[nodiscard]] auto recurse(hir::Type_id type_id) const -> bool
        {
            return occurs_check(arena, id, arena.types[type_id]);
        }

        [[nodiscard]] auto recurse(hir::Type type) const -> bool
        {
            return recurse(type.id);
        }

        [[nodiscard]] auto recurse() const
        {
            return [*this](hir::Type type) -> bool { return recurse(type); };
        }

        auto operator()(hir::type::Variable const& variable) const -> bool
        {
            return id == variable.id;
        }

        auto operator()(hir::type::Array const& array) const -> bool
        {
            return recurse(array.element_type) or recurse(arena.expressions[array.length].type_id);
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
                or std::ranges::any_of(function.parameter_types, recurse());
        }

        auto operator()(hir::type::Tuple const& tuple) const -> bool
        {
            return std::ranges::any_of(tuple.types, recurse());
        }

        auto operator()(hir::type::Structure const&) const -> bool
        {
            return false; // TODO
        }

        auto operator()(hir::type::Enumeration const&) const -> bool
        {
            return false; // TODO
        }

        auto operator()(utl::one_of<
                        db::Error,
                        hir::type::Integer,
                        hir::type::Floating,
                        hir::type::Character,
                        hir::type::Boolean,
                        hir::type::String,
                        hir::type::Parameterized> auto const&) const -> bool
        {
            return false;
        }
    };
} // namespace

auto ki::res::occurs_check(
    hir::Arena const& arena, hir::Type_variable_id id, hir::Type_variant const& type) -> bool
{
    return std::visit(Visitor { .arena = arena, .id = id }, type);
}
