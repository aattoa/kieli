#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>

namespace {
    struct Type_format_visitor {
        libformat::State& state;

        auto operator()(kieli::built_in_type::Integer const integer)
        {
            state.format("{}", kieli::built_in_type::integer_name(integer));
        }

        auto operator()(kieli::built_in_type::Floating const&)
        {
            state.format("Float");
        }

        auto operator()(kieli::built_in_type::Character const&)
        {
            state.format("Char");
        }

        auto operator()(kieli::built_in_type::Boolean const&)
        {
            state.format("Bool");
        }

        auto operator()(kieli::built_in_type::String const&)
        {
            state.format("String");
        }

        auto operator()(cst::Wildcard const& wildcard)
        {
            state.format(wildcard);
        }

        auto operator()(cst::type::Self const&)
        {
            state.format("Self");
        }

        auto operator()(cst::type::Typename const& type)
        {
            state.format(type.name);
        }

        auto operator()(cst::type::Template_application const& application)
        {
            state.format(application.name);
            state.format(application.template_arguments);
        }

        auto operator()(cst::type::Parenthesized const& parenthesized)
        {
            state.format("(");
            state.format(parenthesized.type.value);
            state.format(")");
        }

        auto operator()(cst::type::Tuple const& tuple)
        {
            state.format("(");
            state.format_comma_separated(tuple.field_types.value.elements);
            state.format(")");
        }

        auto operator()(cst::type::Reference const& reference)
        {
            state.format("&");
            state.format_mutability_with_trailing_whitespace(reference.mutability);
            state.format(reference.referenced_type);
        }

        auto operator()(cst::type::Pointer const& pointer)
        {
            state.format("*");
            state.format_mutability_with_trailing_whitespace(pointer.mutability);
            state.format(pointer.pointee_type);
        }

        auto operator()(cst::type::Function const& function)
        {
            state.format("fn(");
            state.format_comma_separated(function.parameter_types.value.elements);
            state.format("): ");
            state.format(function.return_type.type);
        }

        auto operator()(cst::type::Array const& array)
        {
            state.format("[");
            state.format(array.element_type);
            state.format("; ");
            state.format(array.array_length);
            state.format("]");
        }

        auto operator()(cst::type::Slice const& slice)
        {
            state.format("[");
            state.format(slice.element_type.value);
            state.format("]");
        }

        auto operator()(cst::type::Typeof const& typeof_)
        {
            state.format("typeof(");
            state.format(typeof_.inspected_expression.value);
            state.format(")");
        }

        auto operator()(cst::type::Instance_of const& instance)
        {
            state.format("inst ");
            state.format_separated(instance.classes.elements, " + ");
        }
    };
} // namespace

auto libformat::State::format(cst::Type const& type) -> void
{
    std::visit(Type_format_visitor { *this }, type.value);
}
