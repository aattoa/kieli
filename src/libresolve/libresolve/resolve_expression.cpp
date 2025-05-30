#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto get_constructor_id(db::Database& db, Context& ctx, db::Symbol const& symbol)
        -> std::optional<hir::Constructor_id>
    {
        if (auto const* id = std::get_if<hir::Constructor_id>(&symbol.variant)) {
            return *id;
        }
        if (auto const* id = std::get_if<hir::Structure_id>(&symbol.variant)) {
            return resolve_structure(db, ctx, *id).constructor_id;
        }
        return std::nullopt;
    }

    struct Visitor {
        db::Database&      db;
        Context&           ctx;
        Block_state&       state;
        db::Environment_id env_id;
        lsp::Range         this_range;

        auto error(lsp::Range range, std::string message) -> hir::Expression
        {
            db::add_error(db, ctx.doc_id, range, std::move(message));
            return error_expression(ctx.constants, this_range);
        }

        auto unknown_type_error(lsp::Range range) -> hir::Expression
        {
            return error(range, "This type is unknown at this point, annotations are required");
        }

        auto field_error(hir::Expression const& base, lsp::Range range, std::string_view field)
            -> hir::Expression
        {
            if (std::holds_alternative<db::Error>(ctx.arena.hir.types[base.type_id])) {
                return error_expression(ctx.constants, this_range);
            }
            if (std::holds_alternative<hir::type::Variable>(ctx.arena.hir.types[base.type_id])) {
                return unknown_type_error(base.range);
            }
            auto type = hir::to_string(ctx.arena.hir, db.string_pool, base.type_id);
            return error(range, std::format("No field '{}' on type {}", field, type));
        }

        auto recurse(ast::Expression const& expression) -> hir::Expression
        {
            return resolve_expression(db, ctx, state, env_id, expression);
        }

        auto operator()(db::Integer const& integer) -> hir::Expression
        {
            return {
                .variant  = integer,
                .type_id  = fresh_integral_type_variable(ctx, state, this_range).id,
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
            db::Symbol& symbol = ctx.arena.symbols[resolve_path(db, ctx, state, env_id, path)];
            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return error_expression(ctx.constants, this_range);
            }

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

            if (auto const ctor_id = get_constructor_id(db, ctx, symbol)) {
                auto const& ctor = ctx.arena.hir.constructors[ctor_id.value()];

                auto const visitor = utl::Overload {
                    [&](hir::Unit_constructor const&) -> hir::Expression {
                        return {
                            .variant  = hir::expr::Initializer {
                                .constructor = ctor_id.value(),
                                .arguments   = {},
                            },
                            .type_id  = ctor.owner_type_id,
                            .category = hir::Expression_category::Value,
                            .range    = this_range,
                        };
                    },
                    [&](hir::Tuple_constructor const& tuple) -> hir::Expression {
                        return {
                            .variant  = hir::expr::Constructor_reference { .id = ctor_id.value() },
                            .type_id  = tuple.function_type_id,
                            .category = hir::Expression_category::Value,
                            .range    = this_range,
                        };
                    },
                    [&](hir::Struct_constructor const&) -> hir::Expression {
                        auto message = std::format(
                            "Expected an expression, but '{}' is a struct constructor",
                            db.string_pool.get(ctor.name.id));
                        return error(path.head().name.range, std::move(message));
                    },
                };
                return std::visit(visitor, ctor.body);
            }

            auto message = std::format(
                "Expected an expression, but '{}' is {}",
                db.string_pool.get(symbol.name.id),
                db::describe_symbol_kind(symbol.variant));
            return error(path.head().name.range, std::move(message));
        }

        auto operator()(ast::expr::Array const& array) -> hir::Expression
        {
            auto const element_type = fresh_general_type_variable(ctx, state, this_range);

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

            auto const type_id = ctx.arena.hir.types.push(hir::type::Array {
                .element_type = element_type,
                .length       = length,
            });

            return {
                .variant  = hir::expr::Array { std::move(elements) },
                .type_id  = type_id,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Tuple const& tuple) -> hir::Expression
        {
            auto fields = std::ranges::to<std::vector>(
                std::views::transform(tuple.fields, std::bind_front(&Visitor::recurse, this)));
            auto types = std::ranges::to<std::vector>( //
                std::views::transform(fields, hir::expression_type));
            return {
                .variant  = hir::expr::Tuple { std::move(fields) },
                .type_id  = ctx.arena.hir.types.push(hir::type::Tuple { std::move(types) }),
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Loop const& loop) -> hir::Expression
        {
            auto old_loop_info = state.loop_info;

            state.loop_info = Loop_info { .result_type_id = std::nullopt, .source = loop.source };
            auto body       = recurse(ctx.arena.ast.expressions[loop.body]);
            auto type_id    = state.loop_info.value().result_type_id;

            state.loop_info = old_loop_info;

            return {
                .variant  = hir::expr::Loop { ctx.arena.hir.expressions.push(std::move(body)) },
                .type_id  = type_id.value_or(ctx.constants.type_unit),
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Break const& brk) -> hir::Expression
        {
            if (not state.loop_info.has_value()) {
                return error(this_range, "'break' can not be used outside of a loop");
            }

            auto result = recurse(ctx.arena.ast.expressions[brk.result]);

            if (state.loop_info.value().result_type_id.has_value()) {
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    result.range,
                    ctx.arena.hir.types[result.type_id],
                    ctx.arena.hir.types[state.loop_info.value().result_type_id.value()]);
            }
            else {
                state.loop_info.value().result_type_id = result.type_id;
            }

            return {
                .variant  = hir::expr::Break { ctx.arena.hir.expressions.push(std::move(result)) },
                .type_id  = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Continue const&) -> hir::Expression
        {
            if (not state.loop_info.has_value()) {
                return error(this_range, "'continue' can not be used outside of a loop");
            }
            return {
                .variant  = hir::expr::Continue {},
                .type_id  = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
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

            auto result_type = fresh_general_type_variable(ctx, state, this_range);

            for (auto const& arg_id : call.arguments) {
                auto range = ctx.arena.ast.expressions[arg_id].range;
                parameter_types.push_back(fresh_general_type_variable(ctx, state, range));
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

        auto operator()(ast::expr::Struct_init const& init) -> hir::Expression
        {
            db::Symbol& symbol = ctx.arena.symbols[resolve_path(db, ctx, state, env_id, init.path)];

            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return error_expression(ctx.constants, this_range);
            }

            auto const ctor_id = get_constructor_id(db, ctx, symbol);
            if (not ctor_id.has_value()) {
                auto message = std::format(
                    "Expected a struct constructor, but '{}' is {}",
                    db.string_pool.get(symbol.name.id),
                    db::describe_symbol_kind(symbol.variant));
                return error(init.path.head().name.range, std::move(message));
            }

            auto const& ctor = ctx.arena.hir.constructors[ctor_id.value()];
            auto const* body = std::get_if<hir::Struct_constructor>(&ctor.body);

            if (not body) {
                auto message = std::format(
                    "Expected a struct constructor, but '{}' is a {} constructor",
                    db.string_pool.get(symbol.name.id),
                    hir::describe_constructor(ctor.body));
                return error(init.path.head().name.range, std::move(message));
            }

            std::unordered_map<std::size_t, hir::Expression> map;

            for (ast::Field_init const& field : init.fields) {
                if (auto const it = body->fields.find(field.name.id); it != body->fields.end()) {
                    hir::Field_info& info = ctx.arena.hir.fields[it->second];
                    db::add_reference(db, ctx.doc_id, lsp::read(field.name.range), info.symbol_id);

                    auto expression = recurse(ctx.arena.ast.expressions[field.expression]);
                    require_subtype_relationship(
                        db,
                        ctx,
                        state,
                        expression.range,
                        ctx.arena.hir.types[expression.type_id],
                        ctx.arena.hir.types[info.type.id]);

                    if (map.contains(info.field_index)) {
                        auto message = std::format(
                            "Duplicate initializer for field '{}'",
                            db.string_pool.get(field.name.id));
                        db::add_error(db, ctx.doc_id, field.name.range, std::move(message));
                    }

                    map.insert_or_assign(info.field_index, std::move(expression));
                }
                else {
                    auto message = std::format(
                        "No field '{}' on constructor {}",
                        db.string_pool.get(field.name.id),
                        db.string_pool.get(ctor.name.id));
                    db::add_error(db, ctx.doc_id, field.name.range, std::move(message));
                }
            }

            auto const has_no_initializer = [&](hir::Field_id id) {
                return not map.contains(ctx.arena.hir.fields[id].field_index);
            };

            auto missing_field_ids = std::views::values(body->fields)
                                   | std::views::filter(has_no_initializer)
                                   | std::ranges::to<std::vector>();

            if (missing_field_ids.empty()) {
                auto arguments = std::ranges::to<std::vector>(
                    std::views::repeat(hir::Expression_id(0), body->fields.size()));

                for (auto& [field_index, argument] : map) {
                    arguments.at(field_index) = ctx.arena.hir.expressions.push(std::move(argument));
                }

                return {
                    .variant  = hir::expr::Initializer {
                        .constructor = ctor_id.value(),
                        .arguments   = std::move(arguments),
                    },
                    .type_id  = ctor.owner_type_id,
                    .category = hir::Expression_category::Value,
                    .range    = this_range,
                };
            }

            // Sort the missing fields in their definition order.
            std::ranges::sort(missing_field_ids, std::less {}, [&](hir::Field_id id) {
                return ctx.arena.hir.fields[id].name.range.start;
            });

            std::string message = "Missing initializers for struct fields";
            for (hir::Field_id field_id : missing_field_ids) {
                hir::Field_info& field = ctx.arena.hir.fields[field_id];
                std::format_to(
                    std::back_inserter(message),
                    "\n- {}: {}",
                    db.string_pool.get(field.name.id),
                    hir::to_string(ctx.arena.hir, db.string_pool, field.type));
            }

            std::optional<lsp::Position> final_field_end;
            if (not init.fields.empty()) {
                final_field_end
                    = ctx.arena.ast.expressions[init.fields.back().expression].range.stop;
            }

            db::add_action(
                db,
                ctx.doc_id,
                this_range,
                db::Action_fill_in_struct_init {
                    .field_ids       = std::move(missing_field_ids),
                    .final_field_end = final_field_end,
                });

            return error(this_range, std::move(message));
        }

        auto operator()(ast::expr::Struct_field const& field) -> hir::Expression
        {
            auto  base    = recurse(ctx.arena.ast.expressions[field.base]);
            auto& variant = ctx.arena.hir.types[base.type_id];

            flatten_type(ctx, state, variant);

            if (auto const* type = std::get_if<hir::type::Structure>(&variant)) {
                auto const& structure = resolve_structure(db, ctx, type->id);
                auto const& ctor      = ctx.arena.hir.constructors[structure.constructor_id];

                if (auto const* constructor = std::get_if<hir::Struct_constructor>(&ctor.body)) {
                    auto const it = constructor->fields.find(field.name.id);
                    if (it != constructor->fields.end()) {
                        db::add_reference(
                            db,
                            ctx.doc_id,
                            lsp::read(field.name.range),
                            ctx.arena.hir.fields[it->second].symbol_id);
                        return hir::Expression {
                            .variant  = hir::expr::Struct_field {
                                .base = ctx.arena.hir.expressions.push(std::move(base)),
                                .id   = it->second,
                            },
                            .type_id  = ctx.arena.hir.fields[it->second].type.id,
                            .category = hir::Expression_category::Place,
                            .range    = this_range,
                        };
                    }
                }
            }
            return field_error(base, field.name.range, db.string_pool.get(field.name.id));
        }

        auto operator()(ast::expr::Tuple_field const& field) -> hir::Expression
        {
            auto  base    = recurse(ctx.arena.ast.expressions[field.base]);
            auto& variant = ctx.arena.hir.types[base.type_id];

            flatten_type(ctx, state, variant);

            auto field_index = [&](std::span<hir::Type const> types) {
                if (field.index < types.size()) {
                    return hir::Expression {
                        .variant = hir::expr::Tuple_field {
                            .base  = ctx.arena.hir.expressions.push(std::move(base)),
                            .index = field.index,
                        },
                        .type_id  = types[field.index].id,
                        .category = hir::Expression_category::Place,
                        .range    = this_range,
                    };
                }
                return field_error(base, field.index_range, std::to_string(field.index));
            };

            if (auto const* tuple = std::get_if<hir::type::Tuple>(&variant)) {
                return field_index(tuple->types);
            }
            else if (auto const* type = std::get_if<hir::type::Structure>(&variant)) {
                auto const& structure   = resolve_structure(db, ctx, type->id);
                auto const& constructor = ctx.arena.hir.constructors[structure.constructor_id];
                if (auto const* body = std::get_if<hir::Tuple_constructor>(&constructor.body)) {
                    return field_index(body->types);
                }
            }
            return field_error(base, field.index_range, std::to_string(field.index));
        }

        auto operator()(ast::expr::Array_index const&) -> hir::Expression
        {
            return error(this_range, "Array index resolution has not been implemented");
        }

        auto operator()(ast::expr::Infix_call const& call) -> hir::Expression
        {
            (void)recurse(ctx.arena.ast.expressions[call.left]);
            (void)recurse(ctx.arena.ast.expressions[call.right]);
            return error(this_range, "Infix call resolution has not been implemented");
        }

        auto operator()(ast::expr::Method_call const& call) -> hir::Expression
        {
            (void)recurse(ctx.arena.ast.expressions[call.expression]);
            return error(this_range, "Method call resolution has not been implemented");
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
            auto result_type = fresh_general_type_variable(ctx, state, this_range);

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
                    .arms = std::ranges::to<std::vector>(std::views::transform(match.arms, resolve_arm)),
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
            auto const local_id = ctx.arena.hir.local_types.push(hir::Local_type {
                .name    = alias.name,
                .type_id = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[alias.type]).id,
            });
            bind_symbol(db, ctx, env_id, alias.name, local_id);
            return unit_expression(ctx.constants, this_range);
        }

        auto operator()(ast::expr::Return const& ret) -> hir::Expression
        {
            auto result = recurse(ctx.arena.ast.expressions[ret.expression]);
            return {
                .variant  = hir::expr::Return { ctx.arena.hir.expressions.push(std::move(result)) },
                .type_id  = ctx.constants.type_unit,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Sizeof const& sizeof_) -> hir::Expression
        {
            auto type = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[sizeof_.type]);
            return {
                .variant  = hir::expr::Sizeof { type },
                .type_id  = fresh_integral_type_variable(ctx, state, this_range).id,
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
            auto ref_type = fresh_general_type_variable(ctx, state, this_range);
            auto ref_mut  = fresh_mutability_variable(ctx, state, this_range);
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
            auto type = fresh_general_type_variable(ctx, state, this_range);
            db::add_type_hint(db, ctx.doc_id, this_range.stop, type.id);
            return {
                .variant  = db::Error {},
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
    Block_state&           state,
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
