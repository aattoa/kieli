#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>
#include <libdesugar/desugar.hpp>

using namespace ki;
using namespace ki::des;

auto ki::des::desugar(Context& ctx, cst::Field const& field) -> ast::Field
{
    return ast::Field {
        .name = field.name,
        .type = desugar(ctx, field.type.type),
    };
}

auto ki::des::desugar(Context& ctx, cst::Constructor_body const& body) -> ast::Constructor_body
{
    auto const visitor = utl::Overload {
        [&](cst::Struct_constructor const& constructor) {
            return ast::Struct_constructor { desugar(ctx, constructor.fields) };
        },
        [&](cst::Tuple_constructor const& constructor) {
            return ast::Tuple_constructor { desugar(ctx, constructor.types) };
        },
        [&](cst::Unit_constructor const&) { return ast::Unit_constructor {}; },
    };
    return std::visit<ast::Constructor_body>(visitor, body);
}

auto ki::des::desugar(Context& ctx, cst::Constructor const& constructor) -> ast::Constructor
{
    return ast::Constructor {
        .name = constructor.name,
        .body = desugar(ctx, constructor.body),
    };
}

auto ki::des::desugar(Context& ctx, cst::Function const& function) -> ast::Function
{
    return ast::Function {
        .signature = desugar(ctx, function.signature),
        .body      = desugar(ctx, function.body),
    };
}

auto ki::des::desugar(Context& ctx, cst::Struct const& structure) -> ast::Struct
{
    return ast::Struct {
        .constructor         = desugar(ctx, structure.constructor),
        .template_parameters = structure.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::des::desugar(Context& ctx, cst::Enum const& enumeration) -> ast::Enum
{
    return ast::Enum {
        .constructors        = desugar(ctx, enumeration.constructors),
        .name                = enumeration.name,
        .template_parameters = enumeration.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::des::desugar(Context& ctx, cst::Alias const& alias) -> ast::Alias
{
    return ast::Alias {
        .name                = alias.name,
        .type                = desugar(ctx, alias.type),
        .template_parameters = alias.template_parameters.transform(desugar(ctx)),
    };
}

auto ki::des::desugar(Context& ctx, cst::Concept const& concept_) -> ast::Concept
{
    std::vector<ast::Function_signature> functions;
    std::vector<ast::Type_signature>     types;

    for (auto const& requirement : concept_.requirements) {
        auto const visitor = utl::Overload {
            [&](cst::Function_signature const& sig) { functions.push_back(desugar(ctx, sig)); },
            [&](cst::Type_signature const& sig) { types.push_back(desugar(ctx, sig)); },
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

auto ki::des::desugar_function(Context& ctx, cst::Function const& function) -> ast::Function
{
    return desugar(ctx, function);
}

auto ki::des::desugar_struct(Context& ctx, cst::Struct const& structure) -> ast::Struct
{
    return desugar(ctx, structure);
}

auto ki::des::desugar_enum(Context& ctx, cst::Enum const& enumeration) -> ast::Enum
{
    return desugar(ctx, enumeration);
}

auto ki::des::desugar_alias(Context& ctx, cst::Alias const& alias) -> ast::Alias
{
    return desugar(ctx, alias);
}

auto ki::des::desugar_concept(Context& ctx, cst::Concept const& concept_) -> ast::Concept
{
    return desugar(ctx, concept_);
}
