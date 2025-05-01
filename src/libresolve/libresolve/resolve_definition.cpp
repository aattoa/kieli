#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto resolve_function_parameter(
        db::Database&                  db,
        Context&                       ctx,
        Inference_state&               state,
        db::Environment_id             env_id,
        ast::Function_parameter const& parameter) -> hir::Function_parameter
    {
        auto pattern
            = resolve_pattern(db, ctx, state, env_id, ctx.arena.ast.patterns[parameter.pattern]);
        auto type = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[parameter.type]);

        require_subtype_relationship(
            db,
            ctx,
            state,
            pattern.range,
            ctx.arena.hir.types[pattern.type_id],
            ctx.arena.hir.types[type.id]);

        auto const resolve_default = [&](ast::Expression_id const argument) {
            auto expression
                = resolve_expression(db, ctx, state, env_id, ctx.arena.ast.expressions[argument]);
            require_subtype_relationship(
                db,
                ctx,
                state,
                expression.range,
                ctx.arena.hir.types[expression.type_id],
                ctx.arena.hir.types[type.id]);
            return ctx.arena.hir.expressions.push(std::move(expression));
        };

        return hir::Function_parameter {
            .pattern_id       = ctx.arena.hir.patterns.push(std::move(pattern)),
            .type             = type,
            .default_argument = parameter.default_argument.transform(resolve_default),
        };
    }

    void resolve_signature(
        db::Database&                  db,
        Context&                       ctx,
        hir::Function_id const         fun_id,
        db::Environment_id const       env_id,
        ast::Function_signature const& signature)
    {
        auto state            = Inference_state {};
        auto signature_env_id = new_scope(ctx, env_id);

        auto template_parameters = resolve_template_parameters(
            db, ctx, state, signature_env_id, signature.template_parameters);

        auto const resolve_parameter = [&](ast::Function_parameter const& parameter) {
            return resolve_function_parameter(db, ctx, state, signature_env_id, parameter);
        };

        auto parameters = std::views::transform(signature.function_parameters, resolve_parameter)
                        | std::ranges::to<std::vector>();
        auto parameter_types = std::views::transform(parameters, &hir::Function_parameter::type)
                             | std::ranges::to<std::vector>();

        auto return_type = resolve_type(db, ctx, state, signature_env_id, signature.return_type);

        auto const type_id = ctx.arena.hir.types.push(hir::type::Function {
            .parameter_types = std::move(parameter_types),
            .return_type     = return_type,
        });

        ensure_no_unsolved_variables(db, ctx, state);

        ctx.signature_scope_map.insert_or_assign(fun_id, signature_env_id);

        ctx.arena.hir.functions[fun_id].signature = hir::Function_signature {
            .template_paramters = std::move(template_parameters),
            .parameters         = std::move(parameters),
            .return_type        = return_type,
            .function_type      = hir::Type { .id = type_id, .range = signature.name.range },
            .name               = signature.name,
        };
    }
} // namespace

auto ki::res::resolve_function_body(db::Database& db, Context& ctx, hir::Function_id id)
    -> hir::Expression&
{
    hir::Function_info& info = ctx.arena.hir.functions[id];

    if (not info.body.has_value()) {
        auto  state     = Inference_state {};
        auto& signature = resolve_function_signature(db, ctx, id);

        auto const it = ctx.signature_scope_map.find(id);
        cpputil::always_assert(it != ctx.signature_scope_map.end());
        info.body = resolve_expression(db, ctx, state, it->second, info.ast.body);
        report_unused(db, ctx, it->second);
        recycle_environment(ctx, it->second);
        ctx.signature_scope_map.erase(it);

        require_subtype_relationship(
            db,
            ctx,
            state,
            info.body.value().range,
            ctx.arena.hir.types[info.body.value().type_id],
            ctx.arena.hir.types[signature.return_type.id]);
        ensure_no_unsolved_variables(db, ctx, state);
    }
    return info.body.value();
}

auto ki::res::resolve_function_signature(db::Database& db, Context& ctx, hir::Function_id id)
    -> hir::Function_signature&
{
    hir::Function_info& info = ctx.arena.hir.functions[id];
    if (not info.signature.has_value()) {
        resolve_signature(db, ctx, id, info.env_id, info.ast.signature);
    }
    return info.signature.value();
}

auto ki::res::resolve_enumeration(db::Database& db, Context& ctx, hir::Enumeration_id id)
    -> hir::Enumeration&
{
    hir::Enumeration_info& info = ctx.arena.hir.enumerations[id];
    if (not info.hir.has_value()) {
        (void)db;
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}

auto ki::res::resolve_concept(db::Database& db, Context& ctx, hir::Concept_id id) -> hir::Concept&
{
    hir::Concept_info& info = ctx.arena.hir.concepts[id];
    if (not info.hir.has_value()) {
        (void)db;
        (void)ctx;
        cpputil::todo();
    }
    return info.hir.value();
}

auto ki::res::resolve_alias(db::Database& db, Context& ctx, hir::Alias_id id) -> hir::Alias&
{
    hir::Alias_info& info = ctx.arena.hir.aliases[id];
    if (not info.hir.has_value()) {
        auto state = Inference_state {};
        auto type  = resolve_type(db, ctx, state, info.env_id, info.ast.type);
        ensure_no_unsolved_variables(db, ctx, state);
        info.hir = hir::Alias { .name = info.name, .type = type };
    }
    return info.hir.value();
}
