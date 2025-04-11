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

        auto error(lsp::Range const range, std::string message) -> hir::Expression
        {
            db::add_error(ctx.db, state.doc_id, range, std::move(message));
            return error_expression(ctx.constants, this_range);
        }

        auto unsupported() -> hir::Expression
        {
            return error(this_range, "Unsupported expression");
        }

        auto recurse()
        {
            return [&](ast::Expression const& expression) -> hir::Expression {
                return resolve_expression(ctx, state, scope_id, env_id, expression);
            };
        }

        auto recurse(ast::Expression const& expression) -> hir::Expression
        {
            return recurse()(expression);
        }

        auto operator()(db::Integer const& integer) -> hir::Expression
        {
            return {
                .variant  = integer,
                .type     = fresh_integral_type_variable(state, ctx.hir, this_range).id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Floating const& floating) -> hir::Expression
        {
            return {
                .variant  = floating,
                .type     = ctx.constants.type_floating,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Boolean const& boolean) -> hir::Expression
        {
            return {
                .variant  = boolean,
                .type     = ctx.constants.type_boolean,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::String const& string) -> hir::Expression
        {
            return {
                .variant  = string,
                .type     = ctx.constants.type_string,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::Path const& path) -> hir::Expression
        {
            if (path.is_unqualified()) {
                if (auto* bind = find_variable(ctx, scope_id, path.head().name.id)) {
                    bind->unused = false;
                    return {
                        .variant = hir::expr::Variable_reference {
                            .name = bind->name,
                            .tag  = bind->tag,
                        },
                        .type     = bind->type,
                        .category = hir::Expression_category::Place,
                        .range    = this_range,
                    };
                }
                auto const name = ctx.db.string_pool.get(path.head().name.id);
                return error(this_range, std::format("Undeclared identifier: {}", name));
            }
            return unsupported();
        }

        auto operator()(ast::expr::Array const& array) -> hir::Expression
        {
            auto const element_type = fresh_general_type_variable(state, ctx.hir, this_range);

            std::vector<hir::Expression> elements;
            for (ast::Expression const& element : array.elements) {
                elements.push_back(recurse(element));
                require_subtype_relationship(
                    ctx,
                    state,
                    element.range,
                    ctx.hir.types[elements.back().type],
                    ctx.hir.types[element_type.id]);
            }

            auto const length = ctx.hir.expressions.push(hir::Expression {
                .variant  = db::Integer { ssize(elements) },
                .type     = ctx.constants.type_u64,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            });

            auto const type = ctx.hir.types.push(hir::type::Array {
                .element_type = element_type,
                .length       = length,
            });

            return {
                .variant  = hir::expr::Array_literal { std::move(elements) },
                .type     = type,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Tuple const& tuple) -> hir::Expression
        {
            auto fields = std::views::transform(tuple.fields, recurse()) //
                        | std::ranges::to<std::vector>();
            auto types = std::views::transform(fields, hir::expression_type)
                       | std::ranges::to<std::vector>();
            return {
                .variant  = hir::expr::Tuple { .fields = std::move(fields) },
                .type     = ctx.hir.types.push(hir::type::Tuple { std::move(types) }),
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Loop const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Break const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Continue const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Block const& block) -> hir::Expression
        {
            return child_scope(ctx, scope_id, [&](Scope_id scope_id) {
                auto const resolve_effect = [&](ast::Expression const& expression) {
                    auto effect = resolve_expression(ctx, state, scope_id, env_id, expression);
                    require_subtype_relationship(
                        ctx,
                        state,
                        effect.range,
                        ctx.hir.types[effect.type],
                        ctx.hir.types[ctx.constants.type_unit]);
                    return effect;
                };

                auto side_effects = std::views::transform(block.side_effects, resolve_effect)
                                  | std::ranges::to<std::vector>();

                auto result = resolve_expression(
                    ctx, state, scope_id, env_id, ast().expressions[block.result]);

                auto const result_type  = result.type;
                auto const result_range = result.range;

                report_unused(ctx.db, ctx.scopes.index_vector[scope_id]);
                return hir::Expression {
                    .variant = hir::expr::Block {
                        .side_effects = std::move(side_effects),
                        .result       = ctx.hir.expressions.push(std::move(result)),
                    },
                    .type     = result_type,
                    .category = hir::Expression_category::Value,
                    .range    = result_range,
                };
            });
        }

        auto operator()(ast::expr::Function_call const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Tuple_initializer const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Struct_initializer const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Infix_call const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Struct_field const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Tuple_field const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Array_index const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Method_call const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Conditional const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Match const& match) -> hir::Expression
        {
            auto scrutinee   = recurse(ast().expressions[match.scrutinee]);
            auto result_type = fresh_general_type_variable(state, ctx.hir, this_range);

            auto const resolve_arm = [&](ast::Match_arm const& arm) {
                auto pattern
                    = resolve_pattern(ctx, state, scope_id, env_id, ast().patterns[arm.pattern]);
                require_subtype_relationship(
                    ctx,
                    state,
                    scrutinee.range,
                    ctx.hir.types[scrutinee.type],
                    ctx.hir.types[pattern.type]);
                auto expression = recurse(ast().expressions[arm.expression]);
                require_subtype_relationship(
                    ctx,
                    state,
                    expression.range,
                    ctx.hir.types[expression.type],
                    ctx.hir.types[result_type.id]);
                return hir::Match_arm {
                    .pattern    = ctx.hir.patterns.push(std::move(pattern)),
                    .expression = ctx.hir.expressions.push(std::move(expression)),
                };
            };

            return {
                .variant = hir::expr::Match {
                    .arms = std::views::transform(match.arms, resolve_arm) | std::ranges::to<std::vector>(),
                    .scrutinee = ctx.hir.expressions.push(std::move(scrutinee)),
                },
                .type     = result_type.id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Type_ascription const& ascription) -> hir::Expression
        {
            auto expression = recurse(ast().expressions[ascription.expression]);
            auto ascribed
                = resolve_type(ctx, state, scope_id, env_id, ast().types[ascription.type]);
            require_subtype_relationship(
                ctx,
                state,
                expression.range,
                ctx.hir.types[expression.type],
                ctx.hir.types[ascribed.id]);
            return expression;
        }

        auto operator()(ast::expr::Let const& let) -> hir::Expression
        {
            hir::Expression initializer = recurse(ast().expressions[let.initializer]);

            auto type = resolve_type(ctx, state, scope_id, env_id, ast().types[let.type]);
            auto pattern
                = resolve_pattern(ctx, state, scope_id, env_id, ast().patterns[let.pattern]);

            require_subtype_relationship(
                ctx, state, pattern.range, ctx.hir.types[pattern.type], ctx.hir.types[type.id]);
            require_subtype_relationship(
                ctx,
                state,
                initializer.range,
                ctx.hir.types[initializer.type],
                ctx.hir.types[type.id]);

            return {
                .variant  = hir::expr::Let {
                    .pattern     = ctx.hir.patterns.push(std::move(pattern)),
                    .type        = type,
                    .initializer = ctx.hir.expressions.push(std::move(initializer)),
                },
                .type     = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Type_alias const& alias) -> hir::Expression
        {
            auto const type = resolve_type(ctx, state, scope_id, env_id, ast().types[alias.type]);
            bind_type(
                ctx.scopes.index_vector[scope_id],
                hir::Type_bind { .name = alias.name, .type = type.id });
            return unit_expression(ctx.constants, this_range);
        }

        auto operator()(ast::expr::Return const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Sizeof const& sizeof_) -> hir::Expression
        {
            auto const type = resolve_type(ctx, state, scope_id, env_id, ast().types[sizeof_.type]);
            return {
                .variant  = hir::expr::Sizeof { type },
                .type     = fresh_integral_type_variable(state, ctx.hir, this_range).id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Addressof const& addressof) -> hir::Expression
        {
            hir::Expression    place_expression = recurse(ast().expressions[addressof.expression]);
            hir::Type_id const referenced_type  = place_expression.type;
            hir::Mutability    mutability = resolve_mutability(ctx, scope_id, addressof.mutability);
            if (place_expression.category != hir::Expression_category::Place) {
                return error(
                    place_expression.range,
                    "This expression does not identify a place in memory, "
                    "so its address can not be taken");
            }
            return {
                .variant = hir::expr::Addressof {
                    .mutability       = mutability,
                    .expression = ctx.hir.expressions.push(std::move(place_expression)),
                },
                .type = ctx.hir.types.push(hir::type::Reference {
                    .referenced_type { .id = referenced_type, .range = this_range },
                    .mutability = mutability,
                }),
                .category = hir::Expression_category::Value,
                .range = this_range,
            };
        }

        auto operator()(ast::expr::Deref const& dereference) -> hir::Expression
        {
            auto ref_type = fresh_general_type_variable(state, ctx.hir, this_range);
            auto ref_mut  = fresh_mutability_variable(state, ctx.hir, this_range);
            auto ref_expr = recurse(ast().expressions[dereference.expression]);

            require_subtype_relationship(
                ctx,
                state,
                ref_expr.range,
                ctx.hir.types[ref_expr.type],
                hir::type::Reference { .referenced_type = ref_type, .mutability = ref_mut });

            return {
                .variant  = hir::expr::Deref { ctx.hir.expressions.push(std::move(ref_expr)) },
                .type     = ref_type.id,
                .category = hir::Expression_category::Place,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Defer const& defer) -> hir::Expression
        {
            return {
                .variant = hir::expr::Defer {
                    ctx.hir.expressions.push(recurse(ast().expressions[defer.expression])),
                },
                .type     = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Expression
        {
            return {
                .variant  = hir::Wildcard {},
                .type     = fresh_general_type_variable(state, ctx.hir, this_range).id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Error const&) const -> hir::Expression
        {
            return error_expression(ctx.constants, this_range);
        }
    };
} // namespace

auto ki::res::resolve_expression(
    Context&                  ctx,
    Inference_state&          state,
    Scope_id const            scope_id,
    hir::Environment_id const env_id,
    ast::Expression const&    expression) -> hir::Expression
{
    Visitor visitor {
        .ctx        = ctx,
        .state      = state,
        .scope_id   = scope_id,
        .env_id     = env_id,
        .this_range = expression.range,
    };
    return std::visit(visitor, expression.variant);
}
