#include "utl/utilities.hpp"
#include "desugaring_internals.hpp"


namespace {
    struct Type_desugaring_visitor {
        Desugaring_context& context;
        ast::Type    const& this_type;

        template <class T>
        auto operator()(ast::type::Primitive<T> const& primitive) -> hir::Type::Variant {
            return primitive;
        }
        auto operator()(ast::type::Integer const& integer) -> hir::Type::Variant {
            return integer;
        }
        auto operator()(ast::type::Wildcard const&) -> hir::Type::Variant {
            return hir::type::Wildcard {};
        }
        auto operator()(ast::type::Self const& self) -> hir::Type::Variant {
            return self;
        }
        auto operator()(ast::type::Typename const& name) -> hir::Type::Variant {
            return hir::type::Typename { context.desugar(name.name) };
        }
        auto operator()(ast::type::Tuple const& tuple) -> hir::Type::Variant {
            return hir::type::Tuple {
                .field_types = utl::map(context.desugar(), tuple.field_types)
            };
        }
        auto operator()(ast::type::Array const& array) -> hir::Type::Variant {
            return hir::type::Array {
                .element_type = context.desugar(array.element_type),
                .array_length = context.desugar(array.array_length)
            };
        }
        auto operator()(ast::type::Slice const& slice) -> hir::Type::Variant {
            return hir::type::Slice {
                .element_type = context.desugar(slice.element_type)
            };
        }
        auto operator()(ast::type::Function const& function) -> hir::Type::Variant {
            return hir::type::Function {
                .argument_types = utl::map(context.desugar(), function.argument_types),
                .return_type    = context.desugar(function.return_type)
            };
        }
        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type::Variant {
            return hir::type::Typeof {
                .inspected_expression = context.desugar(typeof_.inspected_expression)
            };
        }
        auto operator()(ast::type::Reference const& reference) -> hir::Type::Variant {
            return hir::type::Reference {
                .referenced_type = context.desugar(reference.referenced_type),
                .mutability      = reference.mutability
            };
        }
        auto operator()(ast::type::Pointer const& pointer) -> hir::Type::Variant {
            return hir::type::Pointer {
                .pointed_to_type = context.desugar(pointer.pointed_to_type),
                .mutability      = pointer.mutability
            };
        }
        auto operator()(ast::type::Instance_of const& instance_of) -> hir::Type::Variant {
            return hir::type::Instance_of { .classes = utl::map(context.desugar(), instance_of.classes) };
        }
        auto operator()(ast::type::Template_application const& application) -> hir::Type::Variant {
            return hir::type::Template_application {
                .arguments = utl::map(context.desugar(), application.arguments),
                .name      = context.desugar(application.name)
            };
        }
    };
}


auto Desugaring_context::desugar(ast::Type const& type) -> hir::Type {
    return {
        .value = std::visit(
            Type_desugaring_visitor {
                .context   = *this,
                .this_type = type
            },
            type.value
        ),
        .source_view = type.source_view
    };
}
