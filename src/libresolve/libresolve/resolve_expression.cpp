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
                return resolve_expression(db, ctx, state, env_id, expression);
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
                .type_id  = fresh_integral_type_variable(state, ctx.arena.hir, this_range).id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Floating const& floating) -> hir::Expression
        {
            return {
                .variant  = floating,
                .type_id  = ctx.constants.type_floating,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Boolean const& boolean) -> hir::Expression
        {
            return {
                .variant  = boolean,
                .type_id  = ctx.constants.type_boolean,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::String const& string) -> hir::Expression
        {
            return {
                .variant  = string,
                .type_id  = ctx.constants.type_string,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::Path const& path) -> hir::Expression
        {
            db::Symbol_id symbol_id = resolve_path(db, ctx, state, env_id, path);
            db::Symbol&   symbol    = ctx.arena.symbols[symbol_id];

            if (auto const* local_id = std::get_if<hir::Local_variable_id>(&symbol.variant)) {
                return {
                    .variant  = hir::expr::Variable_reference { .id = *local_id },
                    .type_id  = ctx.arena.hir.local_variables[*local_id].type_id,
                    .category = hir::Expression_category::Place,
                    .range    = this_range,
                };
            }
            if (auto const* fun_id = std::get_if<hir::Function_id>(&symbol.variant)) {
                return {
                    .variant  = hir::expr::Function_reference { .id = *fun_id },
                    .type_id  = resolve_function_signature(db, ctx, *fun_id).function_type.id,
                    .category = hir::Expression_category::Value,
                    .range    = this_range,
                };
            }
            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return error_expression(ctx.constants, this_range);
            }

            auto kind    = db::describe_symbol_kind(symbol.variant);
            auto message = std::format("Expected an expression, but found {}", kind);
            return error(this_range, std::move(message));
        }

        auto operator()(ast::expr::Array const& array) -> hir::Expression
        {
            auto const element_type = fresh_general_type_variable(state, ctx.arena.hir, this_range);

            std::vector<hir::Expression> elements;
            for (ast::Expression const& element : array.elements) {
                elements.push_back(recurse(element));
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    element.range,
                    ctx.arena.hir.types[elements.back().type_id],
                    ctx.arena.hir.types[element_type.id]);
            }

            auto const length = ctx.arena.hir.expressions.push(hir::Expression {
                .variant  = db::Integer { ssize(elements) },
                .type_id  = ctx.constants.type_u64,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            });

            auto const type = ctx.arena.hir.types.push(hir::type::Array {
                .element_type = element_type,
                .length       = length,
            });

            return {
                .variant  = hir::expr::Array { std::move(elements) },
                .type_id  = type,
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
                .type_id  = ctx.arena.hir.types.push(hir::type::Tuple { std::move(types) }),
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
            auto block_env_id = new_scope(ctx, env_id);

            auto const resolve_effect = [&](ast::Expression const& expression) {
                auto effect = resolve_expression(db, ctx, state, block_env_id, expression);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    effect.range,
                    ctx.arena.hir.types[effect.type_id],
                    hir::type::Tuple {});
                return effect;
            };

            auto side_effects = block.effects //
                              | std::views::transform(resolve_effect)
                              | std::ranges::to<std::vector>();

            auto result = resolve_expression(
                db, ctx, state, block_env_id, ctx.arena.ast.expressions[block.result]);

            auto result_type  = result.type_id;
            auto result_range = result.range;

            report_unused(db, ctx, block_env_id);
            recycle_environment(ctx, block_env_id);

            return hir::Expression {
                .variant = hir::expr::Block {
                    .effects = std::move(side_effects),
                    .result  = ctx.arena.hir.expressions.push(std::move(result)),
                },
                .type_id  = result_type,
                .category = hir::Expression_category::Value,
                .range    = result_range,
            };
        }

        auto operator()(ast::expr::Function_call const& call) -> hir::Expression
        {
            auto invocable = recurse(ctx.arena.ast.expressions[call.invocable]);

            if (auto const* ref = std::get_if<hir::expr::Function_reference>(&invocable.variant)) {
                auto& signature = resolve_function_signature(db, ctx, ref->id);

                if (signature.parameters.size() != call.arguments.size()) {
                    auto message = std::format(
                        "'{}' has {} parameters, but {} arguments were supplied",
                        db.string_pool.get(signature.name.id),
                        signature.parameters.size(),
                        call.arguments.size());
                    return error(this_range, std::move(message));
                }

                std::vector<hir::Expression_id> arguments;
                arguments.reserve(signature.parameters.size());

                for (std::size_t i = 0; i != signature.parameters.size(); ++i) {
                    auto const& parameter = signature.parameters.at(i);
                    auto        argument = recurse(ctx.arena.ast.expressions[call.arguments.at(i)]);

                    require_subtype_relationship(
                        db,
                        ctx,
                        state,
                        argument.range,
                        ctx.arena.hir.types[argument.type_id],
                        ctx.arena.hir.types[parameter.type.id]);
                    arguments.push_back(ctx.arena.hir.expressions.push(std::move(argument)));

                    db::add_param_hint(db, ctx.doc_id, argument.range.start, parameter.pattern_id);
                }

                return {
                    .variant  = hir::expr::Function_call {
                        .invocable = ctx.arena.hir.expressions.push(std::move(invocable)),
                        .arguments = std::move(arguments),
                    },
                    .type_id  = signature.return_type.id,
                    .category = hir::Expression_category::Value,
                    .range    = this_range,
                };
            }

            std::vector<hir::Expression_id> arguments;
            arguments.reserve(call.arguments.size());

            std::vector<hir::Type> parameter_types;
            parameter_types.reserve(call.arguments.size());

            auto result_type = fresh_general_type_variable(state, ctx.arena.hir, this_range);

            for (auto const& arg_id : call.arguments) {
                auto range = ctx.arena.ast.expressions[arg_id].range;
                parameter_types.push_back(fresh_general_type_variable(state, ctx.arena.hir, range));
            }

            // Unify the invocable type with a synthetic function type.
            // The real parameter types can be captured this way.
            // This greatly improves argument type mismatch error messages.
            require_subtype_relationship(
                db,
                ctx,
                state,
                this_range,
                ctx.arena.hir.types[invocable.type_id],
                hir::type::Function {
                    .parameter_types = parameter_types,
                    .return_type     = result_type,
                });

            for (auto const& [type, arg_id] : std::views::zip(parameter_types, call.arguments)) {
                auto argument = recurse(ctx.arena.ast.expressions[arg_id]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    argument.range,
                    ctx.arena.hir.types[argument.type_id],
                    ctx.arena.hir.types[type.id]);
                arguments.push_back(ctx.arena.hir.expressions.push(std::move(argument)));
            }

            return {
                .variant  = hir::expr::Function_call {
                    .invocable = ctx.arena.hir.expressions.push(std::move(invocable)),
                    .arguments = std::move(arguments),
                },
                .type_id  = result_type.id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Tuple_init const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Struct_init const&) -> hir::Expression
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

        auto operator()(ast::expr::Conditional const& conditional) -> hir::Expression
        {
            auto condition    = recurse(ctx.arena.ast.expressions[conditional.condition]);
            auto true_branch  = recurse(ctx.arena.ast.expressions[conditional.true_branch]);
            auto false_branch = recurse(ctx.arena.ast.expressions[conditional.false_branch]);

            require_subtype_relationship(
                db,
                ctx,
                state,
                condition.range,
                ctx.arena.hir.types[condition.type_id],
                hir::type::Boolean {});

            require_subtype_relationship(
                db,
                ctx,
                state,
                true_branch.range,
                ctx.arena.hir.types[true_branch.type_id],
                ctx.arena.hir.types[false_branch.type_id]);

            auto result_type = false_branch.type_id;

            auto arm = [&](bool boolean, hir::Expression branch) {
                return hir::Match_arm {
                    .pattern    = ctx.arena.hir.patterns.push(hir::Pattern {
                           .variant = db::Boolean { boolean },
                           .type_id = ctx.constants.type_boolean,
                           .range   = condition.range,
                    }),
                    .expression = ctx.arena.hir.expressions.push(std::move(branch)),
                };
            };

            hir::expr::Match match {
                .arms      = utl::to_vector({
                    arm(true, std::move(true_branch)),
                    arm(false, std::move(false_branch)),
                }),
                .scrutinee = ctx.arena.hir.expressions.push(std::move(condition)),
            };

            return {
                .variant  = std::move(match),
                .type_id  = result_type,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Match const& match) -> hir::Expression
        {
            auto scrutinee   = recurse(ctx.arena.ast.expressions[match.scrutinee]);
            auto result_type = fresh_general_type_variable(state, ctx.arena.hir, this_range);

            auto const resolve_arm = [&](ast::Match_arm const& arm) {
                auto pattern
                    = resolve_pattern(db, ctx, state, env_id, ctx.arena.ast.patterns[arm.pattern]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    scrutinee.range,
                    ctx.arena.hir.types[scrutinee.type_id],
                    ctx.arena.hir.types[pattern.type_id]);
                auto expression = recurse(ctx.arena.ast.expressions[arm.expression]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    expression.range,
                    ctx.arena.hir.types[expression.type_id],
                    ctx.arena.hir.types[result_type.id]);
                return hir::Match_arm {
                    .pattern    = ctx.arena.hir.patterns.push(std::move(pattern)),
                    .expression = ctx.arena.hir.expressions.push(std::move(expression)),
                };
            };

            return {
                .variant = hir::expr::Match {
                    .arms = std::views::transform(match.arms, resolve_arm) | std::ranges::to<std::vector>(),
                    .scrutinee = ctx.arena.hir.expressions.push(std::move(scrutinee)),
                },
                .type_id  = result_type.id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Ascription const& ascription) -> hir::Expression
        {
            auto expression = recurse(ctx.arena.ast.expressions[ascription.expression]);
            auto ascribed
                = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[ascription.type]);
            require_subtype_relationship(
                db,
                ctx,
                state,
                expression.range,
                ctx.arena.hir.types[expression.type_id],
                ctx.arena.hir.types[ascribed.id]);
            return expression;
        }

        auto operator()(ast::expr::Let const& let) -> hir::Expression
        {
            auto initializer = recurse(ctx.arena.ast.expressions[let.initializer]);
            auto pattern
                = resolve_pattern(db, ctx, state, env_id, ctx.arena.ast.patterns[let.pattern]);

            if (let.type.has_value()) {
                auto type
                    = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[let.type.value()]);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    pattern.range,
                    ctx.arena.hir.types[pattern.type_id],
                    ctx.arena.hir.types[type.id]);
            }
            else {
                db::add_type_hint(db, ctx.doc_id, pattern.range.stop, pattern.type_id);
            }

            require_subtype_relationship(
                db,
                ctx,
                state,
                initializer.range,
                ctx.arena.hir.types[initializer.type_id],
                ctx.arena.hir.types[pattern.type_id]);

            return {
                .variant  = hir::expr::Let {
                    .pattern     = ctx.arena.hir.patterns.push(std::move(pattern)),
                    .type        = initializer.type_id,
                    .initializer = ctx.arena.hir.expressions.push(std::move(initializer)),
                },
                .type_id  = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Type_alias const& alias) -> hir::Expression
        {
            auto const type = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[alias.type]);
            auto const local_id = ctx.arena.hir.local_types.push(hir::Local_type {
                .name    = alias.name,
                .type_id = type.id,
            });
            bind_symbol(db, ctx, env_id, alias.name, local_id);
            return unit_expression(ctx.constants, this_range);
        }

        auto operator()(ast::expr::Return const&) -> hir::Expression
        {
            return unsupported();
        }

        auto operator()(ast::expr::Sizeof const& sizeof_) -> hir::Expression
        {
            auto type = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[sizeof_.type]);
            return {
                .variant  = hir::expr::Sizeof { type },
                .type_id  = fresh_integral_type_variable(state, ctx.arena.hir, this_range).id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Addressof const& addressof) -> hir::Expression
        {
            auto place_expression = recurse(ctx.arena.ast.expressions[addressof.expression]);
            auto referenced_type  = place_expression.type_id;
            auto mutability       = resolve_mutability(db, ctx, env_id, addressof.mutability);
            if (place_expression.category != hir::Expression_category::Place) {
                return error(
                    place_expression.range,
                    "This expression does not identify a place in memory, "
                    "so its address can not be taken");
            }
            return {
                .variant = hir::expr::Addressof {
                    .mutability = mutability,
                    .expression = ctx.arena.hir.expressions.push(std::move(place_expression)),
                },
                .type_id = ctx.arena.hir.types.push(hir::type::Reference {
                    .referenced_type = hir::Type { .id = referenced_type, .range = this_range },
                    .mutability      = mutability,
                }),
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Deref const& dereference) -> hir::Expression
        {
            auto ref_type = fresh_general_type_variable(state, ctx.arena.hir, this_range);
            auto ref_mut  = fresh_mutability_variable(state, ctx.arena.hir, this_range);
            auto ref_expr = recurse(ctx.arena.ast.expressions[dereference.expression]);

            require_subtype_relationship(
                db,
                ctx,
                state,
                ref_expr.range,
                ctx.arena.hir.types[ref_expr.type_id],
                hir::type::Reference { .referenced_type = ref_type, .mutability = ref_mut });

            return {
                .variant = hir::expr::Deref { ctx.arena.hir.expressions.push(std::move(ref_expr)) },
                .type_id = ref_type.id,
                .category = hir::Expression_category::Place,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Defer const& defer) -> hir::Expression
        {
            return {
                .variant = hir::expr::Defer {
                    ctx.arena.hir.expressions.push(recurse(ctx.arena.ast.expressions[defer.expression])),
                },
                .type_id  = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Expression
        {
            auto type = fresh_general_type_variable(state, ctx.arena.hir, this_range);
            db::add_type_hint(db, ctx.doc_id, this_range.stop, type.id);
            return {
                .variant  = hir::Wildcard {},
                .type_id  = type.id,
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
    db::Database&          db,
    Context&               ctx,
    Inference_state&       state,
    db::Environment_id     env_id,
    ast::Expression const& expression) -> hir::Expression
{
    Visitor visitor {
        .db         = db,
        .ctx        = ctx,
        .state      = state,
        .env_id     = env_id,
        .this_range = expression.range,
    };
    return std::visit(visitor, expression.variant);
}
