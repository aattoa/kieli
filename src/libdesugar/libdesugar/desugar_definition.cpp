#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>
#include <libdesugar/desugar.hpp>

using namespace ki::desugar;

namespace {
    auto duplicate_fields_error(
        Context const          ctx,
        std::string_view const description,
        std::string_view const name,
        ki::Range const        first,
        ki::Range const        second) -> ki::Diagnostic
    {
        return ki::Diagnostic {
            .message      = std::format("Multiple definitions for {} {}", description, name),
            .range        = second,
            .severity     = ki::Severity::Error,
            .related_info = utl::to_vector({
                ki::Diagnostic_related {
                    .message = "First defined here here",
                    .location { .doc_id = ctx.doc_id, .range = first },
                },
            }),
        };
    }

    template <typename T>
    void ensure_no_duplicates(
        Context const ctx, std::string_view const description, std::vector<T> const& elements)
    {
        for (auto it = elements.begin(); it != elements.end(); ++it) {
            auto const duplicate = std::ranges::find(
                it + 1, elements.end(), it->name.id, [](auto const& elem) { return elem.name.id; });
            if (duplicate != elements.end()) {
                ki::Diagnostic diagnostic = duplicate_fields_error(
                    ctx,
                    description,
                    ctx.db.string_pool.get(it->name.id),
                    it->name.range,
                    duplicate->name.range);
                ki::add_diagnostic(ctx.db, ctx.doc_id, std::move(diagnostic));
            }
        }
    }
} // namespace

auto ki::desugar::desugar(Context const ctx, cst::Field const& field) -> ast::Field
{
    return ast::Field {
        .name  = field.name,
        .type  = deref_desugar(ctx, field.type.type),
        .range = field.range,
    };
};

auto ki::desugar::desugar(Context const ctx, cst::Constructor_body const& body)
    -> ast::Constructor_body
{
    auto const visitor = utl::Overload {
        [&](cst::Struct_constructor const& constructor) {
            ensure_no_duplicates(ctx, "field", constructor.fields.value.elements);
            return ast::Struct_constructor { desugar(ctx, constructor.fields) };
        },
        [&](cst::Tuple_constructor const& constructor) {
            return ast::Tuple_constructor { desugar(ctx, constructor.types) };
        },
        [&](cst::Unit_constructor const&) { return ast::Unit_constructor {}; },
    };
    return std::visit<ast::Constructor_body>(visitor, body);
}

auto ki::desugar::desugar(Context const ctx, cst::Constructor const& constructor)
    -> ast::Constructor
{
    return ast::Constructor {
        .name = constructor.name,
        .body = desugar(ctx, constructor.body),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Function const& function) -> ast::Function
{
    return ast::Function {
        .signature = desugar(ctx, function.signature),
        .body      = deref_desugar(ctx, function.body),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Struct const& structure) -> ast::Enumeration
{
    return ast::Enumeration {
        .constructors { utl::to_vector({ ast::Constructor {
            .name = structure.name,
            .body = desugar(ctx, structure.body),
        } }) },
        .name                = structure.name,
        .template_parameters = structure.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Enum const& enumeration) -> ast::Enumeration
{
    ensure_no_duplicates(ctx, "constructor", enumeration.constructors.elements);
    return ast::Enumeration {
        .constructors        = desugar(ctx, enumeration.constructors),
        .name                = enumeration.name,
        .template_parameters = enumeration.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Alias const& alias) -> ast::Alias
{
    return ast::Alias {
        .name                = alias.name,
        .type                = deref_desugar(ctx, alias.type),
        .template_parameters = alias.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Concept const& concept_) -> ast::Concept
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

    return ast::Concept {
        .function_signatures = std::move(functions),
        .type_signatures     = std::move(types),
        .name                = concept_.name,
        .template_parameters = concept_.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Impl const& impl) -> ast::Impl
{
    return ast::Impl {
        .type                = deref_desugar(ctx, impl.self_type),
        .definitions         = desugar(ctx, impl.definitions),
        .template_parameters = impl.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Submodule const& submodule) -> ast::Submodule
{
    return ast::Submodule {
        .definitions         = desugar(ctx, submodule.definitions),
        .name                = submodule.name,
        .template_parameters = submodule.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Definition const& definition) -> ast::Definition
{
    auto const visitor = utl::Overload {
        [&](cst::Function const& function) { return desugar(ctx, function); },
        [&](cst::Struct const& structure) { return desugar(ctx, structure); },
        [&](cst::Enum const& enumeration) { return desugar(ctx, enumeration); },
        [&](cst::Alias const& alias) { return desugar(ctx, alias); },
        [&](cst::Concept const& concept_) { return desugar(ctx, concept_); },
        [&](cst::Impl const& impl) { return desugar(ctx, impl); },
        [&](cst::Submodule const& module) { return desugar(ctx, module); },
    };
    return ast::Definition {
        .variant     = std::visit<ast::Definition_variant>(visitor, definition.variant),
        .document_id = definition.doc_id,
        .range       = definition.range,
    };
}

auto ki::desugar::desugar_definition(Context ctx, cst::Definition const& definition)
    -> ast::Definition
{
    return desugar(ctx, definition);
}

auto ki::desugar::desugar_function(Context ctx, cst::Function const& function) -> ast::Function
{
    return desugar(ctx, function);
}

auto ki::desugar::desugar_struct(Context ctx, cst::Struct const& structure) -> ast::Enumeration
{
    return desugar(ctx, structure);
}

auto ki::desugar::desugar_enum(Context ctx, cst::Enum const& enumeration) -> ast::Enumeration
{
    return desugar(ctx, enumeration);
}

auto ki::desugar::desugar_alias(Context ctx, cst::Alias const& alias) -> ast::Alias
{
    return desugar(ctx, alias);
}

auto ki::desugar::desugar_concept(Context ctx, cst::Concept const& concept_) -> ast::Concept
{
    return desugar(ctx, concept_);
}

auto ki::desugar::desugar_impl(Context ctx, cst::Impl const& impl) -> ast::Impl
{
    return desugar(ctx, impl);
}
