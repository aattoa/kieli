#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    struct Pattern_resolution_visitor {
        Context&            ctx;
        Inference_state&    state;
        hir::Scope_id       scope_id;
        hir::Environment_id env_id;
        ast::Pattern const& this_pattern;

        auto ast() -> ast::Arena&
        {
            return ctx.documents.at(state.doc_id).ast;
        }

        auto recurse()
        {
            return [&](ast::Pattern const& pattern) -> hir::Pattern {
                return resolve_pattern(ctx, state, scope_id, env_id, pattern);
            };
        }

        auto recurse(ast::Pattern const& pattern) -> hir::Pattern
        {
            return recurse()(pattern);
        }

        auto operator()(ki::Integer const& integer) -> hir::Pattern
        {
            auto type = fresh_integral_type_variable(state, ctx.hir, this_pattern.range);
            return { .variant = integer, .type = type.id, .range = this_pattern.range };
        }

        auto operator()(ki::Floating const& floating) -> hir::Pattern
        {
            return {
                .variant = floating,
                .type    = ctx.constants.type_floating,
                .range   = this_pattern.range,
            };
        }

        auto operator()(ki::Boolean const& boolean) -> hir::Pattern
        {
            return {
                .variant = boolean,
                .type    = ctx.constants.type_boolean,
                .range   = this_pattern.range,
            };
        }

        auto operator()(ki::String const& string) -> hir::Pattern
        {
            return {
                .variant = string,
                .type    = ctx.constants.type_string,
                .range   = this_pattern.range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            return {
                .variant = hir::Wildcard {},
                .type    = fresh_general_type_variable(state, ctx.hir, this_pattern.range).id,
                .range   = this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Name const& pattern) -> hir::Pattern
        {
            auto const tag  = fresh_local_variable_tag(ctx.tags);
            auto const mut  = resolve_mutability(ctx, scope_id, pattern.mutability);
            auto const type = fresh_general_type_variable(state, ctx.hir, pattern.name.range);

            bind_variable(
                ctx.info.scopes.index_vector[scope_id],
                Variable_bind {
                    .name       = pattern.name,
                    .type       = type.id,
                    .mutability = mut,
                    .tag        = tag,
                });

            hir::pattern::Name variant {
                .mutability   = mut,
                .identifier   = pattern.name.id,
                .variable_tag = tag,
            };

            return {
                .variant = std::move(variant),
                .type    = type.id,
                .range   = this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Abbreviated_constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Tuple const& tuple) -> hir::Pattern
        {
            auto patterns = std::views::transform(tuple.field_patterns, recurse())
                          | std::ranges::to<std::vector>();
            auto types = std::views::transform(patterns, hir::pattern_type)
                       | std::ranges::to<std::vector>();
            return {
                .variant = hir::pattern::Tuple { std::move(patterns) },
                .type    = ctx.hir.type.push(hir::type::Tuple { std::move(types) }),
                .range   = this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Slice const& slice) -> hir::Pattern
        {
            auto patterns = std::views::transform(slice.element_patterns, recurse())
                          | std::ranges::to<std::vector>();

            auto const element_type
                = fresh_general_type_variable(state, ctx.hir, this_pattern.range);

            for (hir::Pattern const& pattern : patterns) {
                require_subtype_relationship(
                    ctx,
                    state,
                    pattern.range,
                    ctx.hir.type[pattern.type],
                    ctx.hir.type[element_type.id]);
            }

            return {
                .variant = hir::pattern::Slice { std::move(patterns) },
                .type    = ctx.hir.type.push(hir::type::Slice { element_type }),
                .range   = this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Guarded const& guarded) -> hir::Pattern
        {
            auto pattern = recurse(ast().patt[guarded.guarded_pattern]);
            auto guard = resolve_expression(ctx, state, scope_id, env_id, guarded.guard_expression);

            require_subtype_relationship(
                ctx,
                state,
                guard.range,
                ctx.hir.type[guard.type],
                ctx.hir.type[ctx.constants.type_boolean]);

            return {
                .variant = hir::pattern::Guarded {
                    .guarded_pattern  = ctx.hir.patt.push(std::move(pattern)),
                    .guard_expression = ctx.hir.expr.push(std::move(guard)),
                },
                .type  = pattern.type,
                .range = this_pattern.range,
            };
        }
    };
} // namespace

auto ki::resolve::resolve_pattern(
    Context&                  ctx,
    Inference_state&          state,
    hir::Scope_id const       scope_id,
    hir::Environment_id const env_id,
    ast::Pattern const&       pattern) -> hir::Pattern
{
    Pattern_resolution_visitor visitor {
        .ctx          = ctx,
        .state        = state,
        .scope_id     = scope_id,
        .env_id       = env_id,
        .this_pattern = pattern,
    };
    return std::visit(visitor, pattern.variant);
}
