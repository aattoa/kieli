#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

namespace {

    using namespace libdesugar;

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

    auto normalize_function_body(Context& context, ast::Expression expression) -> ast::Expression
    {
        if (std::holds_alternative<ast::expression::Block>(expression.variant)) {
            return expression;
        }
        auto const source_range = expression.source_range;
        return ast::Expression {
            .variant = ast::expression::Block {
                .result = context.wrap(std::move(expression)),
            },
            .source_range = source_range,
        };
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

            parameters.append_range(std::views::transform(
                function.signature.function_parameters.value.normal_parameters.elements,
                context.desugar()));

            return ast::definition::Function {
                .signature {
                    .template_parameters
                    = function.signature.template_parameters.transform(context.desugar()),
                    .function_parameters = std::move(parameters),
                    .self_parameter
                    = function.signature.function_parameters.value.self_parameter.transform(
                        context.desugar()),
                    .return_type = function.signature.return_type.transform(context.desugar()),
                    .name        = function.signature.name,
                },
                .body = normalize_function_body(context, context.desugar(*function.body)),
            };
        }

        auto operator()(cst::definition::Struct const& structure) -> ast::Definition::Variant
        {
            return ast::definition::Enumeration {
                .constructors = utl::to_vector({ ast::definition::Constructor {
                    .name = structure.name,
                    .body = context.desugar(structure.body),
                } }),

                .name                = structure.name,
                .template_parameters = structure.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Enum const& enumeration) -> ast::Definition::Variant
        {
            ensure_no_duplicates(context, source, "constructor", enumeration.constructors.elements);
            return ast::definition::Enumeration {
                .constructors        = context.desugar(enumeration.constructors),
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

        auto operator()(cst::definition::Submodule const& submodule) -> ast::Definition::Variant
        {
            return ast::definition::Submodule {
                .definitions         = context.desugar(submodule.definitions),
                .name                = submodule.name,
                .template_parameters = submodule.template_parameters.transform(context.desugar()),
            };
        }
    };

} // namespace

auto libdesugar::Context::desugar(cst::definition::Field const& field) -> ast::definition::Field
{
    return ast::definition::Field {
        .name         = field.name,
        .type         = desugar(field.type),
        .source_range = field.source_range,
    };
};

auto libdesugar::Context::desugar(cst::definition::Constructor_body const& body)
    -> ast::definition::Constructor_body
{
    return std::visit<ast::definition::Constructor_body>(
        utl::Overload {
            [&](cst::definition::Struct_constructor const& constructor) {
                ensure_no_duplicates(*this, source, "field", constructor.fields.value.elements);
                return ast::definition::Struct_constructor {
                    .fields = desugar(constructor.fields),
                };
            },
            [&](cst::definition::Tuple_constructor const& constructor) {
                return ast::definition::Tuple_constructor {
                    .types = desugar(constructor.types),
                };
            },
            [&](cst::definition::Unit_constructor const&) {
                return ast::definition::Unit_constructor {};
            },
        },
        body);
}

auto libdesugar::Context::desugar(cst::definition::Constructor const& constructor)
    -> ast::definition::Constructor
{
    return ast::definition::Constructor {
        .name = constructor.name,
        .body = desugar(constructor.body),
    };
}

auto libdesugar::Context::desugar(cst::Definition const& definition) -> ast::Definition
{
    return {
        .variant = std::visit(
            Definition_desugaring_visitor { *this, definition.source }, definition.variant),
        .source       = definition.source,
        .source_range = definition.source_range,
    };
}
