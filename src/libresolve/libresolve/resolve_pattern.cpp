#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&      db;
        Context&           ctx;
        Block_state&       state;
        db::Environment_id env_id;
        lsp::Range         this_range;

        auto recurse(ast::Pattern const& pattern) -> hir::Pattern
        {
            return resolve_pattern(db, ctx, state, env_id, pattern);
        }

        auto operator()(db::Integer const& integer) -> hir::Pattern
        {
            return hir::Pattern {
                .variant = integer,
                .type_id = fresh_integral_type_variable(ctx, state, this_range).id,
                .range   = this_range,
            };
        }

        auto operator()(db::Floating const& floating) -> hir::Pattern
        {
            return hir::Pattern {
                .variant = floating,
                .type_id = ctx.constants.type_floating,
                .range   = this_range,
            };
        }

        auto operator()(db::Boolean const& boolean) -> hir::Pattern
        {
            return hir::Pattern {
                .variant = boolean,
                .type_id = ctx.constants.type_boolean,
                .range   = this_range,
            };
        }

        auto operator()(db::String const& string) -> hir::Pattern
        {
            return hir::Pattern {
                .variant = string,
                .type_id = ctx.constants.type_string,
                .range   = this_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            return hir::Pattern {
                .variant = hir::Wildcard {},
                .type_id = fresh_general_type_variable(ctx, state, this_range).id,
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Name const& pattern) -> hir::Pattern
        {
            auto const mut  = resolve_mutability(db, ctx, env_id, pattern.mutability);
            auto const type = fresh_general_type_variable(ctx, state, pattern.name.range);

            auto const local_id = ctx.arena.hir.local_variables.push(
                hir::Local_variable {
                    .name    = pattern.name,
                    .mut_id  = mut.id,
                    .type_id = type.id,
                });
            bind_symbol(db, ctx, env_id, pattern.name, local_id);

            return hir::Pattern {
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
            std::string message = "Constructor pattern resolution has not been implemented";
            db::add_error(db, ctx.doc_id, this_range, std::move(message));

            return hir::Pattern {
                .variant = hir::Wildcard {},
                .type_id = ctx.constants.type_error,
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Tuple const& tuple) -> hir::Pattern
        {
            std::vector<hir::Pattern> fields;
            std::vector<hir::Type>    types;

            fields.reserve(tuple.fields.size());
            types.reserve(tuple.fields.size());

            for (ast::Pattern_id field_id : tuple.fields) {
                hir::Pattern field = recurse(ctx.arena.ast.patterns[field_id]);
                types.push_back(hir::pattern_type(field));
                fields.push_back(std::move(field));
            }

            return hir::Pattern {
                .variant = hir::patt::Tuple { std::move(fields) },
                .type_id = ctx.arena.hir.types.push(hir::type::Tuple { std::move(types) }),
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Slice const& slice) -> hir::Pattern
        {
            std::vector<hir::Pattern> elements;
            elements.reserve(slice.elements.size());

            hir::Type element_type = fresh_general_type_variable(ctx, state, this_range);

            for (ast::Pattern_id element_id : slice.elements) {
                hir::Pattern element = recurse(ctx.arena.ast.patterns[element_id]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    element.range,
                    ctx.arena.hir.types[element.type_id],
                    ctx.arena.hir.types[element_type.id]);
                elements.push_back(std::move(element));
            }

            return hir::Pattern {
                .variant = hir::patt::Slice { std::move(elements) },
                .type_id = ctx.arena.hir.types.push(hir::type::Slice { element_type }),
                .range   = this_range,
            };
        }

        auto operator()(ast::patt::Guarded const& guarded) -> hir::Pattern
        {
            hir::Expression guard = resolve_expression(
                db, ctx, state, env_id, ctx.arena.ast.expressions[guarded.guard]);
            hir::Pattern pattern = recurse(ctx.arena.ast.patterns[guarded.pattern]);

            require_subtype_relationship(
                db,
                ctx,
                state,
                guard.range,
                ctx.arena.hir.types[guard.type_id],
                ctx.arena.hir.types[ctx.constants.type_boolean]);

            return hir::Pattern {
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
    Block_state&        state,
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
