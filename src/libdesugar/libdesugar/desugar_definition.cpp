#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    template <class T>
    auto ensure_no_duplicates(
        Context&                   context,
        utl::Source::Wrapper const source,
        std::string_view const     description,
        std::vector<T> const&      elements)
    {
        for (auto it = elements.begin(); it != elements.end(); ++it) {
            auto const duplicate = std::ranges::find(it + 1, elements.end(), it->name, &T::name);
            if (duplicate != elements.end()) {
                context.diagnostics().error(
                    {
                        { source, it->name.source_range, "First specified here" },
                        { source, duplicate->name.source_range, "Later specified here" },
                    },
                    "More than one definition for {} {}",
                    description,
                    it->name);
            }
        }
    }

    struct Definition_desugaring_visitor {
        Context&             context;
        utl::Source::Wrapper source;

        auto operator()(cst::definition::Function const& function) -> ast::Definition::Variant
        {
            std::vector<ast::Function_parameter> parameters;
            if (function.signature.function_parameters.value.self_parameter.has_value()) {
                parameters.push_back(context.normalize_self_parameter(
                    function.signature.function_parameters.value.self_parameter.value()));
            }

            parameters.append_range(
                function.signature.function_parameters.value.normal_parameters.elements
                | std::views::transform(context.desugar()));

            // Convert function bodies defined with shorthand syntax into blocks
            ast::Expression function_body = context.desugar(*function.body);
            if (!std::holds_alternative<ast::expression::Block>(function_body.value)) {
                function_body.value = ast::expression::Block {
                    .result_expression = context.wrap(ast::Expression {
                        .value        = std::move(function_body.value),
                        .source_range = function_body.source_range,
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
                    = function.signature.function_parameters.value.self_parameter.transform(
                        context.desugar()),
                    .return_type = function.signature.return_type.transform(context.desugar()),
                    .name        = function.signature.name,
                },
                .body = std::move(function_body),
            };
        }

        auto operator()(cst::definition::Struct const& structure) -> ast::Definition::Variant
        {
            ensure_no_duplicates(context, source, "member", structure.members.elements);
            auto const desugar_member = [this](cst::definition::Struct::Member const& member) {
                return ast::definition::Struct::Member {
                    .name         = member.name,
                    .type         = context.desugar(member.type),
                    .source_range = member.source_range,
                };
            };
            return ast::definition::Struct {
                .members = std::views::transform(structure.members.elements, desugar_member)
                         | std::ranges::to<std::vector>(),
                .name                = structure.name,
                .template_parameters = structure.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Enum const& enumeration) -> ast::Definition::Variant
        {
            ensure_no_duplicates(context, source, "constructor", enumeration.constructors.elements);
            auto const desugar_ctor = [this](cst::definition::Enum::Constructor const& ctor) {
                return ast::definition::Enum::Constructor {
                    .name          = ctor.name,
                    .payload_types = ctor.payload_types.transform(context.desugar()),
                    .source_range  = ctor.source_range,
                };
            };
            return ast::definition::Enum {
                .constructors
                = std::views::transform(enumeration.constructors.elements, desugar_ctor)
                | std::ranges::to<std::vector>(),
                .name                = enumeration.name,
                .template_parameters = enumeration.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Alias const& alias) -> ast::Definition::Variant
        {
            return ast::definition::Alias {
                .name                = alias.name,
                .type                = context.desugar(*alias.type),
                .template_parameters = alias.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Typeclass const& typeclass) -> ast::Definition::Variant
        {
            return ast::definition::Typeclass {
                .function_signatures = context.desugar(typeclass.function_signatures),
                .type_signatures     = context.desugar(typeclass.type_signatures),
                .name                = typeclass.name,
                .template_parameters = typeclass.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Implementation const& implementation)
            -> ast::Definition::Variant
        {
            return ast::definition::Implementation {
                .type        = context.desugar(*implementation.self_type),
                .definitions = context.desugar(implementation.definitions),
                .template_parameters
                = implementation.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Instantiation const& instantiation)
            -> ast::Definition::Variant
        {
            return ast::definition::Instantiation {
                .typeclass   = context.desugar(instantiation.typeclass),
                .self_type   = context.desugar(*instantiation.self_type),
                .definitions = context.desugar(instantiation.definitions),
                .template_parameters
                = instantiation.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Namespace const& space) -> ast::Definition::Variant
        {
            return ast::definition::Namespace {
                .definitions         = context.desugar(space.definitions),
                .name                = space.name,
                .template_parameters = space.template_parameters.transform(context.desugar()),
            };
        }
    };
} // namespace

auto libdesugar::Context::desugar(cst::Definition const& definition) -> ast::Definition
{
    cpputil::always_assert(!definition.value.valueless_by_exception());
    return {
        .value = std::visit<ast::Definition::Variant>(
            Definition_desugaring_visitor { *this, definition.source }, definition.value),
        .source       = definition.source,
        .source_range = definition.source_range,
    };
}
