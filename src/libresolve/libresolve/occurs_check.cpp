#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    struct Occurs_check_visitor {
        Unification_state const& state;
        hir::Type_variable_tag   tag;

        [[nodiscard]] auto recurse() const
        {
            return std::bind_front(occurs_check, std::ref(state), tag);
        }

        auto operator()(hir::type::Variable const& variable) const -> bool
        {
            return std::visit(
                utl::Overload {
                    [&](hir::Type_variable_state::Solved const& solved) -> bool {
                        return occurs_check(state, tag, solved.solution);
                    },
                    [&](hir::Type_variable_state::Unsolved const& unsolved) -> bool {
                        return tag == unsolved.tag;
                    },
                },
                state.type_states.at(variable.tag.value).variant);
        }

        auto operator()(hir::type::Array const& array) const -> bool
        {
            return occurs_check(state, tag, array.element_type)
                || occurs_check(state, tag, array.length->type);
        }

        auto operator()(hir::type::Slice const& slice) const -> bool
        {
            return occurs_check(state, tag, slice.element_type);
        }

        auto operator()(hir::type::Reference const& reference) const -> bool
        {
            return occurs_check(state, tag, reference.referenced_type);
        }

        auto operator()(hir::type::Pointer const& pointer) const -> bool
        {
            return occurs_check(state, tag, pointer.pointee_type);
        }

        auto operator()(hir::type::Function const& function) const -> bool
        {
            return occurs_check(state, tag, function.return_type)
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
                        kieli::built_in_type::Integer,
                        kieli::built_in_type::Floating,
                        kieli::built_in_type::Character,
                        kieli::built_in_type::Boolean,
                        kieli::built_in_type::String,
                        hir::type::Parameterized,
                        hir::type::Error> auto const&) const -> bool
        {
            return false;
        }
    };
} // namespace

auto libresolve::occurs_check(
    Unification_state const& state, hir::Type_variable_tag const tag, hir::Type const type) -> bool
{
    return std::visit(Occurs_check_visitor { state, tag }, *type.variant);
}
