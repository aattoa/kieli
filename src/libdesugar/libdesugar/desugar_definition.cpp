#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>
#include <libdesugar/desugar.hpp>

using namespace libdesugar;

namespace {
    auto duplicate_fields_error(
        Context&                context,
        std::string_view const  description,
        kieli::Identifier const identifier,
        kieli::Range const      first,
        kieli::Range const      second) -> kieli::Diagnostic
    {
        return kieli::Diagnostic {
            .message      = std::format("Multiple definitions for {} {}", description, identifier),
            .range        = second,
            .severity     = kieli::Severity::error,
            .related_info = utl::to_vector({
                kieli::Diagnostic_related_info {
                    .message = "First defined here here",
                    .location { .document_id = context.document_id, .range = first },
                },
            }),
        };
    }

    template <class T>
    auto ensure_no_duplicates(
        Context&               context,
        std::string_view const description,
        std::vector<T> const&  elements) -> void
    {
        for (auto it = elements.begin(); it != elements.end(); ++it) {
            auto const duplicate = std::ranges::find(it + 1, elements.end(), it->name, &T::name);
            if (duplicate != elements.end()) {
                kieli::Diagnostic diagnostic = duplicate_fields_error(
                    context,
                    description,
                    it->name.identifier,
                    it->name.range,
                    duplicate->name.range);
                kieli::add_diagnostic(context.db, context.document_id, std::move(diagnostic));
            }
        }
    }

    // Convert function bodies defined with `=body` syntax into block form.
    auto normalize_function_body(Context& context, ast::Expression expression) -> ast::Expression
    {
        if (std::holds_alternative<ast::expression::Block>(expression.variant)) {
            return expression;
        }
        auto range = expression.range;
        auto block = ast::expression::Block {
            .result = context.ast.expressions.push(std::move(expression)),
        };
        return ast::Expression { std::move(block), range };
    }

    struct Definition_desugaring_visitor {
        Context&           context;
        kieli::Document_id source;

        auto operator()(cst::definition::Function const& function) -> ast::Definition_variant
        {
            return ast::definition::Function {
                .signature = context.desugar(function.signature),
                .body      = normalize_function_body(context, context.deref_desugar(function.body)),
            };
        }

        auto operator()(cst::definition::Struct const& structure) -> ast::Definition_variant
        {
            return ast::definition::Enumeration {
                .constructors { utl::to_vector({ ast::definition::Constructor {
                    .name = structure.name,
                    .body = context.desugar(structure.body),
                } }) },
                .name                = structure.name,
                .template_parameters = structure.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Enum const& enumeration) -> ast::Definition_variant
        {
            ensure_no_duplicates(context, "constructor", enumeration.constructors.elements);
            return ast::definition::Enumeration {
                .constructors        = context.desugar(enumeration.constructors),
                .name                = enumeration.name,
                .template_parameters = enumeration.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Alias const& alias) -> ast::Definition_variant
        {
            return ast::definition::Alias {
                .name                = alias.name,
                .type                = context.deref_desugar(alias.type),
                .template_parameters = alias.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Concept const& concept_) -> ast::Definition_variant
        {
            std::vector<ast::Function_signature> functions;
            std::vector<ast::Type_signature>     types;

            for (auto const& requirement : concept_.requirements) {
                auto const visitor = utl::Overload {
                    [&](cst::Function_signature const& signature) {
                        functions.push_back(context.desugar(signature));
                    },
                    [&](cst::Type_signature const& signature) {
                        types.push_back(context.desugar(signature));
                    },
                };
                std::visit(visitor, requirement);
            }

            return ast::definition::Concept {
                .function_signatures = std::move(functions),
                .type_signatures     = std::move(types),
                .name                = concept_.name,
                .template_parameters = concept_.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Implementation const& implementation)
            -> ast::Definition_variant
        {
            return ast::definition::Implementation {
                .type        = context.deref_desugar(implementation.self_type),
                .definitions = context.desugar(implementation.definitions),
                .template_parameters
                = implementation.template_parameters.transform(context.desugar()),
            };
        }

        auto operator()(cst::definition::Submodule const& submodule) -> ast::Definition_variant
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
        .name  = field.name,
        .type  = deref_desugar(field.type.type),
        .range = field.range,
    };
};

auto libdesugar::Context::desugar(cst::definition::Constructor_body const& body)
    -> ast::definition::Constructor_body
{
    auto const visitor = utl::Overload {
        [&](cst::definition::Struct_constructor const& constructor) {
            ensure_no_duplicates(*this, "field", constructor.fields.value.elements);
            return ast::definition::Struct_constructor { desugar(constructor.fields) };
        },
        [&](cst::definition::Tuple_constructor const& constructor) {
            return ast::definition::Tuple_constructor { desugar(constructor.types) };
        },
        [&](cst::definition::Unit_constructor const&) {
            return ast::definition::Unit_constructor {};
        },
    };
    return std::visit<ast::definition::Constructor_body>(visitor, body);
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
    return ast::Definition {
        std::visit(Definition_desugaring_visitor { *this, definition.source }, definition.variant),
        definition.source,
        definition.range,
    };
}

auto kieli::desugar(
    Database&              db,
    Document_id const      document_id,
    ast::Arena&            ast,
    cst::Arena const&      cst,
    cst::Definition const& definition) -> ast::Definition
{
    libdesugar::Context context { db, cst, ast, document_id };
    return context.desugar(definition);
}
