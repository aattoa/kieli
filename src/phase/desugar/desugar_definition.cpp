#include "utl/utilities.hpp"
#include "desugaring_internals.hpp"


namespace {

    auto desugar_self_parameter(ast::Self_parameter const& parameter, Desugaring_context& context)
        -> hir::Function_parameter
    {
        hir::Type self_type {
            .value       = hir::type::Self {},
            .source_view = parameter.source_view
        };
        if (parameter.is_reference.get()) {
            self_type = hir::Type {
                .value = hir::type::Reference {
                    .referenced_type = context.wrap(std::move(self_type)),
                    .mutability      = parameter.mutability
                },
                .source_view = parameter.source_view
            };
        }

        hir::Pattern self_pattern {
            .value = hir::pattern::Name {
                .identifier = context.self_variable_identifier,
                .mutability = parameter.is_reference.get()
                    ? ast::Mutability { ast::Mutability::Concrete { .is_mutable = false }, parameter.source_view }
                    : parameter.mutability
            },
            .source_view = parameter.source_view
        };

        return hir::Function_parameter {
            .pattern = std::move(self_pattern),
            .type    = std::move(self_type)
        };
    }


    struct Definition_desugaring_visitor {
        Desugaring_context& context;

        auto operator()(ast::definition::Function const& function) -> hir::definition::Function {
            std::vector<hir::Function_parameter> parameters;
            if (function.self_parameter.has_value())
                parameters.push_back(desugar_self_parameter(*function.self_parameter, context));

            ranges::move(
                ranges::views::transform(function.parameters, context.desugar()),
                std::back_inserter(parameters));

            // Convert function bodies defined with shorthand syntax into blocks
            hir::Expression function_body = context.desugar(function.body);
            if (!std::holds_alternative<hir::expression::Block>(function_body.value)) {
                function_body.value = hir::expression::Block {
                    .result_expression = context.wrap(hir::Expression {
                        .value       = std::move(function_body.value),
                        .source_view = function_body.source_view
                    })
                };
            }

            return {
                .body           = std::move(function_body),
                .parameters     = std::move(parameters),
                .name           = function.name,
                .return_type    = function.return_type.transform(context.desugar()),
                .self_parameter = function.self_parameter
            };
        }

        auto operator()(ast::definition::Struct const& structure) -> hir::definition::Struct {
            auto const desugar_member = [this](ast::definition::Struct::Member const& member)
                -> hir::definition::Struct::Member
            {
                return {
                    .name        = member.name,
                    .type        = context.desugar(member.type),
                    .is_public   = member.is_public,
                    .source_view = member.source_view
                };
            };

            return {
                .members = utl::map(desugar_member, structure.members),
                .name    = structure.name,
            };
        }

        auto operator()(ast::definition::Enum const& enumeration) -> hir::definition::Enum {
            auto const desugar_constructor = [this](ast::definition::Enum::Constructor const& ctor)
                -> hir::definition::Enum::Constructor
            {
                return {
                    .name         = ctor.name,
                    .payload_type = ctor.payload_type.transform(context.desugar()),
                    .source_view  = ctor.source_view
                };
            };

            return {
                .constructors = utl::map(desugar_constructor, enumeration.constructors),
                .name         = enumeration.name,
            };
        }

        auto operator()(ast::definition::Alias const& alias) -> hir::definition::Alias {
            return {
                .name = alias.name,
                .type = context.desugar(alias.type),
            };
        }

        auto operator()(ast::definition::Typeclass const& typeclass) -> hir::definition::Typeclass {
            return {
                .function_signatures          = utl::map(context.desugar(), typeclass.function_signatures),
                .function_template_signatures = utl::map(context.desugar(), typeclass.function_template_signatures),
                .type_signatures              = utl::map(context.desugar(), typeclass.type_signatures),
                .type_template_signatures     = utl::map(context.desugar(), typeclass.type_template_signatures),
                .name                         = typeclass.name,
            };
        }

        auto operator()(ast::definition::Implementation const& implementation) -> hir::definition::Implementation {
            return {
                .type        = context.desugar(implementation.type),
                .definitions = utl::map(context.desugar(), implementation.definitions),
            };
        }

        auto operator()(ast::definition::Instantiation const& instantiation) -> hir::definition::Instantiation {
            return {
                .typeclass   = context.desugar(instantiation.typeclass),
                .self_type   = context.desugar(instantiation.self_type),
                .definitions = utl::map(context.desugar(), instantiation.definitions),
            };
        }

        auto operator()(ast::definition::Namespace const& space) -> hir::definition::Namespace {
            return {
                .definitions = utl::map(context.desugar(), space.definitions),
                .name        = space.name,
            };
        }

        template <class Definition>
        auto operator()(ast::definition::Template<Definition> const& template_definition) {
            return ast::definition::Template<decltype(operator()(template_definition.definition))> {
                .definition = operator()(template_definition.definition),
                .parameters = utl::map(context.desugar(), template_definition.parameters)
            };
        }
    };

}


auto Desugaring_context::desugar(ast::Definition const& definition) -> hir::Definition {
    utl::always_assert(!definition.value.valueless_by_exception());
    return {
        std::visit<hir::Definition::Variant>(Definition_desugaring_visitor { *this }, definition.value),
        definition.source_view,
    };
}
