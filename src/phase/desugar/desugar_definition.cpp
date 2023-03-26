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
        if (parameter.is_reference) {
            self_type = hir::Type {
                .value = hir::type::Reference {
                    .referenced_type = utl::wrap(std::move(self_type)),
                    .mutability      = parameter.mutability
                },
                .source_view = parameter.source_view
            };
        }

        hir::Pattern self_pattern {
            .value = hir::pattern::Name {
                .identifier = context.self_variable_identifier,
                .mutability = parameter.is_reference
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
            utl::always_assert(context.current_function_implicit_template_parameters == nullptr);
            std::vector<hir::Implicit_template_parameter> implicit_template_parameters;
            context.current_function_implicit_template_parameters = &implicit_template_parameters;

            // The parameters must be desugared first in order to collect the implicit template parameters.

            std::vector<hir::Function_parameter> parameters;
            if (function.self_parameter.has_value()) {
                parameters.push_back(desugar_self_parameter(*function.self_parameter, context));
            }
            auto explicit_parameters = utl::map(context.desugar(), function.parameters);
            parameters.insert(parameters.end(),
                std::move_iterator { explicit_parameters.begin() },
                std::move_iterator { explicit_parameters.end() });

            context.current_function_implicit_template_parameters = nullptr;

            return {
                .implicit_template_parameters = std::move(implicit_template_parameters),
                .parameters                   = std::move(parameters),
                .return_type                  = function.return_type.transform(context.desugar()),
                .body                         = context.desugar(function.body),
                .name                         = function.name,
                .self_parameter               = function.self_parameter
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

    utl::Usize const old_kind = std::exchange(current_definition_kind, definition.value.index());
    auto hir_definition = std::visit<hir::Definition::Variant>(Definition_desugaring_visitor { .context = *this }, definition.value);
    current_definition_kind = old_kind;

    return { std::move(hir_definition), definition.source_view };
}
