#include <libutl/utilities.hpp>
#include <libdesugar/desugar.hpp>
#include <libdesugar/internals.hpp>

auto ki::des::wrap_desugar(Context& ctx, cst::Expression const& expression) -> ast::Expression_id
{
    return ctx.ast.expressions.push(desugar(ctx, expression));
}

auto ki::des::wrap_desugar(Context& ctx, cst::Pattern const& pattern) -> ast::Pattern_id
{
    return ctx.ast.patterns.push(desugar(ctx, pattern));
}

auto ki::des::wrap_desugar(Context& ctx, cst::Type const& type) -> ast::Type_id
{
    return ctx.ast.types.push(desugar(ctx, type));
}

auto ki::des::deref_desugar(Context& ctx, cst::Expression_id const id) -> ast::Expression
{
    return desugar(ctx, ctx.cst.expressions[id]);
}

auto ki::des::deref_desugar(Context& ctx, cst::Pattern_id const id) -> ast::Pattern
{
    return desugar(ctx, ctx.cst.patterns[id]);
}

auto ki::des::deref_desugar(Context& ctx, cst::Type_id const id) -> ast::Type
{
    return desugar(ctx, ctx.cst.types[id]);
}

auto ki::des::desugar(Context& ctx, cst::Expression_id const id) -> ast::Expression_id
{
    return ctx.ast.expressions.push(deref_desugar(ctx, id));
}

auto ki::des::desugar(Context& ctx, cst::Pattern_id const id) -> ast::Pattern_id
{
    return ctx.ast.patterns.push(deref_desugar(ctx, id));
}

auto ki::des::desugar(Context& ctx, cst::Type_id const id) -> ast::Type_id
{
    return ctx.ast.types.push(deref_desugar(ctx, id));
}

auto ki::des::desugar(Context& ctx, cst::Wildcard const& wildcard) -> ast::Wildcard
{
    (void)ctx;
    return ast::Wildcard { .range = wildcard.underscore_token };
}

auto ki::des::desugar(Context& ctx, cst::Template_argument const& argument)
    -> ast::Template_argument
{
    return std::visit<ast::Template_argument>(desugar(ctx), argument);
}

auto ki::des::desugar(Context& ctx, cst::Template_parameter const& template_parameter)
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

auto ki::des::desugar(Context& ctx, cst::Path_segment const& segment) -> ast::Path_segment
{
    return ast::Path_segment {
        .template_arguments = segment.template_arguments.transform(desugar(ctx)),
        .name               = segment.name,
    };
}

auto ki::des::desugar(Context& ctx, cst::Path const& path) -> ast::Path
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

auto ki::des::desugar(Context& ctx, cst::Function_parameters const& cst_parameters)
    -> std::vector<ast::Function_parameter>
{
    std::vector<ast::Function_parameter> ast_parameters;
    ast_parameters.reserve(cst_parameters.value.elements.size());

    auto const desugar_argument = [&](cst::Value_parameter_default_argument const& argument) {
        if (auto const* const wildcard = std::get_if<cst::Wildcard>(&argument.variant)) {
            auto const range = wildcard->underscore_token;
            ctx.add_diagnostic(lsp::error(range, "A default argument may not be a wildcard"));
            return ctx.ast.expressions.push(db::Error {}, range);
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
        auto const range = ctx.cst.patterns[parameter.pattern].range;
        ctx.add_diagnostic(lsp::error(range, "The final parameter type must not be omitted"));
        return ctx.ast.types.push(db::Error {}, range);
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

auto ki::des::desugar(Context& ctx, cst::Function_signature const& signature)
    -> ast::Function_signature
{
    // If there is no explicit return type, insert the unit type.
    ast::Type_id return_type = signature.return_type.has_value()
                                 ? desugar(ctx, signature.return_type.value().type)
                                 : ctx.ast.types.push(unit_type(signature.name.range));
    return ast::Function_signature {
        .template_parameters = signature.template_parameters.transform(desugar(ctx)),
        .function_parameters = desugar(ctx, signature.function_parameters),
        .return_type         = return_type,
        .name                = signature.name,
    };
}

auto ki::des::desugar(Context& ctx, cst::Type_signature const& signature) -> ast::Type_signature
{
    return ast::Type_signature {
        .concepts = desugar(ctx, signature.concepts.elements),
        .name     = signature.name,
    };
}

auto ki::des::desugar(Context& ctx, cst::Field_init const& field) -> ast::Field_init
{
    if (field.equals.has_value()) {
        return { .name = field.name, .expression = desugar(ctx, field.equals.value().expression) };
    }
    auto segment = ast::Path_segment { .template_arguments = std::nullopt, .name = field.name };
    auto path    = ast::Path { .root = {}, .segments = utl::to_vector({ std::move(segment) }) };
    return ast::Field_init {
        .name       = field.name,
        .expression = ctx.ast.expressions.push(std::move(path), field.name.range),
    };
}

auto ki::des::desugar(Context& ctx, cst::patt::Field const& field) -> ast::patt::Field
{
    auto const pattern = std::invoke([&] {
        if (field.equals.has_value()) {
            return desugar(ctx, field.equals.value().pattern);
        }
        ast::Mutability mutability { .variant = db::Mutability::Immut, .range = field.name.range };
        ast::patt::Name name { .name = field.name, .mutability = mutability };
        return ctx.ast.patterns.push(ast::Pattern { .variant = name, .range = field.name.range });
    });
    return ast::patt::Field { .name = field.name, .pattern = pattern };
}

auto ki::des::desugar(Context& ctx, cst::patt::Constructor_body const& body)
    -> ast::patt::Constructor_body
{
    auto const visitor = utl::Overload {
        [&](cst::patt::Struct_constructor const& constructor) {
            return ast::patt::Struct_constructor { .fields = desugar(ctx, constructor.fields) };
        },
        [&](cst::patt::Tuple_constructor const& constructor) {
            return ast::patt::Tuple_constructor { .fields = desugar(ctx, constructor.fields) };
        },
        [&](cst::patt::Unit_constructor const&) { return ast::patt::Unit_constructor {}; },
    };
    return std::visit<ast::patt::Constructor_body>(visitor, body);
};

auto ki::des::desugar(Context& ctx, cst::Type_parameter_default_argument const& argument)
    -> ast::Template_type_parameter::Default
{
    return std::visit<ast::Template_type_parameter::Default>(desugar(ctx), argument.variant);
}

auto ki::des::desugar(Context& ctx, cst::Value_parameter_default_argument const& argument)
    -> ast::Template_value_parameter::Default
{
    return std::visit<ast::Template_value_parameter::Default>(desugar(ctx), argument.variant);
}

auto ki::des::desugar(Context& ctx, cst::Mutability_parameter_default_argument const& argument)
    -> ast::Template_mutability_parameter::Default
{
    return std::visit<ast::Template_mutability_parameter::Default>(desugar(ctx), argument.variant);
}

auto ki::des::desugar(Context& ctx, cst::Type_annotation const& annotation) -> ast::Type_id
{
    return desugar(ctx, annotation.type);
}

auto ki::des::desugar(Context& ctx, cst::Mutability const& mutability) -> ast::Mutability
{
    (void)ctx;

    auto const visitor = utl::Overload {
        [](cst::Parameterized_mutability const& parameterized) {
            return ast::Parameterized_mutability { parameterized.name };
        },
        [](db::Mutability concrete) { return concrete; },
    };
    return ast::Mutability {
        .variant = std::visit<ast::Mutability_variant>(visitor, mutability.variant),
        .range   = mutability.range,
    };
}

auto ki::des::desugar_opt_mut(
    Context& ctx, std::optional<cst::Mutability> const& mutability, lsp::Range const range)
    -> ast::Mutability
{
    if (mutability.has_value()) {
        return desugar(ctx, mutability.value());
    };
    return ast::Mutability { .variant = db::Mutability::Immut, .range = range };
}

auto ki::des::unit_type(lsp::Range const range) -> ast::Type
{
    return { .variant = ast::type::Tuple {}, .range = range };
}

auto ki::des::wildcard_type(lsp::Range const range) -> ast::Type
{
    return { .variant = ast::Wildcard { range }, .range = range };
}

auto ki::des::unit_value(lsp::Range const range) -> ast::Expression
{
    return { .variant = ast::expr::Tuple {}, .range = range };
}

auto ki::des::wildcard_pattern(lsp::Range const range) -> ast::Pattern
{
    return { .variant = ast::Wildcard { range }, .range = range };
}
