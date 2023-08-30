#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    struct Type_desugaring_visitor {
        Desugar_context& context;
        cst::Type const& this_type;

        auto operator()(cst::type::Parenthesized const& parenthesized) -> ast::Type::Variant
        {
            return utl::match(parenthesized.type.value->value, *this);
        }

        auto operator()(compiler::built_in_type::Integer const& integer) -> ast::Type::Variant
        {
            return integer;
        }

        auto operator()(compiler::built_in_type::String const& string) -> ast::Type::Variant
        {
            return string;
        }

        auto operator()(compiler::built_in_type::Floating const& floating) -> ast::Type::Variant
        {
            return floating;
        }

        auto operator()(compiler::built_in_type::Character const& character) -> ast::Type::Variant
        {
            return character;
        }

        auto operator()(compiler::built_in_type::Boolean const& boolean) -> ast::Type::Variant
        {
            return boolean;
        }

        auto operator()(cst::type::Wildcard const&) -> ast::Type::Variant
        {
            return ast::type::Wildcard {};
        }

        auto operator()(cst::type::Self const&) -> ast::Type::Variant
        {
            return ast::type::Self {};
        }

        auto operator()(cst::type::Typename const& name) -> ast::Type::Variant
        {
            return ast::type::Typename {
                .name = context.desugar(name.name),
            };
        }

        auto operator()(cst::type::Tuple const& tuple) -> ast::Type::Variant
        {
            return ast::type::Tuple {
                .field_types = utl::map(context.deref_desugar(), tuple.field_types.value.elements),
            };
        }

        auto operator()(cst::type::Array const& array) -> ast::Type::Variant
        {
            return ast::type::Array {
                .element_type = context.desugar(array.element_type),
                .array_length = context.desugar(array.array_length),
            };
        }

        auto operator()(cst::type::Slice const& slice) -> ast::Type::Variant
        {
            return ast::type::Slice {
                .element_type = context.desugar(slice.element_type.value),
            };
        }

        auto operator()(cst::type::Function const& function) -> ast::Type::Variant
        {
            return ast::type::Function {
                .argument_types
                = utl::map(context.deref_desugar(), function.parameter_types.value.elements),
                .return_type = context.wrap(context.desugar(function.return_type)),
            };
        }

        auto operator()(cst::type::Typeof const& typeof_) -> ast::Type::Variant
        {
            return ast::type::Typeof {
                .inspected_expression = context.desugar(typeof_.inspected_expression.value),
            };
        }

        auto operator()(cst::type::Reference const& reference) -> ast::Type::Variant
        {
            return ast::type::Reference {
                .referenced_type = context.desugar(reference.referenced_type),
                .mutability      = context.desugar_mutability(
                    reference.mutability, reference.ampersand_token.source_view),
            };
        }

        auto operator()(cst::type::Pointer const& pointer) -> ast::Type::Variant
        {
            return ast::type::Pointer {
                .pointed_to_type = context.desugar(pointer.pointed_to_type),
                .mutability      = context.desugar_mutability(
                    pointer.mutability, pointer.asterisk_token.source_view),
            };
        }

        auto operator()(cst::type::Instance_of const& instance_of) -> ast::Type::Variant
        {
            return ast::type::Instance_of { .classes
                                            = context.desugar(instance_of.classes.elements) };
        }

        auto operator()(cst::type::Template_application const& application) -> ast::Type::Variant
        {
            return ast::type::Template_application {
                .arguments = context.desugar(application.template_arguments.value.elements),
                .name      = context.desugar(application.name),
            };
        }
    };
} // namespace

auto libdesugar::Desugar_context::desugar(cst::Type const& type) -> ast::Type
{
    return {
        .value = std::visit(
            Type_desugaring_visitor {
                *this,
                type,
            },
            type.value),
        .source_view = type.source_view,
    };
}
