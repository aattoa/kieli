#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto resolve_function_parameter(
        db::Database&                  db,
        Context&                       ctx,
        Inference_state&               state,
        Scope_id                       scope_id,
        hir::Environment_id            env_id,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        ast::Arena& ast = db.documents[state.doc_id].arena.ast;

        auto pattern
            = resolve_pattern(db, ctx, state, scope_id, env_id, ast.patterns[parameter.pattern]);
        auto type = resolve_type(db, ctx, state, scope_id, env_id, ast.types[parameter.type]);

        require_subtype_relationship(
            db, ctx, state, pattern.range, ctx.hir.types[pattern.type], ctx.hir.types[type.id]);

        auto const resolve_default = [&](ast::Expression_id const argument) {
            auto expression
                = resolve_expression(db, ctx, state, scope_id, env_id, ast.expressions[argument]);
            require_subtype_relationship(
                db,
                ctx,
                state,
                expression.range,
                ctx.hir.types[expression.type],
                ctx.hir.types[type.id]);
            return expression;
        };

        return hir::Function_parameter {
            .pattern          = std::move(pattern),
            .type             = type,
            .default_argument = parameter.default_argument.transform(resolve_default),
        };
    }

    void resolve_signature(
        db::Database&                  db,
        Context&                       ctx,
        hir::Function_id const         fun_id,
        hir::Environment_id const      env_id,
        db::Document_id const          doc_id,
        ast::Function_signature const& signature)
    {
        auto state    = inference_state(doc_id);
        auto scope_id = ctx.scopes.push(make_scope(doc_id));

        auto template_parameters = resolve_template_parameters(
            db, ctx, state, scope_id, env_id, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(db, ctx, state, scope_id, env_id, parameter);
        };

        auto parameters = std::views::transform(signature.function_parameters, resolve_parameter)
                        | std::ranges::to<std::vector>();
        auto parameter_types = std::views::transform(parameters, &hir::Function_parameter::type)
                             | std::ranges::to<std::vector>();

        auto return_type = resolve_type(db, ctx, state, scope_id, env_id, signature.return_type);

        auto const id = ctx.hir.types.push(hir::type::Function {
            .parameter_types = std::move(parameter_types),
            .return_type     = return_type,
        });

        ensure_no_unsolved_variables(db, ctx, state);

        ctx.scope_map.insert_or_assign(fun_id, scope_id);
        ctx.hir.functions[fun_id].signature = hir::Function_signature {
            .template_paramters = std::move(template_parameters),
            .parameters         = std::move(parameters),
            .return_type        = return_type,
            .function_type      = hir::Type { .id = id, .range = signature.name.range },
            .name               = signature.name,
        };
    }
} // namespace

auto ki::res::resolve_function_body(db::Database& db, Context& ctx, hir::Function_id id)
    -> hir::Expression&
{
    hir::Function_info& info = ctx.hir.functions[id];

    if (not info.body.has_value()) {
        auto state = inference_state(info.doc_id);

        hir::Function_signature& signature = resolve_function_signature(db, ctx, id);

        auto const it = ctx.scope_map.find(id);
        cpputil::always_assert(it != ctx.scope_map.end());
        info.body = resolve_expression(db, ctx, state, it->second, info.env_id, info.ast.body);
        ctx.scope_map.erase(it);

        require_subtype_relationship(
            db,
            ctx,
            state,
            info.body.value().range,
            ctx.hir.types[info.body.value().type],
            ctx.hir.types[signature.return_type.id]);
        ensure_no_unsolved_variables(db, ctx, state);
    }
    return info.body.value();
}

auto ki::res::resolve_function_signature(db::Database& db, Context& ctx, hir::Function_id id)
    -> hir::Function_signature&
{
    hir::Function_info& info = ctx.hir.functions[id];
    if (not info.signature.has_value()) {
        resolve_signature(db, ctx, id, info.env_id, info.doc_id, info.ast.signature);
    }
    return info.signature.value();
}

auto ki::res::resolve_enumeration(db::Database& db, Context& ctx, hir::Enumeration_id id)
    -> hir::Enumeration&
{
    hir::Enumeration_info& info = ctx.hir.enumerations[id];
    if (not info.hir.has_value()) {
        (void)db;
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}

auto ki::res::resolve_concept(db::Database& db, Context& ctx, hir::Concept_id id) -> hir::Concept&
{
    hir::Concept_info& info = ctx.hir.concepts[id];
    if (not info.hir.has_value()) {
        (void)db;
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}

auto ki::res::resolve_alias(db::Database& db, Context& ctx, hir::Alias_id id) -> hir::Alias&
{
    hir::Alias_info& info = ctx.hir.aliases[id];
    if (not info.hir.has_value()) {
        (void)db;
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}
