#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::occurs_check(hir::Unification_variable_tag const tag, hir::Type const type) -> bool
{
    return std::visit(
        utl::Overload {
            [tag](hir::type::Unification_variable const& variable) -> bool {
                auto const unsolved = std::get_if<hir::Unification_type_variable_state::Unsolved>(
                    &variable.state->variant);
                return unsolved && unsolved->tag == tag;
            },
            [tag](hir::type::Array const& array) -> bool {
                return occurs_check(tag, array.element_type)
                    || occurs_check(tag, array.length->type);
            },
            [tag](hir::type::Slice const& slice) -> bool {
                return occurs_check(tag, slice.element_type);
            },
            [tag](hir::type::Reference const& reference) -> bool {
                return occurs_check(tag, reference.referenced_type);
            },
            [tag](hir::type::Pointer const& pointer) -> bool {
                return occurs_check(tag, pointer.pointee_type);
            },
            [tag](hir::type::Function const& function) -> bool {
                return occurs_check(tag, function.return_type)
                    || std::ranges::any_of(
                           function.parameter_types, std::bind_front(occurs_check, tag));
            },
            [tag](hir::type::Tuple const& tuple) -> bool {
                return std::ranges::any_of(tuple.types, std::bind_front(occurs_check, tag));
            },
            [tag](hir::type::Enumeration const& enumeration) -> bool {
                // TODO: recurse on template arguments
                (void)tag;
                (void)enumeration;
                cpputil::todo();
            },
            [](kieli::built_in_type::Integer const&) -> bool { return false; },
            [](kieli::built_in_type::Floating const&) -> bool { return false; },
            [](kieli::built_in_type::Character const&) -> bool { return false; },
            [](kieli::built_in_type::Boolean const&) -> bool { return false; },
            [](kieli::built_in_type::String const&) -> bool { return false; },
            [](hir::type::Parameterized const&) -> bool { return false; },
            [](hir::type::Error const&) -> bool { return false; },
        },
        *type.variant);
}
