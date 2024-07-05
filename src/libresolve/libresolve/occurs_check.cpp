#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    struct Occurs_check_visitor {
        hir::Type_variable_tag tag;

        [[nodiscard]] auto recurse(hir::Type const type) const -> bool
        {
            return libresolve::occurs_check(tag, *type.variant);
        }

        [[nodiscard]] auto recurse() const
        {
            return [*this](hir::Type const type) -> bool { return recurse(type); };
        }

        auto operator()(hir::type::Variable const& variable) const -> bool
        {
            return tag == variable.tag;
        }

        auto operator()(hir::type::Array const& array) const -> bool
        {
            return recurse(array.element_type) || recurse(array.length->type);
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

auto libresolve::occurs_check(hir::Type_variable_tag tag, hir::Type::Variant const& type) -> bool
{
    return std::visit(Occurs_check_visitor { tag }, type);
}
