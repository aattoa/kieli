#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;


namespace {
    struct Type_desugaring_visitor {
        Desugar_context& context;
        cst::Type const& this_type;

        auto operator()(cst::type::Parenthesized const& parenthesized) -> hir::Type::Variant {
            return utl::match(parenthesized.type.value->value, *this);
        }
        auto operator()(compiler::built_in_type::Integer   const& integer) -> hir::Type::Variant { return integer; }
        auto operator()(compiler::built_in_type::String    const& string) -> hir::Type::Variant { return string; }
        auto operator()(compiler::built_in_type::Floating  const& floating) -> hir::Type::Variant { return floating; }
        auto operator()(compiler::built_in_type::Character const& character) -> hir::Type::Variant { return character; }
        auto operator()(compiler::built_in_type::Boolean   const& boolean) -> hir::Type::Variant { return boolean; }
        auto operator()(cst::type::Wildcard const&) -> hir::Type::Variant {
            return hir::type::Wildcard {};
        }
        auto operator()(cst::type::Self const&) -> hir::Type::Variant {
            return hir::type::Self {};
        }
        auto operator()(cst::type::Typename const& name) -> hir::Type::Variant {
            return hir::type::Typename {
                .name = context.desugar(name.name),
            };
        }
        auto operator()(cst::type::Tuple const& tuple) -> hir::Type::Variant {
            return hir::type::Tuple {
                .field_types = utl::map(context.deref_desugar(), tuple.field_types.value.elements),
            };
        }
        auto operator()(cst::type::Array const& array) -> hir::Type::Variant {
            return hir::type::Array {
                .element_type = context.desugar(array.element_type),
                .array_length = context.desugar(array.array_length),
            };
        }
        auto operator()(cst::type::Slice const& slice) -> hir::Type::Variant {
            return hir::type::Slice {
                .element_type = context.desugar(slice.element_type.value),
            };
        }
        auto operator()(cst::type::Function const& function) -> hir::Type::Variant {
            return hir::type::Function {
                .argument_types = utl::map(context.deref_desugar(), function.parameter_types.value.elements),
                .return_type    = context.wrap(context.desugar(function.return_type)),
            };
        }
        auto operator()(cst::type::Typeof const& typeof_) -> hir::Type::Variant {
            return hir::type::Typeof {
                .inspected_expression = context.desugar(typeof_.inspected_expression.value),
            };
        }
        auto operator()(cst::type::Reference const& reference) -> hir::Type::Variant {
            return hir::type::Reference {
                .referenced_type = context.desugar(reference.referenced_type),
                .mutability      = context.desugar_mutability(reference.mutability, reference.ampersand_token.source_view),
            };
        }
        auto operator()(cst::type::Pointer const& pointer) -> hir::Type::Variant {
            return hir::type::Pointer {
                .pointed_to_type = context.desugar(pointer.pointed_to_type),
                .mutability      = context.desugar_mutability(pointer.mutability, pointer.asterisk_token.source_view),
            };
        }
        auto operator()(cst::type::Instance_of const& instance_of) -> hir::Type::Variant {
            return hir::type::Instance_of { .classes = utl::map(context.desugar(), instance_of.classes.elements) };
        }
        auto operator()(cst::type::Template_application const& application) -> hir::Type::Variant {
            return hir::type::Template_application {
                .arguments = utl::map(context.desugar(), application.template_arguments.value.elements),
                .name      = context.desugar(application.name),
            };
        }
    };
}


auto libdesugar::Desugar_context::desugar(cst::Type const& type) -> hir::Type {
    return {
        .value = std::visit(Type_desugaring_visitor { *this, type, }, type.value),
        .source_view = type.source_view,
    };
}
