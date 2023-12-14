#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    template <class T>
    auto ensure_no_duplicates(
        Context& context, std::string_view const description, std::vector<T> const& elements)
    {
        for (auto it = elements.begin(); it != elements.end(); ++it) {
            auto const duplicate = std::ranges::find(it + 1, elements.end(), it->name, &T::name);
            if (duplicate != elements.end()) {
                context.diagnostics().error(
                    {
                        { it->name.source_view, "First specified here" },
                        { duplicate->name.source_view, "Later specified here" },
                    },
                    "More than one definition for {} {}",
                    description,
                    it->name);
            }
        }
    }

    struct Definition_desugaring_visitor {
        Context& context;

        template <class Definition>
        auto definition(
            Definition&& definition, std::optional<cst::Template_parameters> const& parameters)
            const -> ast::Definition::Variant
        {
            if (parameters.has_value()) {
                return ast::definition::Template<std::remove_reference_t<Definition>> {
                    .definition = std::forward<Definition>(definition),
                    .parameters = context.desugar(parameters->value.elements),
                };
            }
            return std::forward<Definition>(definition);
        }

        auto operator()(cst::definition::Function const& function) -> ast::Definition::Variant
        {
            std::vector<ast::Function_parameter> parameters;
            if (function.signature.function_parameters.self_parameter.has_value()) {
                parameters.push_back(context.normalize_self_parameter(
                    *function.signature.function_parameters.self_parameter));
            }

            std::ranges::move(
                std::views::transform(
                    function.signature.function_parameters.normal_parameters.elements,
                    context.desugar()),
                std::back_inserter(parameters));

            // Convert function bodies defined with shorthand syntax into blocks
            ast::Expression function_body = context.desugar(*function.body);
            if (!std::holds_alternative<ast::expression::Block>(function_body.value)) {
                function_body.value = ast::expression::Block {
                    .result_expression = context.wrap(ast::Expression {
                        .value       = std::move(function_body.value),
                        .source_view = function_body.source_view,
                    }),
                };
            }

            return ast::definition::Function {
                .signature {
                    .template_parameters
                    = function.signature.template_parameters.transform(context.desugar())
                          .value_or(std::vector<ast::Template_parameter> {}),
                    .function_parameters = std::move(parameters),
                    .self_parameter
                    = function.signature.function_parameters.self_parameter.transform(
                        context.desugar()),
                    .return_type = function.signature.return_type.transform(context.desugar()),
                    .name        = function.signature.name,
                },
                .body = std::move(function_body),
            };
        }

        auto operator()(cst::definition::Struct const& structure) -> ast::Definition::Variant
        {
            ensure_no_duplicates(context, "member", structure.members.elements);
            auto const desugar_member = [this](cst::definition::Struct::Member const& member) {
                return ast::definition::Struct::Member {
                    .name        = member.name,
                    .type        = context.desugar(member.type),
                    .is_public   = member.is_public,
                    .source_view = member.source_view,
                };
            };
            return definition(
                ast::definition::Struct {
                    .members = std::views::transform(structure.members.elements, desugar_member)
                             | std::ranges::to<std::vector>(),
                    .name = structure.name,
                },
                structure.template_parameters);
        }

        auto operator()(cst::definition::Enum const& enumeration) -> ast::Definition::Variant
        {
            ensure_no_duplicates(context, "constructor", enumeration.constructors.elements);
            auto const desugar_ctor = [this](cst::definition::Enum::Constructor const& ctor) {
                return ast::definition::Enum::Constructor {
                    .name          = ctor.name,
                    .payload_types = ctor.payload_types.transform(context.desugar()),
                    .source_view   = ctor.source_view,
                };
            };
            return definition(
                ast::definition::Enum {
                    .constructors
                    = std::views::transform(enumeration.constructors.elements, desugar_ctor)
                    | std::ranges::to<std::vector>(),
                    .name = enumeration.name,
                },
                enumeration.template_parameters);
        }

        auto operator()(cst::definition::Alias const& alias) -> ast::Definition::Variant
        {
            return definition(
                ast::definition::Alias {
                    .name = alias.name,
                    .type = context.desugar(*alias.type),
                },
                alias.template_parameters);
        }

        auto operator()(cst::definition::Typeclass const& typeclass) -> ast::Definition::Variant
        {
            return definition(
                ast::definition::Typeclass {
                    .function_signatures = context.desugar(typeclass.function_signatures),
                    .type_signatures     = context.desugar(typeclass.type_signatures),
                    .name                = typeclass.name,
                },
                typeclass.template_parameters);
        }

        auto operator()(cst::definition::Implementation const& implementation)
            -> ast::Definition::Variant
        {
            return definition(
                ast::definition::Implementation {
                    .type        = context.desugar(*implementation.self_type),
                    .definitions = context.desugar(implementation.definitions),
                },
                implementation.template_parameters);
        }

        auto operator()(cst::definition::Instantiation const& instantiation)
            -> ast::Definition::Variant
        {
            return definition(
                ast::definition::Instantiation {
                    .typeclass   = context.desugar(instantiation.typeclass),
                    .self_type   = context.desugar(*instantiation.self_type),
                    .definitions = context.desugar(instantiation.definitions),
                },
                instantiation.template_parameters);
        }

        auto operator()(cst::definition::Namespace const& space) -> ast::Definition::Variant
        {
            return definition(
                ast::definition::Namespace {
                    .definitions = context.desugar(space.definitions),
                    .name        = space.name,
                },
                space.template_parameters);
        }
    };
} // namespace

auto libdesugar::Context::desugar(cst::Definition const& definition) -> ast::Definition
{
    utl::always_assert(!definition.value.valueless_by_exception());
    return {
        .value = std::visit<ast::Definition::Variant>(
            Definition_desugaring_visitor { *this }, definition.value),
        .source_view = definition.source_view,
    };
}
