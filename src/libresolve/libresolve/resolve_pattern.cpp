#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&       db;
        Context&            ctx;
        Inference_state&    state;
        Scope_id            scope_id;
        hir::Environment_id env_id;
        lsp::Range          this_range;

        auto ast() -> ast::Arena&
        {
            return db.documents[state.doc_id].ast;
        }

        auto recurse()
        {
            return [&](ast::Pattern const& pattern) -> hir::Pattern {
                return resolve_pattern(db, ctx, state, scope_id, env_id, pattern);
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
            auto const mut  = resolve_mutability(db, ctx, scope_id, pattern.mutability);
            auto const type = fresh_general_type_variable(state, ctx.hir, pattern.name.range);

            hir::Local_variable variable {
                .name    = pattern.name,
                .mut_id  = mut.id,
                .type_id = type.id,
                .unused  = true,
            };

            return {
                .variant = hir::patt::Name {
                    .name_id = pattern.name.id,
                    .mut_id  = mut.id,
                    .var_id  = bind_local_variable(db, ctx, scope_id, pattern.name, variable),
                },
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
                    db,
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
            auto pattern = recurse(ast().patterns[guarded.pattern]);
            auto guard   = resolve_expression(db, ctx, state, scope_id, env_id, guarded.guard);

            require_subtype_relationship(
                db,
                ctx,
                state,
                guard.range,
                ctx.hir.types[guard.type],
                ctx.hir.types[ctx.constants.type_boolean]);

            return {
                .variant = hir::patt::Guarded {
                    .pattern = ctx.hir.patterns.push(std::move(pattern)),
                    .guard   = ctx.hir.expressions.push(std::move(guard)),
                },
                .type  = pattern.type,
                .range = this_range,
            };
        }
    };
} // namespace

auto ki::res::resolve_pattern(
    db::Database&       db,
    Context&            ctx,
    Inference_state&    state,
    Scope_id            scope_id,
    hir::Environment_id env_id,
    ast::Pattern const& pattern) -> hir::Pattern
{
    Visitor visitor {
        .db         = db,
        .ctx        = ctx,
        .state      = state,
        .scope_id   = scope_id,
        .env_id     = env_id,
        .this_range = pattern.range,
    };
    return std::visit(visitor, pattern.variant);
}
