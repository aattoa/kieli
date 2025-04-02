#include <libutl/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/internals.hpp>

auto ki::desugar::wrap_desugar(Context const ctx, cst::Expression const& expression)
    -> ast::Expression_id
{
    return ctx.ast.expr.push(desugar(ctx, expression));
}

auto ki::desugar::wrap_desugar(Context const ctx, cst::Pattern const& pattern) -> ast::Pattern_id
{
    return ctx.ast.patt.push(desugar(ctx, pattern));
}

auto ki::desugar::wrap_desugar(Context const ctx, cst::Type const& type) -> ast::Type_id
{
    return ctx.ast.type.push(desugar(ctx, type));
}

auto ki::desugar::deref_desugar(Context const ctx, cst::Expression_id const id) -> ast::Expression
{
    return desugar(ctx, ctx.cst.expr[id]);
}

auto ki::desugar::deref_desugar(Context const ctx, cst::Pattern_id const id) -> ast::Pattern
{
    return desugar(ctx, ctx.cst.patt[id]);
}

auto ki::desugar::deref_desugar(Context const ctx, cst::Type_id const id) -> ast::Type
{
    return desugar(ctx, ctx.cst.type[id]);
}

auto ki::desugar::desugar(Context const ctx, cst::Expression_id const id) -> ast::Expression_id
{
    return ctx.ast.expr.push(deref_desugar(ctx, id));
}

auto ki::desugar::desugar(Context const ctx, cst::Pattern_id const id) -> ast::Pattern_id
{
    return ctx.ast.patt.push(deref_desugar(ctx, id));
}

auto ki::desugar::desugar(Context const ctx, cst::Type_id const id) -> ast::Type_id
{
    return ctx.ast.type.push(deref_desugar(ctx, id));
}

auto ki::desugar::desugar(Context const ctx, cst::Wildcard const& wildcard) -> ast::Wildcard
{
    return ast::Wildcard { .range = ctx.cst.range[wildcard.underscore_token] };
}

auto ki::desugar::desugar(Context const ctx, cst::Template_argument const& argument)
    -> ast::Template_argument
{
    return std::visit<ast::Template_argument>(desugar(ctx), argument);
}

auto ki::desugar::desugar(Context const ctx, cst::Template_parameter const& template_parameter)
    -> ast::Template_parameter
{
    auto const visitor = utl::Overload {
        [&](cst::Template_type_parameter const& parameter) {
            return ast::Template_type_parameter {
                .name             = parameter.name,
                .concepts         = desugar(ctx, parameter.concepts),
                .default_argument = parameter.default_argument.transform(desugar(ctx)),
            };
        },
        [&](cst::Template_value_parameter const& parameter) {
            return ast::Template_value_parameter {
                .name             = parameter.name,
                .type             = desugar(ctx, parameter.type_annotation),
                .default_argument = parameter.default_argument.transform(desugar(ctx)),
            };
        },
        [&](cst::Template_mutability_parameter const& parameter) {
            return ast::Template_mutability_parameter {
                .name             = parameter.name,
                .default_argument = parameter.default_argument.transform(desugar(ctx)),
            };
        },
    };
    return ast::Template_parameter {
        .variant = std::visit<ast::Template_parameter_variant>(visitor, template_parameter.variant),
        .range   = template_parameter.range,
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Path_segment const& segment) -> ast::Path_segment
{
    return ast::Path_segment {
        .template_arguments = segment.template_arguments.transform(desugar(ctx)),
        .name               = segment.name,
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Path const& path) -> ast::Path
{
    auto const root_visitor = utl::Overload {
        [](std::monostate) { return std::monostate {}; },
        [](cst::Path_root_global const&) { return ast::Path_root_global {}; },
        [&](cst::Type_id const type) { return desugar(ctx, type); },
    };
    return ast::Path {
        .root     = std::visit<ast::Path_root>(root_visitor, path.root),
        .segments = desugar(ctx, path.segments),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Function_parameters const& cst_parameters)
    -> std::vector<ast::Function_parameter>
{
    std::vector<ast::Function_parameter> ast_parameters;
    ast_parameters.reserve(cst_parameters.value.elements.size());

    auto const desugar_argument = [&](cst::Value_parameter_default_argument const& argument) {
        if (auto const* const wildcard = std::get_if<cst::Wildcard>(&argument.variant)) {
            auto const range = ctx.cst.range[wildcard->underscore_token];
            ki::add_error(ctx.db, ctx.doc_id, range, "A default argument may not be a wildcard");
            return ctx.ast.expr.push(ki::Error {}, range);
        }
        return desugar(ctx, std::get<cst::Expression_id>(argument.variant));
    };

    auto const desugar_type = [&](cst::Function_parameter const& parameter) {
        if (parameter.type.has_value()) {
            return desugar(ctx, parameter.type.value());
        }
        if (not ast_parameters.empty()) {
            return ast_parameters.front().type;
        }
        auto const range = ctx.cst.patt[parameter.pattern].range;
        ki::add_error(
            ctx.db, ctx.doc_id, range, "The type of the final parameter must not be omitted");
        return ctx.ast.type.push(ki::Error {}, range);
    };

    for (auto const& parameter : std::views::reverse(cst_parameters.value.elements)) {
        ast_parameters.emplace(
            ast_parameters.begin(),
            desugar(ctx, parameter.pattern),
            desugar_type(parameter),
            parameter.default_argument.transform(desugar_argument));
    }

    return ast_parameters;
}

auto ki::desugar::desugar(Context const ctx, cst::Function_signature const& signature)
    -> ast::Function_signature
{
    // If there is no explicit return type, insert the unit type.
    ast::Type return_type = signature.return_type.has_value()
                              ? desugar(ctx, ctx.cst.type[signature.return_type.value().type])
                              : unit_type(signature.name.range);
    return ast::Function_signature {
        .template_parameters = signature.template_parameters.transform(desugar(ctx)),
        .function_parameters = desugar(ctx, signature.function_parameters),
        .return_type         = std::move(return_type),
        .name                = signature.name,
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Type_signature const& signature)
    -> ast::Type_signature
{
    return ast::Type_signature {
        .concepts = desugar(ctx, signature.concepts.elements),
        .name     = signature.name,
    };
}

auto ki::desugar::desugar(Context const ctx, cst::Struct_field_init const& field)
    -> ast::Struct_field_initializer
{
    if (field.equals.has_value()) {
        return { .name = field.name, .expression = desugar(ctx, field.equals.value().expression) };
    }
    ast::Path path {
        .root     = {},
        .segments = utl::to_vector(
            { ast::Path_segment { .template_arguments = std::nullopt, .name = field.name } }),
    };
    return {
        .name       = field.name,
        .expression = ctx.ast.expr.push(std::move(path), field.name.range),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::pattern::Field const& field)
    -> ast::pattern::Field
{
    return ast::pattern::Field {
        .name    = field.name,
        .pattern = field.equals.transform(
            [&](cst::pattern::Equals const& field) { return desugar(ctx, field.pattern); }),
    };
}

auto ki::desugar::desugar(Context const ctx, cst::pattern::Constructor_body const& body)
    -> ast::pattern::Constructor_body
{
    auto visitor = utl::Overload {
        [&](cst::pattern::Struct_constructor const& constructor) {
            return ast::pattern::Struct_constructor { .fields = desugar(ctx, constructor.fields) };
        },
        [&](cst::pattern::Tuple_constructor const& constructor) {
            return ast::pattern::Tuple_constructor { .pattern = desugar(ctx, constructor.pattern) };
        },
        [&](cst::pattern::Unit_constructor const&) { return ast::pattern::Unit_constructor {}; },
    };
    return std::visit<ast::pattern::Constructor_body>(visitor, body);
};

auto ki::desugar::desugar(Context const ctx, cst::Type_parameter_default_argument const& argument)
    -> ast::Template_type_parameter::Default
{
    return std::visit<ast::Template_type_parameter::Default>(desugar(ctx), argument.variant);
}

auto ki::desugar::desugar(Context const ctx, cst::Value_parameter_default_argument const& argument)
    -> ast::Template_value_parameter::Default
{
    return std::visit<ast::Template_value_parameter::Default>(desugar(ctx), argument.variant);
}

auto ki::desugar::desugar(
    Context const ctx, cst::Mutability_parameter_default_argument const& argument)
    -> ast::Template_mutability_parameter::Default
{
    return std::visit<ast::Template_mutability_parameter::Default>(desugar(ctx), argument.variant);
}

auto ki::desugar::desugar(Context const ctx, cst::Type_annotation const& annotation) -> ast::Type_id
{
    return desugar(ctx, annotation.type);
}

auto ki::desugar::desugar(Context /*ctx*/, cst::Mutability const& mutability) -> ast::Mutability
{
    return desugar_mutability(mutability);
}

auto ki::desugar::desugar_mutability(
    std::optional<cst::Mutability> const& mutability, ki::Range const range) -> ast::Mutability
{
    if (mutability.has_value()) {
        return desugar_mutability(mutability.value());
    };
    return { .variant = ki::Mutability::Immut, .range = range };
}

auto ki::desugar::desugar_mutability(cst::Mutability const& mutability) -> ast::Mutability
{
    static constexpr auto visitor = utl::Overload {
        [](cst::Parameterized_mutability const& parameterized) {
            return ast::Parameterized_mutability { parameterized.name };
        },
        [](ki::Mutability const concrete) { return concrete; },
    };
    return ast::Mutability {
        .variant = std::visit<ast::Mutability_variant>(visitor, mutability.variant),
        .range   = mutability.range,
    };
}

auto ki::desugar::unit_type(ki::Range const range) -> ast::Type
{
    return { .variant = ast::type::Tuple {}, .range = range };
}

auto ki::desugar::wildcard_type(ki::Range const range) -> ast::Type
{
    return { .variant = ast::Wildcard { range }, .range = range };
}

auto ki::desugar::unit_value(ki::Range const range) -> ast::Expression
{
    return { .variant = ast::expression::Tuple {}, .range = range };
}

auto ki::desugar::wildcard_pattern(ki::Range const range) -> ast::Pattern
{
    return { .variant = ast::Wildcard { range }, .range = range };
}
