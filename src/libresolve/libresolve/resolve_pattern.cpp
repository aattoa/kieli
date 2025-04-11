#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        Context&            ctx;
        Inference_state&    state;
        Scope_id            scope_id;
        hir::Environment_id env_id;
        lsp::Range          this_range;

        auto ast() -> ast::Arena&
        {
            return ctx.db.documents[state.doc_id].ast;
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

        auto operator()(db::Integer const& integer) -> hir::Pattern
        {
            auto type = fresh_integral_type_variable(state, ctx.hir, this_range);
            return { .variant = integer, .type = type.id, .range = this_range };
        }

        auto operator()(db::Floating const& floating) -> hir::Pattern
        {
            return {
                .variant = floating,
                .type    = ctx.constants.type_floating,
                .range   = this_range,
            };
        }

        auto operator()(db::Boolean const& boolean) -> hir::Pattern
        {
            return {
                .variant = boolean,
                .type    = ctx.constants.type_boolean,
                .range   = this_range,
            };
        }

        auto operator()(db::String const& string) -> hir::Pattern
        {
            return {
                .variant = string,
                .type    = ctx.constants.type_string,
                .range   = this_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            return {
                .variant = hir::Wildcard {},
                .type    = fresh_general_type_variable(state, ctx.hir, this_range).id,
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Name const& pattern) -> hir::Pattern
        {
            auto const tag  = fresh_local_variable_tag(ctx.tags);
            auto const mut  = resolve_mutability(ctx, scope_id, pattern.mutability);
            auto const type = fresh_general_type_variable(state, ctx.hir, pattern.name.range);

            bind_variable(
                ctx.scopes.index_vector[scope_id],
                hir::Variable_bind {
                    .name       = pattern.name,
                    .type       = type.id,
                    .mutability = mut,
                    .tag        = tag,
                });

            hir::patt::Name variant {
                .mutability   = mut,
                .identifier   = pattern.name.id,
                .variable_tag = tag,
            };

            return {
                .variant = std::move(variant),
                .type    = type.id,
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::patt::Abbreviated_constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::patt::Tuple const& tuple) -> hir::Pattern
        {
            auto patterns = std::views::transform(tuple.field_patterns, recurse())
                          | std::ranges::to<std::vector>();
            auto types = std::views::transform(patterns, hir::pattern_type)
                       | std::ranges::to<std::vector>();
            return {
                .variant = hir::patt::Tuple { std::move(patterns) },
                .type    = ctx.hir.types.push(hir::type::Tuple { std::move(types) }),
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Slice const& slice) -> hir::Pattern
        {
            auto patterns = std::views::transform(slice.element_patterns, recurse())
                          | std::ranges::to<std::vector>();

            auto const element_type = fresh_general_type_variable(state, ctx.hir, this_range);

            for (hir::Pattern const& pattern : patterns) {
                require_subtype_relationship(
                    ctx,
                    state,
                    pattern.range,
                    ctx.hir.types[pattern.type],
                    ctx.hir.types[element_type.id]);
            }

            return {
                .variant = hir::patt::Slice { std::move(patterns) },
                .type    = ctx.hir.types.push(hir::type::Slice { element_type }),
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Guarded const& guarded) -> hir::Pattern
        {
            auto pattern = recurse(ast().patterns[guarded.guarded_pattern]);
            auto guard = resolve_expression(ctx, state, scope_id, env_id, guarded.guard_expression);

            require_subtype_relationship(
                ctx,
                state,
                guard.range,
                ctx.hir.types[guard.type],
                ctx.hir.types[ctx.constants.type_boolean]);

            return {
                .variant = hir::patt::Guarded {
                    .guarded_pattern  = ctx.hir.patterns.push(std::move(pattern)),
                    .guard_expression = ctx.hir.expressions.push(std::move(guard)),
                },
                .type  = pattern.type,
                .range = this_range,
            };
        }
    };
} // namespace

auto ki::res::resolve_pattern(
    Context&                  ctx,
    Inference_state&          state,
    Scope_id const            scope_id,
    hir::Environment_id const env_id,
    ast::Pattern const&       pattern) -> hir::Pattern
{
    Visitor visitor {
        .ctx        = ctx,
        .state      = state,
        .scope_id   = scope_id,
        .env_id     = env_id,
        .this_range = pattern.range,
    };
    return std::visit(visitor, pattern.variant);
}
