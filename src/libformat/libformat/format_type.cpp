#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

using namespace libformat;

namespace {
    struct Type_format_visitor {
        State& state;

        auto operator()(cst::Wildcard const& wildcard)
        {
            format(state, wildcard);
        }

        auto operator()(cst::Path const& path)
        {
            format(state, path);
        }

        auto operator()(cst::type::Never const&)
        {
            format(state, "!");
        }

        auto operator()(cst::type::Parenthesized const& parenthesized)
        {
            format(state, "(");
            format(state, parenthesized.type.value);
            format(state, ")");
        }

        auto operator()(cst::type::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.field_types.value.elements);
            format(state, ")");
        }

        auto operator()(cst::type::Reference const& reference)
        {
            format(state, "&");
            format_mutability_with_whitespace(state, reference.mutability);
            format(state, reference.referenced_type);
        }

        auto operator()(cst::type::Pointer const& pointer)
        {
            format(state, "*");
            format_mutability_with_whitespace(state, pointer.mutability);
            format(state, pointer.pointee_type);
        }

        auto operator()(cst::type::Function const& function)
        {
            format(state, "fn(");
            format_comma_separated(state, function.parameter_types.value.elements);
            format(state, ")");
            format(state, function.return_type);
        }

        auto operator()(cst::type::Array const& array)
        {
            format(state, "[");
            format(state, array.element_type);
            format(state, "; ");
            format(state, array.length);
            format(state, "]");
        }

        auto operator()(cst::type::Slice const& slice)
        {
            format(state, "[");
            format(state, slice.element_type.value);
            format(state, "]");
        }

        auto operator()(cst::type::Typeof const& typeof_)
        {
            format(state, "typeof(");
            format(state, typeof_.inspected_expression.value);
            format(state, ")");
        }

        auto operator()(cst::type::Implementation const& implementation)
        {
            format(state, "impl ");
            format_separated(state, implementation.concepts.elements, " + ");
        }
    };
} // namespace

auto libformat::format(State& state, cst::Type const& type) -> void
{
    std::visit(Type_format_visitor { state }, type.variant);
}
