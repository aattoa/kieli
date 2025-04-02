#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    auto resolve_function_parameter(
        Context&                       ctx,
        Inference_state&               state,
        hir::Scope_id                  scope_id,
        hir::Environment_id            env_id,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        ast::Arena& ast = ctx.documents.at(state.doc_id).ast;

        auto pattern = resolve_pattern(ctx, state, scope_id, env_id, ast.patt[parameter.pattern]);
        auto type    = resolve_type(ctx, state, scope_id, env_id, ast.type[parameter.type]);

        require_subtype_relationship(
            ctx, state, pattern.range, ctx.hir.type[pattern.type], ctx.hir.type[type.id]);

        auto const resolve_default = [&](ast::Expression_id const argument) {
            auto expression = resolve_expression(ctx, state, scope_id, env_id, ast.expr[argument]);
            require_subtype_relationship(
                ctx, state, expression.range, ctx.hir.type[expression.type], ctx.hir.type[type.id]);
            return expression;
        };

        return hir::Function_parameter {
            .pattern          = std::move(pattern),
            .type             = type,
            .default_argument = parameter.default_argument.transform(resolve_default),
        };
    }

    auto resolve_signature(
        Context&                       ctx,
        hir::Environment_id const      env_id,
        ki::Document_id const          doc_id,
        ast::Function_signature const& signature) -> hir::Function_signature
    {
        auto state    = make_inference_state(doc_id);
        auto scope_id = ctx.info.scopes.push(make_scope(doc_id));

        auto template_parameters = resolve_template_parameters(
            ctx, state, scope_id, env_id, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(ctx, state, scope_id, env_id, parameter);
        };

        auto parameters = std::views::transform(signature.function_parameters, resolve_parameter)
                        | std::ranges::to<std::vector>();
        auto parameter_types = std::views::transform(parameters, &hir::Function_parameter::type)
                             | std::ranges::to<std::vector>();

        auto const return_type = resolve_type(ctx, state, scope_id, env_id, signature.return_type);

        auto const id = ctx.hir.type.push(hir::type::Function {
            .parameter_types = std::move(parameter_types),
            .return_type     = return_type,
        });

        ensure_no_unsolved_variables(ctx, state);

        return hir::Function_signature {
            .template_paramters = std::move(template_parameters),
            .parameters         = std::move(parameters),
            .return_type        = return_type,
            .function_type {
                .id    = id,
                .range = signature.name.range,
            },
            .name     = signature.name,
            .scope_id = scope_id,
        };
    }
} // namespace

auto ki::resolve::resolve_function_body(Context& ctx, Function_info& info) -> hir::Expression&
{
    if (not info.body.has_value()) {
        Inference_state state {
            .type_vars    = {},
            .type_var_set = {},
            .mut_vars     = {},
            .mut_var_set  = {},
            .doc_id       = info.doc_id,
        };

        hir::Function_signature& signature = resolve_function_signature(ctx, info);
        info.body = resolve_expression(ctx, state, signature.scope_id, info.env_id, info.ast.body);

        require_subtype_relationship(
            ctx,
            state,
            info.body.value().range,
            ctx.hir.type[info.body.value().type],
            ctx.hir.type[signature.return_type.id]);
        ensure_no_unsolved_variables(ctx, state);
    }
    return info.body.value();
}

auto ki::resolve::resolve_function_signature(Context& ctx, Function_info& info)
    -> hir::Function_signature&
{
    if (not info.signature.has_value()) {
        info.signature = resolve_signature(ctx, info.env_id, info.doc_id, info.ast.signature);
    }
    return info.signature.value();
}

auto ki::resolve::resolve_enumeration(Context& ctx, Enumeration_info& info) -> hir::Enumeration&
{
    if (not info.hir.has_value()) {
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}

auto ki::resolve::resolve_concept(Context& ctx, Concept_info& info) -> hir::Concept&
{
    if (not info.hir.has_value()) {
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}

auto ki::resolve::resolve_alias(Context& ctx, Alias_info& info) -> hir::Alias&
{
    if (not info.hir.has_value()) {
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}
