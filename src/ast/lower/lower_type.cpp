#include "utl/utilities.hpp"
#include "lowering_internals.hpp"


namespace {

    struct Type_lowering_visitor {
        Lowering_context& context;
        ast::Type  const& this_type;

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
            return hir::type::Typename { context.lower(name.name) };
        }
        auto operator()(ast::type::Tuple const& tuple) -> hir::Type::Variant {
            return hir::type::Tuple {
                .field_types = utl::map(context.lower())(tuple.field_types)
            };
        }
        auto operator()(ast::type::Array const& array) -> hir::Type::Variant {
            return hir::type::Array {
                .element_type = context.lower(array.element_type),
                .array_length = context.lower(array.array_length)
            };
        }
        auto operator()(ast::type::Slice const& slice) -> hir::Type::Variant {
            return hir::type::Slice {
                .element_type = context.lower(slice.element_type)
            };
        }
        auto operator()(ast::type::Function const& function) -> hir::Type::Variant {
            return hir::type::Function {
                .argument_types = utl::map(context.lower())(function.argument_types),
                .return_type    = context.lower(function.return_type)
            };
        }
        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type::Variant {
            return hir::type::Typeof {
                .inspected_expression = context.lower(typeof_.inspected_expression)
            };
        }
        auto operator()(ast::type::Reference const& reference) -> hir::Type::Variant {
            return hir::type::Reference {
                .referenced_type = context.lower(reference.referenced_type),
                .mutability      = reference.mutability
            };
        }
        auto operator()(ast::type::Pointer const& pointer) -> hir::Type::Variant {
            return hir::type::Pointer {
                .pointed_to_type = context.lower(pointer.pointed_to_type),
                .mutability      = pointer.mutability
            };
        }
        auto operator()(ast::type::Instance_of const& instance_of) -> hir::Type::Variant {
            if (context.current_function_implicit_template_parameters) {
                // Within a function's parameter list or return type, inst types
                // are converted into references to implicit template parameters.

                auto const tag = context.fresh_name_tag();

                context.current_function_implicit_template_parameters->emplace_back(
                    utl::map(context.lower())(instance_of.classes),
                    hir::Implicit_template_parameter::Tag { tag }
                );

                return hir::type::Implicit_parameter_reference {
                    .tag = hir::Implicit_template_parameter::Tag { tag } };
            }
            else if (context.is_within_function()) {
                // Within a function body, inst types are simply used for constraint collection.
                return hir::type::Instance_of {
                    .classes = utl::map(context.lower())(instance_of.classes) };
            }
            else {
                context.error(this_type.source_view, { "'inst' types are only usable within functions" });
            }
        }
        auto operator()(ast::type::Template_application const& application) -> hir::Type::Variant {
            return hir::type::Template_application {
                .arguments = utl::map(context.lower())(application.arguments),
                .name      = context.lower(application.name)
            };
        }
    };

}


auto Lowering_context::lower(ast::Type const& type) -> hir::Type {
    return {
        .value = std::visit(
            Type_lowering_visitor {
                .context   = *this,
                .this_type = type
            },
            type.value
        ),
        .source_view = type.source_view
    };
}