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
            return error_expression(ctx, this_range);
        }

        auto unknown_type_error(lsp::Range range) -> hir::Expression
        {
            return error(range, "This type is unknown at this point, annotations are required");
        }

        auto field_error(hir::Expression const& base, lsp::Range range, std::string_view field)
            -> hir::Expression
        {
            if (std::holds_alternative<db::Error>(ctx.arena.hir.types[base.type_id])) {
                return error_expression(ctx, this_range);
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
            return hir::Expression {
                .variant  = integer,
                .type_id  = fresh_integral_type_variable(ctx, state, this_range).id,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Floating const& floating) -> hir::Expression
        {
            return hir::Expression {
                .variant  = floating,
                .type_id  = ctx.constants.type_floating,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Boolean const& boolean) -> hir::Expression
        {
            return hir::Expression {
                .variant  = boolean,
                .type_id  = ctx.constants.type_boolean,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::String const& string) -> hir::Expression
        {
            return hir::Expression {
                .variant  = string,
                .type_id  = ctx.constants.type_string,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::Path const& path) -> hir::Expression
        {
            db::Symbol& symbol = ctx.arena.symbols[resolve_path(db, ctx, state, env_id, path)];
            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return error_expression(ctx, this_range);
            }

            if (auto const* local_id = std::get_if<hir::Local_variable_id>(&symbol.variant)) {
                return hir::Expression {
                    .variant  = hir::expr::Variable_reference { .id = *local_id },
                    .type_id  = ctx.arena.hir.local_variables[*local_id].type_id,
                    .mut_id   = ctx.arena.hir.local_variables[*local_id].mut_id,
                    .category = hir::Expression_category::Place,
                    .range    = this_range,
                };
            }

            if (auto const* fun_id = std::get_if<hir::Function_id>(&symbol.variant)) {
                return hir::Expression {
                    .variant  = hir::expr::Function_reference { .id = *fun_id },
                    .type_id  = resolve_function_signature(db, ctx, *fun_id).function_type.id,
                    .mut_id   = ctx.constants.mut_no,
                    .category = hir::Expression_category::Value,
                    .range    = this_range,
                };
            }

            if (auto const ctor_id = get_constructor_id(db, ctx, symbol)) {
                auto const& ctor = ctx.arena.hir.constructors[ctor_id.value()];

                auto const visitor = utl::Overload {
                    [&](hir::Unit_constructor const&) -> hir::Expression {
                        return hir::Expression {
                            .variant  = hir::expr::Initializer {
                                .constructor = ctor_id.value(),
                                .arguments   = {},
                            },
                            .type_id  = ctor.owner_type_id,
                            .mut_id   = ctx.constants.mut_no,
                            .category = hir::Expression_category::Value,
                            .range    = this_range,
                        };
                    },
                    [&](hir::Tuple_constructor const& tuple) -> hir::Expression {
                        return hir::Expression {
                            .variant  = hir::expr::Constructor_reference { .id = ctor_id.value() },
                            .type_id  = tuple.function_type_id,
                            .mut_id   = ctx.constants.mut_no,
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

            std::vector<hir::Expression_id> elements;
            elements.reserve(array.elements.size());

            for (ast::Expression const& element : array.elements) {
                auto expression = recurse(element);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    element.range,
                    ctx.arena.hir.types[expression.type_id],
                    ctx.arena.hir.types[element_type.id]);
                elements.push_back(ctx.arena.hir.expressions.push(std::move(expression)));
            }

            auto const length = ctx.arena.hir.expressions.push(hir::Expression {
                .variant  = db::Integer { ssize(elements) },
                .type_id  = ctx.constants.type_u64,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            });

            auto const type_id = ctx.arena.hir.types.push(hir::type::Array {
                .element_type = element_type,
                .length       = length,
            });

            return hir::Expression {
                .variant  = hir::expr::Array { std::move(elements) },
                .type_id  = type_id,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Tuple const& tuple) -> hir::Expression
        {
            if (tuple.fields.empty()) {
                return unit_expression(ctx, this_range);
            }

            std::vector<hir::Type>          types;
            std::vector<hir::Expression_id> fields;

            types.reserve(tuple.fields.size());
            fields.reserve(tuple.fields.size());

            for (ast::Expression const& field : tuple.fields) {
                auto expression = recurse(field);
                types.push_back(hir::expression_type(expression));
                fields.push_back(ctx.arena.hir.expressions.push(std::move(expression)));
            }

            return hir::Expression {
                .variant  = hir::expr::Tuple { std::move(fields) },
                .type_id  = ctx.arena.hir.types.push(hir::type::Tuple { std::move(types) }),
                .mut_id   = ctx.constants.mut_no,
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

            return hir::Expression {
                .variant  = hir::expr::Loop { ctx.arena.hir.expressions.push(std::move(body)) },
                .type_id  = type_id.value_or(ctx.constants.type_unit),
                .mut_id   = ctx.constants.mut_no,
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

            return hir::Expression {
                .variant  = hir::expr::Break { ctx.arena.hir.expressions.push(std::move(result)) },
                .type_id  = ctx.constants.type_unit,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Continue const&) -> hir::Expression
        {
            if (not state.loop_info.has_value()) {
                return error(this_range, "'continue' can not be used outside of a loop");
            }
            return hir::Expression {
                .variant  = hir::expr::Continue {},
                .type_id  = ctx.constants.type_unit,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Block const& block) -> hir::Expression
        {
            auto const block_env_id = new_scope(ctx, env_id);

            auto const resolve_effect = [&](ast::Expression const& expression) {
                auto effect = resolve_expression(db, ctx, state, block_env_id, expression);
                require_subtype_relationship(
                    db,
                    ctx,
                    state,
                    effect.range,
                    ctx.arena.hir.types[effect.type_id],
                    hir::type::Tuple {});
                return ctx.arena.hir.expressions.push(std::move(effect));
            };

            auto side_effects = std::ranges::to<std::vector>(
                std::views::transform(block.effects, resolve_effect));

            auto result = resolve_expression(
                db, ctx, state, block_env_id, ctx.arena.ast.expressions[block.result]);

            auto result_type  = result.type_id;
            auto result_range = result.range;

            report_unused(db, ctx, block_env_id);

            return hir::Expression {
                .variant = hir::expr::Block {
                    .effects = std::move(side_effects),
                    .result  = ctx.arena.hir.expressions.push(std::move(result)),
                },
                .type_id  = result_type,
                .mut_id   = ctx.constants.mut_no,
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

                for (std::size_t index = 0; index != call.arguments.size(); ++index) {
                    auto const& parameter = signature.parameters.at(index);
                    auto const& argument  = ctx.arena.ast.expressions[call.arguments.at(index)];

                    db::add_param_hint(db, ctx.doc_id, argument.range.start, parameter.pattern_id);
                    db::add_signature_help(db, ctx.doc_id, argument.range, ref->id, index);

                    auto expression = recurse(argument);
                    require_subtype_relationship(
                        db,
                        ctx,
                        state,
                        argument.range,
                        ctx.arena.hir.types[expression.type_id],
                        ctx.arena.hir.types[parameter.type.id]);
                    arguments.push_back(ctx.arena.hir.expressions.push(std::move(expression)));
                }

                return hir::Expression {
                    .variant  = hir::expr::Function_call {
                        .invocable = ctx.arena.hir.expressions.push(std::move(invocable)),
                        .arguments = std::move(arguments),
                    },
                    .type_id  = signature.return_type.id,
                    .mut_id   = ctx.constants.mut_no,
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

            return hir::Expression {
                .variant  = hir::expr::Function_call {
                    .invocable = ctx.arena.hir.expressions.push(std::move(invocable)),
                    .arguments = std::move(arguments),
                },
                .type_id  = result_type.id,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Struct_init const& init) -> hir::Expression
        {
            db::Symbol& symbol = ctx.arena.symbols[resolve_path(db, ctx, state, env_id, init.path)];
            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return error_expression(ctx, this_range);
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
                db::Field_completion completion { .type_id = ctor.owner_type_id };
                db::add_completion(db, ctx.doc_id, field.name, completion);

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

                return hir::Expression {
                    .variant  = hir::expr::Initializer {
                        .constructor = ctor_id.value(),
                        .arguments   = std::move(arguments),
                    },
                    .type_id  = ctor.owner_type_id,
                    .mut_id   = ctx.constants.mut_no,
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

            db::Field_completion completion { .type_id = base.type_id };
            db::add_completion(db, ctx.doc_id, field.name, completion);

            if (auto const* type = std::get_if<hir::type::Structure>(&variant)) {
                auto const& structure   = resolve_structure(db, ctx, type->id);
                auto const& constructor = ctx.arena.hir.constructors[structure.constructor_id];

                if (auto const* body = std::get_if<hir::Struct_constructor>(&constructor.body)) {
                    auto const it = body->fields.find(field.name.id);
                    if (it != body->fields.end()) {
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
                            .mut_id   = base.mut_id,
                            .category = base.category,
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
                        .mut_id   = base.mut_id,
                        .category = base.category,
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

            return hir::Expression {
                .variant  = std::move(match),
                .type_id  = result_type,
                .mut_id   = ctx.constants.mut_no,
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

            return hir::Expression {
                .variant = hir::expr::Match {
                    .arms = std::ranges::to<std::vector>(std::views::transform(match.arms, resolve_arm)),
                    .scrutinee = ctx.arena.hir.expressions.push(std::move(scrutinee)),
                },
                .type_id  = result_type.id,
                .mut_id   = ctx.constants.mut_no,
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

            return hir::Expression {
                .variant  = hir::expr::Let {
                    .pattern     = ctx.arena.hir.patterns.push(std::move(pattern)),
                    .type        = initializer.type_id,
                    .initializer = ctx.arena.hir.expressions.push(std::move(initializer)),
                },
                .type_id  = ctx.constants.type_unit,
                .mut_id   = ctx.constants.mut_no,
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
            return unit_expression(ctx, this_range);
        }

        auto operator()(ast::expr::Return const& ret) -> hir::Expression
        {
            auto result = recurse(ctx.arena.ast.expressions[ret.expression]);
            return hir::Expression {
                .variant  = hir::expr::Return { ctx.arena.hir.expressions.push(std::move(result)) },
                .type_id  = ctx.constants.type_unit,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Sizeof const& sizeof_) -> hir::Expression
        {
            return hir::Expression {
                .variant  = hir::expr::Sizeof {
                    resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[sizeof_.type]),
                },
                .type_id  = fresh_integral_type_variable(ctx, state, this_range).id,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Addressof const& addressof) -> hir::Expression
        {
            auto place      = recurse(ctx.arena.ast.expressions[addressof.expression]);
            auto place_type = hir::expression_type(place);
            auto mutability = resolve_mutability(db, ctx, env_id, addressof.mutability);

            if (place.category != hir::Expression_category::Place) {
                return error(place.range, "This is a value expression and has no memory address");
            }

            auto success = [&](hir::Mutability_id mut_id) {
                return hir::Expression {
                    .variant  = hir::expr::Addressof {
                        .mutability = { .id = mut_id, .range = mutability.range },
                        .expression = ctx.arena.hir.expressions.push(std::move(place)),
                    },
                    .type_id  = ctx.arena.hir.types.push(hir::type::Reference {
                        .referenced_type = place_type,
                        .mutability = { .id = mut_id, .range = mutability.range },
                    }),
                    .mut_id   = ctx.constants.mut_no,
                    .category = hir::Expression_category::Value,
                    .range    = this_range,
                };
            };

            auto solve = [&](hir::Mutability_variable_id var_id, hir::Mutability_variant solution) {
                set_mut_solution(db, ctx, state, var_id, std::move(solution));
                return success(mutability.id);
            };

            auto const visitor = utl::Overload {
                [&](db::Error, auto const&) {
                    // Place mutability is erroneous, use requested mutability.
                    return success(mutability.id);
                },
                [&](auto const&, db::Error) {
                    // Requested mutability is erroneous, use place mutability.
                    return success(place.mut_id);
                },
                [&](db::Error, db::Error) { return success(mutability.id); },

                [&](db::Mutability place_mut, db::Mutability requested_mut) {
                    if (requested_mut == db::Mutability::Immut) {
                        return success(mutability.id);
                    }
                    if (place_mut == db::Mutability::Immut) {
                        return error(
                            mutability.range,
                            "Can not acquire a mutable reference to an immutable object");
                    }
                    return success(place.mut_id);
                },
                [&](db::Mutability place_mut, hir::mut::Parameterized const&) {
                    if (place_mut == db::Mutability::Mut) {
                        return success(mutability.id);
                    }
                    return error(
                        mutability.range,
                        "Can not acquire a reference of generic "
                        "mutability to an immutable object");
                },

                [&](hir::mut::Parameterized const&, db::Mutability requested_mut) {
                    if (requested_mut == db::Mutability::Immut) {
                        return success(mutability.id);
                    }
                    return error(
                        mutability.range,
                        "Can not acquire a mutable reference "
                        "to an object of generic mutability");
                },
                [&](hir::mut::Parameterized place_mut, hir::mut::Parameterized requested_mut) {
                    if (place_mut.tag.value == requested_mut.tag.value) {
                        return success(mutability.id);
                    }
                    return error(
                        mutability.range,
                        "Can not acquire a reference of generic mutability "
                        "to an object of a different generic mutability");
                },

                [&](db::Mutability solution, hir::mut::Variable variable) {
                    return solve(variable.id, solution);
                },
                [&](hir::mut::Parameterized solution, hir::mut::Variable variable) {
                    return solve(variable.id, solution);
                },
                [&](hir::mut::Variable variable, db::Mutability solution) {
                    if (solution == db::Mutability::Immut) {
                        return success(mutability.id);
                    }
                    return solve(variable.id, solution);
                },
                [&](hir::mut::Variable variable, hir::mut::Parameterized solution) {
                    return solve(variable.id, solution);
                },
                [&](hir::mut::Variable variable, hir::mut::Variable solution) {
                    return solve(variable.id, solution);
                },
            };

            return std::visit(
                visitor,
                ctx.arena.hir.mutabilities[place.mut_id],
                ctx.arena.hir.mutabilities[mutability.id]);
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

            return hir::Expression {
                .variant  = hir::expr::Deref {
                    ctx.arena.hir.expressions.push(std::move(ref_expr)),
                },
                .type_id  = ref_type.id,
                .mut_id   = ref_mut.id,
                .category = hir::Expression_category::Place,
                .range    = this_range,
            };
        }

        auto operator()(ast::expr::Defer const& defer) -> hir::Expression
        {
            return hir::Expression {
                .variant  = hir::expr::Defer {
                    ctx.arena.hir.expressions.push(recurse(ctx.arena.ast.expressions[defer.expression])),
                },
                .type_id  = ctx.constants.type_unit,
                .mut_id   = ctx.constants.mut_no,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Expression
        {
            auto type = fresh_general_type_variable(ctx, state, this_range);
            db::add_type_hint(db, ctx.doc_id, this_range.stop, type.id);
            return hir::Expression {
                .variant  = db::Error {},
                .type_id  = type.id,
                .mut_id   = ctx.constants.mut_yes,
                .category = hir::Expression_category::Value,
                .range    = this_range,
            };
        }

        auto operator()(db::Error const&) const -> hir::Expression
        {
            return error_expression(ctx, this_range);
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
