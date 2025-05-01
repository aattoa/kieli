#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&      db;
        Context&           ctx;
        Inference_state&   state;
        db::Environment_id env_id;
        lsp::Range         this_range;

        auto recurse()
        {
            return [&](ast::Pattern const& pattern) -> hir::Pattern {
                return resolve_pattern(db, ctx, state, env_id, pattern);
            };
        }

        auto recurse(ast::Pattern const& pattern) -> hir::Pattern
        {
            return recurse()(pattern);
        }

        auto operator()(db::Integer const& integer) -> hir::Pattern
        {
            auto type = fresh_integral_type_variable(state, ctx.arena.hir, this_range);
            return { .variant = integer, .type_id = type.id, .range = this_range };
        }

        auto operator()(db::Floating const& floating) -> hir::Pattern
        {
            return {
                .variant = floating,
                .type_id = ctx.constants.type_floating,
                .range   = this_range,
            };
        }

        auto operator()(db::Boolean const& boolean) -> hir::Pattern
        {
            return {
                .variant = boolean,
                .type_id = ctx.constants.type_boolean,
                .range   = this_range,
            };
        }

        auto operator()(db::String const& string) -> hir::Pattern
        {
            return {
                .variant = string,
                .type_id = ctx.constants.type_string,
                .range   = this_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            return {
                .variant = hir::Wildcard {},
                .type_id = fresh_general_type_variable(state, ctx.arena.hir, this_range).id,
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Name const& pattern) -> hir::Pattern
        {
            auto const mut  = resolve_mutability(db, ctx, env_id, pattern.mutability);
            auto const type = fresh_general_type_variable(state, ctx.arena.hir, pattern.name.range);

            auto const local_id = ctx.arena.hir.local_variables.push(hir::Local_variable {
                .name    = pattern.name,
                .mut_id  = mut.id,
                .type_id = type.id,
            });
            bind_symbol(db, ctx, env_id, pattern.name, local_id);

            return {
                .variant = hir::patt::Name {
                    .name_id = pattern.name.id,
                    .mut_id  = mut.id,
                    .var_id  = local_id,
                },
                .type_id = type.id,
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
                .type_id = ctx.arena.hir.types.push(hir::type::Tuple { std::move(types) }),
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Slice const& slice) -> hir::Pattern
        {
            auto patterns = std::views::transform(slice.element_patterns, recurse())
                          | std::ranges::to<std::vector>();

            auto const element_type = fresh_general_type_variable(state, ctx.arena.hir, this_range);

            for (hir::Pattern const& pattern : patterns) {
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    pattern.range,
                    ctx.arena.hir.types[pattern.type_id],
                    ctx.arena.hir.types[element_type.id]);
            }

            return {
                .variant = hir::patt::Slice { std::move(patterns) },
                .type_id = ctx.arena.hir.types.push(hir::type::Slice { element_type }),
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Guarded const& guarded) -> hir::Pattern
        {
            auto pattern = recurse(ctx.arena.ast.patterns[guarded.pattern]);
            auto guard   = resolve_expression(db, ctx, state, env_id, guarded.guard);

            require_subtype_relationship(
                db,
                ctx,
                state,
                guard.range,
                ctx.arena.hir.types[guard.type_id],
                ctx.arena.hir.types[ctx.constants.type_boolean]);

            return {
                .variant = hir::patt::Guarded {
                    .pattern = ctx.arena.hir.patterns.push(std::move(pattern)),
                    .guard   = ctx.arena.hir.expressions.push(std::move(guard)),
                },
                .type_id = pattern.type_id,
                .range   = this_range,
            };
        }
    };
} // namespace

auto ki::res::resolve_pattern(
    db::Database&       db,
    Context&            ctx,
    Inference_state&    state,
    db::Environment_id  env_id,
    ast::Pattern const& pattern) -> hir::Pattern
{
    Visitor visitor {
        .db         = db,
        .ctx        = ctx,
        .state      = state,
        .env_id     = env_id,
        .this_range = pattern.range,
    };
    return std::visit(visitor, pattern.variant);
}
