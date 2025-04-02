#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki::resolve;

namespace {
    struct Expression_resolution_visitor {
        Context&               ctx;
        Inference_state&       state;
        hir::Scope_id          scope_id;
        hir::Environment_id    env_id;
        ast::Expression const& this_expression;

        auto ast() -> ast::Arena&
        {
            return ctx.documents.at(state.doc_id).ast;
        }

        auto error(ki::Range const range, std::string message) -> hir::Expression
        {
            ki::add_error(ctx.db, state.doc_id, range, std::move(message));
            return error_expression(ctx.constants, this_expression.range);
        }

        auto unsupported() -> hir::Expression
        {
            return error(this_expression.range, "Unsupported expression");
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

        auto operator()(ki::Integer const& integer) -> hir::Expression
        {
            return {
                .variant  = integer,
                .type     = fresh_integral_type_variable(state, ctx.hir, this_expression.range).id,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ki::Floating const& floating) -> hir::Expression
        {
            return {
                .variant  = floating,
                .type     = ctx.constants.type_floating,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ki::Boolean const& boolean) -> hir::Expression
        {
            return {
                .variant  = boolean,
                .type     = ctx.constants.type_boolean,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ki::String const& string) -> hir::Expression
        {
            return {
                .variant  = string,
                .type     = ctx.constants.type_string,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::Path const& path) -> hir::Expression
        {
            if (path.is_unqualified()) {
                if (auto* bind = find_variable(ctx, scope_id, path.head().name.id)) {
                    bind->unused = false;
                    return {
                        .variant = hir::expression::Variable_reference {
                            .name = bind->name,
                            .tag  = bind->tag,
                        },
                        .type     = bind->type,
                        .category = hir::Expression_category::Place,
                        .range    = this_expression.range,
                    };
                }
                auto const name = ctx.db.string_pool.get(path.head().name.id);
                return error(this_expression.range, std::format("Undeclared identifier: {}", name));
            }
            return unsupported();
        }

        auto operator()(ast::expression::Array const& array) -> hir::Expression
        {
            hir::Type const element_type
                = fresh_general_type_variable(state, ctx.hir, this_expression.range);

            std::vector<hir::Expression> elements;
            for (ast::Expression const& element : array.elements) {
                elements.push_back(recurse(element));
                require_subtype_relationship(
                    ctx,
                    state,
                    element.range,
                    ctx.hir.type[elements.back().type],
                    ctx.hir.type[element_type.id]);
            }

            auto const length = ctx.hir.expr.push(hir::Expression {
                .variant  = ki::Integer { ssize(elements) },
                .type     = ctx.constants.type_u64,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            });

            auto const type = ctx.hir.type.push(hir::type::Array {
                .element_type = element_type,
                .length       = length,
            });

            return {
                .variant  = hir::expression::Array_literal { std::move(elements) },
                .type     = type,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression
        {
            auto fields = std::views::transform(tuple.fields, recurse()) //
                        | std::ranges::to<std::vector>();
            auto types = std::views::transform(fields, hir::expression_type)
                       | std::ranges::to<std::vector>();
            return {
                .variant  = hir::expression::Tuple { .fields = std::move(fields) },
                .type     = ctx.hir.type.push(hir::type::Tuple { std::move(types) }),
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::expression::Loop const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Break const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Continue const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Block const& block) -> hir::Expression
        {
            return child_scope(ctx, scope_id, [&](hir::Scope_id scope_id) {
                auto const resolve_effect = [&](ast::Expression const& expression) {
                    auto effect = resolve_expression(ctx, state, scope_id, env_id, expression);
                    require_subtype_relationship(
                        ctx,
                        state,
                        effect.range,
                        ctx.hir.type[effect.type],
                        ctx.hir.type[ctx.constants.type_unit]);
                    return effect;
                };

                auto side_effects = std::views::transform(block.side_effects, resolve_effect)
                                  | std::ranges::to<std::vector>();

                auto result
                    = resolve_expression(ctx, state, scope_id, env_id, ast().expr[block.result]);

                auto const result_type  = result.type;
                auto const result_range = result.range;

                report_unused(ctx.db, ctx.info.scopes.index_vector[scope_id]);
                return hir::Expression {
                    .variant = hir::expression::Block {
                        .side_effects = std::move(side_effects),
                        .result       = ctx.hir.expr.push(std::move(result)),
                    },
                    .type     = result_type,
                    .category = hir::Expression_category::Value,
                    .range    = result_range,
                };
            });
        }

        auto operator()(ast::expression::Function_call const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Tuple_initializer const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Struct_initializer const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Infix_call const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Struct_field const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Tuple_field const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Array_index const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Method_call const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Conditional const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Match const& match) -> hir::Expression
        {
            ast::Arena& ast = this->ast();

            auto scrutinee   = recurse(ast.expr[match.scrutinee]);
            auto result_type = fresh_general_type_variable(state, ctx.hir, this_expression.range);

            auto const resolve_case = [&](ast::Match_case const& match_case) {
                auto pattern
                    = resolve_pattern(ctx, state, scope_id, env_id, ast.patt[match_case.pattern]);
                require_subtype_relationship(
                    ctx,
                    state,
                    scrutinee.range,
                    ctx.hir.type[scrutinee.type],
                    ctx.hir.type[pattern.type]);
                hir::Expression expression = recurse(ast.expr[match_case.expression]);
                require_subtype_relationship(
                    ctx,
                    state,
                    expression.range,
                    ctx.hir.type[expression.type],
                    ctx.hir.type[result_type.id]);
                return hir::Match_case {
                    .pattern    = ctx.hir.patt.push(std::move(pattern)),
                    .expression = ctx.hir.expr.push(std::move(expression)),
                };
            };

            return {
                .variant = hir::expression::Match {
                    .cases = std::views::transform(match.cases, resolve_case) | std::ranges::to<std::vector>(),
                    .expression = ctx.hir.expr.push(std::move(scrutinee)),
                },
                .type     = result_type.id,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::expression::Type_ascription const& ascription) -> hir::Expression
        {
            ast::Arena& ast = this->ast();

            auto expression = recurse(ast.expr[ascription.expression]);
            auto ascribed   = resolve_type(ctx, state, scope_id, env_id, ast.type[ascription.type]);
            require_subtype_relationship(
                ctx,
                state,
                expression.range,
                ctx.hir.type[expression.type],
                ctx.hir.type[ascribed.id]);
            return expression;
        }

        auto operator()(ast::expression::Let const& let) -> hir::Expression
        {
            ast::Arena& ast = this->ast();

            hir::Expression initializer = recurse(ast.expr[let.initializer]);

            auto type    = resolve_type(ctx, state, scope_id, env_id, ast.type[let.type]);
            auto pattern = resolve_pattern(ctx, state, scope_id, env_id, ast.patt[let.pattern]);

            require_subtype_relationship(
                ctx, state, pattern.range, ctx.hir.type[pattern.type], ctx.hir.type[type.id]);
            require_subtype_relationship(
                ctx,
                state,
                initializer.range,
                ctx.hir.type[initializer.type],
                ctx.hir.type[type.id]);

            return {
                .variant  = hir::expression::Let {
                    .pattern     = ctx.hir.patt.push(std::move(pattern)),
                    .type        = type,
                    .initializer = ctx.hir.expr.push(std::move(initializer)),
                },
                .type     = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::expression::Type_alias const& alias) -> hir::Expression
        {
            auto const type = resolve_type(ctx, state, scope_id, env_id, ast().type[alias.type]);
            bind_type(
                ctx.info.scopes.index_vector[scope_id],
                Type_bind { .name = alias.name, .type = type.id });
            return unit_expression(ctx.constants, this_expression.range);
        }

        auto operator()(ast::expression::Ret const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression
        {
            auto const type = resolve_type(ctx, state, scope_id, env_id, ast().type[sizeof_.type]);
            return {
                .variant  = hir::expression::Sizeof { type },
                .type     = fresh_integral_type_variable(state, ctx.hir, this_expression.range).id,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::expression::Addressof const& addressof) -> hir::Expression
        {
            hir::Expression    place_expression = recurse(ast().expr[addressof.place_expression]);
            hir::Type_id const referenced_type  = place_expression.type;
            hir::Mutability    mutability = resolve_mutability(ctx, scope_id, addressof.mutability);
            if (place_expression.category != hir::Expression_category::Place) {
                return error(
                    place_expression.range,
                    "This expression does not identify a place in memory, "
                    "so its address can not be taken");
            }
            return {
                .variant = hir::expression::Addressof {
                    .mutability       = mutability,
                    .place_expression = ctx.hir.expr.push(std::move(place_expression)),
                },
                .type = ctx.hir.type.push(hir::type::Reference {
                    .referenced_type { .id = referenced_type, .range = this_expression.range },
                    .mutability = mutability,
                }),
                .category = hir::Expression_category::Value,
                .range = this_expression.range,
            };
        }

        auto operator()(ast::expression::Dereference const& dereference) -> hir::Expression
        {
            auto ref_type = fresh_general_type_variable(state, ctx.hir, this_expression.range);
            auto ref_mutability = fresh_mutability_variable(state, ctx.hir, this_expression.range);
            auto ref_expression = recurse(ast().expr[dereference.reference_expression]);

            require_subtype_relationship(
                ctx,
                state,
                ref_expression.range,
                ctx.hir.type[ref_expression.type],
                hir::type::Reference {
                    .referenced_type = ref_type,
                    .mutability      = ref_mutability,
                });

            return {
                .variant = hir::expression::Dereference {
                    ctx.hir.expr.push(std::move(ref_expression)),
                },
                .type     = ref_type.id,
                .category = hir::Expression_category::Place,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::expression::Defer const& defer) -> hir::Expression
        {
            return {
                .variant = hir::expression::Defer {
                    ctx.hir.expr.push(
                        recurse(ast().expr[defer.effect_expression])),
                },
                .type     = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Expression
        {
            return {
                .variant  = hir::Wildcard {},
                .type     = fresh_general_type_variable(state, ctx.hir, this_expression.range).id,
                .category = hir::Expression_category::Value,
                .range    = this_expression.range,
            };
        }

        auto operator()(ki::Error const&) -> hir::Expression
        {
            return error_expression(ctx.constants, this_expression.range);
        }
    };
} // namespace

auto ki::resolve::resolve_expression(
    Context&                  ctx,
    Inference_state&          state,
    hir::Scope_id const       scope_id,
    hir::Environment_id const env_id,
    ast::Expression const&    expression) -> hir::Expression
{
    Expression_resolution_visitor visitor {
        .ctx             = ctx,
        .state           = state,
        .scope_id        = scope_id,
        .env_id          = env_id,
        .this_expression = expression,
    };
    return std::visit(visitor, expression.variant);
}
