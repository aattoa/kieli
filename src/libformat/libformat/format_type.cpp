#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

using namespace ki::format;

namespace {
    struct Type_format_visitor {
        State& state;

        void operator()(cst::Wildcard const& wildcard)
        {
            format(state, wildcard);
        }

        void operator()(cst::Path const& path)
        {
            format(state, path);
        }

        void operator()(cst::type::Never const&)
        {
            format(state, "!");
        }

        void operator()(cst::type::Paren const& paren)
        {
            format(state, "(");
            format(state, paren.type.value);
            format(state, ")");
        }

        void operator()(cst::type::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.field_types.value.elements);
            format(state, ")");
        }

        void operator()(cst::type::Reference const& reference)
        {
            format(state, "&");
            format_mutability_with_whitespace(state, reference.mutability);
            format(state, reference.referenced_type);
        }

        void operator()(cst::type::Pointer const& pointer)
        {
            format(state, "*");
            format_mutability_with_whitespace(state, pointer.mutability);
            format(state, pointer.pointee_type);
        }

        void operator()(cst::type::Function const& function)
        {
            format(state, "fn(");
            format_comma_separated(state, function.parameter_types.value.elements);
            format(state, ")");
            format(state, function.return_type);
        }

        void operator()(cst::type::Array const& array)
        {
            format(state, "[");
            format(state, array.element_type);
            format(state, "; ");
            format(state, array.length);
            format(state, "]");
        }

        void operator()(cst::type::Slice const& slice)
        {
            format(state, "[");
            format(state, slice.element_type.value);
            format(state, "]");
        }

        void operator()(cst::type::Typeof const& typeof_)
        {
            format(state, "typeof(");
            format(state, typeof_.expression.value);
            format(state, ")");
        }

        void operator()(cst::type::Impl const& implementation)
        {
            format(state, "impl ");
            format_separated(state, implementation.concepts.elements, " + ");
        }
    };
} // namespace

void ki::format::format(State& state, cst::Type const& type)
{
    std::visit(Type_format_visitor { state }, type.variant);
}
