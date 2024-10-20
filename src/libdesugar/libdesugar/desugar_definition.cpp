#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>
#include <libdesugar/desugar.hpp>

using namespace libdesugar;

namespace {
    auto duplicate_fields_error(
        Context const           context,
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

    template <typename T>
    void ensure_no_duplicates(
        Context const context, std::string_view const description, std::vector<T> const& elements)
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
    auto normalize_function_body(Context const context, ast::Expression expression)
        -> ast::Expression
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
} // namespace

auto libdesugar::desugar(Context const ctx, cst::definition::Field const& field)
    -> ast::definition::Field
{
    return ast::definition::Field {
        .name  = field.name,
        .type  = deref_desugar(ctx, field.type.type),
        .range = field.range,
    };
};

auto libdesugar::desugar(Context const ctx, cst::definition::Constructor_body const& body)
    -> ast::definition::Constructor_body
{
    auto const visitor = utl::Overload {
        [&](cst::definition::Struct_constructor const& constructor) {
            ensure_no_duplicates(ctx, "field", constructor.fields.value.elements);
            return ast::definition::Struct_constructor { desugar(ctx, constructor.fields) };
        },
        [&](cst::definition::Tuple_constructor const& constructor) {
            return ast::definition::Tuple_constructor { desugar(ctx, constructor.types) };
        },
        [&](cst::definition::Unit_constructor const&) {
            return ast::definition::Unit_constructor {};
        },
    };
    return std::visit<ast::definition::Constructor_body>(visitor, body);
}

auto libdesugar::desugar(Context const ctx, cst::definition::Constructor const& constructor)
    -> ast::definition::Constructor
{
    return ast::definition::Constructor {
        .name = constructor.name,
        .body = desugar(ctx, constructor.body),
    };
}

auto libdesugar::desugar(Context context, cst::definition::Function const& function)
    -> ast::definition::Function
{
    return ast::definition::Function {
        .signature = desugar(context, function.signature),
        .body      = normalize_function_body(context, deref_desugar(context, function.body)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::definition::Struct const& structure)
    -> ast::definition::Enumeration
{
    return ast::definition::Enumeration {
        .constructors { utl::to_vector({ ast::definition::Constructor {
            .name = structure.name,
            .body = desugar(ctx, structure.body),
        } }) },
        .name                = structure.name,
        .template_parameters = structure.template_parameters.transform(desugar(ctx)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::definition::Enum const& enumeration)
    -> ast::definition::Enumeration
{
    ensure_no_duplicates(ctx, "constructor", enumeration.constructors.elements);
    return ast::definition::Enumeration {
        .constructors        = desugar(ctx, enumeration.constructors),
        .name                = enumeration.name,
        .template_parameters = enumeration.template_parameters.transform(desugar(ctx)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::definition::Alias const& alias)
    -> ast::definition::Alias
{
    return ast::definition::Alias {
        .name                = alias.name,
        .type                = deref_desugar(ctx, alias.type),
        .template_parameters = alias.template_parameters.transform(desugar(ctx)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::definition::Concept const& concept_)
    -> ast::definition::Concept
{
    std::vector<ast::Function_signature> functions;
    std::vector<ast::Type_signature>     types;

    for (auto const& requirement : concept_.requirements) {
        auto const visitor = utl::Overload {
            [&](cst::Function_signature const& signature) {
                functions.push_back(desugar(ctx, signature));
            },
            [&](cst::Type_signature const& signature) { types.push_back(desugar(ctx, signature)); },
        };
        std::visit(visitor, requirement);
    }

    return ast::definition::Concept {
        .function_signatures = std::move(functions),
        .type_signatures     = std::move(types),
        .name                = concept_.name,
        .template_parameters = concept_.template_parameters.transform(desugar(ctx)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::definition::Impl const& impl)
    -> ast::definition::Impl
{
    return ast::definition::Impl {
        .type                = deref_desugar(ctx, impl.self_type),
        .definitions         = desugar(ctx, impl.definitions),
        .template_parameters = impl.template_parameters.transform(desugar(ctx)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::definition::Submodule const& submodule)
    -> ast::definition::Submodule
{
    return ast::definition::Submodule {
        .definitions         = desugar(ctx, submodule.definitions),
        .name                = submodule.name,
        .template_parameters = submodule.template_parameters.transform(desugar(ctx)),
    };
}

auto libdesugar::desugar(Context const ctx, cst::Definition const& definition) -> ast::Definition
{
    auto const visitor = utl::Overload {
        [&](cst::definition::Function const& function) { return desugar(ctx, function); },
        [&](cst::definition::Struct const& structure) { return desugar(ctx, structure); },
        [&](cst::definition::Enum const& enumeration) { return desugar(ctx, enumeration); },
        [&](cst::definition::Alias const& alias) { return desugar(ctx, alias); },
        [&](cst::definition::Concept const& concept_) { return desugar(ctx, concept_); },
        [&](cst::definition::Impl const& impl) { return desugar(ctx, impl); },
        [&](cst::definition::Submodule const& module) { return desugar(ctx, module); },
    };
    return ast::Definition {
        .variant     = std::visit<ast::Definition_variant>(visitor, definition.variant),
        .document_id = definition.document_id,
        .range       = definition.range,
    };
}

auto kieli::desugar_definition(Context const context, cst::Definition const& definition)
    -> ast::Definition
{
    return desugar(context, definition);
}

auto kieli::desugar_function(Context const context, cst::definition::Function const& function)
    -> ast::definition::Function
{
    return desugar(context, function);
}

auto kieli::desugar_struct(Context const context, cst::definition::Struct const& structure)
    -> ast::definition::Enumeration
{
    return desugar(context, structure);
}

auto kieli::desugar_enum(Context const context, cst::definition::Enum const& enumeration)
    -> ast::definition::Enumeration
{
    return desugar(context, enumeration);
}

auto kieli::desugar_alias(Context const context, cst::definition::Alias const& alias)
    -> ast::definition::Alias
{
    return desugar(context, alias);
}

auto kieli::desugar_concept(Context const context, cst::definition::Concept const& concept_)
    -> ast::definition::Concept
{
    return desugar(context, concept_);
}

auto kieli::desugar_impl(Context const context, cst::definition::Impl const& impl)
    -> ast::definition::Impl
{
    return desugar(context, impl);
}
