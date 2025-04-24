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
            return db.documents[ctx.doc_id].arena.ast;
        }

        auto error(lsp::Range const range, std::string message) -> hir::Expression
        {
            db::add_error(db, ctx.doc_id, range, std::move(message));
            return error_expression(ctx.constants, this_range);
        }

        auto unsupported() -> hir::Expression
        {
            return error(this_range, "Unsupported expression");
        }

        auto recurse()
        {
            return [&](ast::Expression const& expression) -> hir::Expression {
                return resolve_expression(db, ctx, state, scope_id, env_id, expression);
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
            hir::Symbol symbol = resolve_path(db, ctx, state, scope_id, env_id, path);
            if (auto const* local_id = std::get_if<hir::Local_variable_id>(&symbol)) {
                return {
                    .variant  = hir::expr::Variable_reference { .id = *local_id },
                    .type     = ctx.hir.local_variables[*local_id].type_id,
                    .category = hir::Expression_category::Place,
                    .range    = this_range,
                };
            }
            if (auto const* fun_id = std::get_if<hir::Function_id>(&symbol)) {
                return {
                    .variant  = hir::expr::Function_reference { .id = *fun_id },
                    .type     = resolve_function_signature(db, ctx, *fun_id).function_type.id,
                    .category = ki::hir::Expression_category::Value,
                    .range    = this_range,
                };
            }
            if (std::holds_alternative<db::Error>(symbol)) {
                return error_expression(ctx.constants, this_range);
            }
            auto kind    = hir::describe_symbol_kind(symbol);
            auto message = std::format("Expected an expression, but found {}", kind);
            return error(this_range, std::move(message));
        }

        auto operator()(ast::expr::Array const& array) -> hir::Expression
        {
            auto const element_type = fresh_general_type_variable(state, ctx.hir, this_range);

            std::vector<hir::Expression> elements;
            for (ast::Expression const& element : array.elements) {
                elements.push_back(recurse(element));
                require_subtype_relationship(
                    db,
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
                    auto effect = resolve_expression(db, ctx, state, scope_id, env_id, expression);
                    require_subtype_relationship(
                        db,
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
                    db, ctx, state, scope_id, env_id, ast().expressions[block.result]);

                auto const result_type  = result.type;
                auto const result_range = result.range;

                report_unused(db, ctx, scope_id);
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
                auto pattern = resolve_pattern(
                    db, ctx, state, scope_id, env_id, ast().patterns[arm.pattern]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    scrutinee.range,
                    ctx.hir.types[scrutinee.type],
                    ctx.hir.types[pattern.type]);
                auto expression = recurse(ast().expressions[arm.expression]);
                require_subtype_relationship(
                    db,
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
                = resolve_type(db, ctx, state, scope_id, env_id, ast().types[ascription.type]);
            require_subtype_relationship(
                db,
                ctx,
                state,
                expression.range,
                ctx.hir.types[expression.type],
                ctx.hir.types[ascribed.id]);
            return expression;
        }

        auto operator()(ast::expr::Let const& let) -> hir::Expression
        {
            auto initializer = recurse(ast().expressions[let.initializer]);
            auto pattern
                = resolve_pattern(db, ctx, state, scope_id, env_id, ast().patterns[let.pattern]);

            if (let.type.has_value()) {
                auto type
                    = resolve_type(db, ctx, state, scope_id, env_id, ast().types[let.type.value()]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    pattern.range,
                    ctx.hir.types[pattern.type],
                    ctx.hir.types[type.id]);
            }
            else {
                db::add_type_hint(db, ctx.doc_id, pattern.range.stop, pattern.type);
            }

            require_subtype_relationship(
                db,
                ctx,
                state,
                initializer.range,
                ctx.hir.types[initializer.type],
                ctx.hir.types[pattern.type]);

            return {
                .variant  = hir::expr::Let {
                    .pattern     =  ctx.hir.patterns.push(std::move(pattern)),
                    .type        =  initializer.type,
                    .initializer = ctx.hir.expressions.push(std::move(initializer)),
                },
                .type     = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Type_alias const& alias) -> hir::Expression
        {
            auto type = resolve_type(db, ctx, state, scope_id, env_id, ast().types[alias.type]);

            hir::Local_type local_type {
                .name    = alias.name,
                .type_id = type.id,
                .unused  = true,
            };

            bind_local_type(db, ctx, scope_id, alias.name, std::move(local_type));
            return unit_expression(ctx.constants, this_range);
        }

        auto operator()(ast::expr::Return const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Sizeof const& sizeof_) -> hir::Expression
        {
            auto type = resolve_type(db, ctx, state, scope_id, env_id, ast().types[sizeof_.type]);
            return {
                .variant  = hir::expr::Sizeof { type },
                .type     = fresh_integral_type_variable(state, ctx.hir, this_range).id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Addressof const& addressof) -> hir::Expression
        {
            auto place_expression = recurse(ast().expressions[addressof.expression]);
            auto referenced_type  = place_expression.type;
            auto mutability       = resolve_mutability(db, ctx, scope_id, addressof.mutability);
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
                db,
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
            auto type = fresh_general_type_variable(state, ctx.hir, this_range);
            db::add_type_hint(db, ctx.doc_id, this_range.stop, type.id);
            return {
                .variant  = hir::Wildcard {},
                .type     = type.id,
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
    db::Database&             db,
    Context&                  ctx,
    Inference_state&          state,
    Scope_id const            scope_id,
    hir::Environment_id const env_id,
    ast::Expression const&    expression) -> hir::Expression
{
    Visitor visitor {
        .db         = db,
        .ctx        = ctx,
        .state      = state,
        .scope_id   = scope_id,
        .env_id     = env_id,
        .this_range = expression.range,
    };
    return std::visit(visitor, expression.variant);
}
