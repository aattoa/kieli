#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    struct Expression_resolution_visitor {
        Context&               context;
        Inference_state&       state;
        Scope&                 scope;
        hir::Environment_id    environment;
        ast::Expression const& this_expression;

        auto error(
            kieli::Range const         range,
            std::string                message,
            std::optional<std::string> help_note = std::nullopt)
        {
            kieli::emit_diagnostic(
                cppdiag::Severity::error,
                context.compile_info,
                context.info.environments[environment].source,
                range,
                std::move(message),
                std::move(help_note));
            return error_expression(context.constants, this_expression.range);
        }

        auto recurse()
        {
            return [&](ast::Expression const& expression) -> hir::Expression {
                return resolve_expression(context, state, scope, environment, expression);
            };
        }

        auto recurse(ast::Expression const& expression) -> hir::Expression
        {
            return recurse()(expression);
        }

        auto operator()(kieli::Integer const& integer) -> hir::Expression
        {
            return {
                integer,
                state.fresh_integral_type_variable(context.hir, this_expression.range).id,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Expression
        {
            return {
                floating,
                context.constants.floating_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::Character const& character) -> hir::Expression
        {
            return {
                character,
                context.constants.character_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Expression
        {
            return {
                boolean,
                context.constants.boolean_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(kieli::String const& string) -> hir::Expression
        {
            return {
                string,
                context.constants.string_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Array_literal const& array) -> hir::Expression
        {
            hir::Type const element_type
                = state.fresh_general_type_variable(context.hir, this_expression.range);

            std::vector<hir::Expression> elements;
            for (ast::Expression const& element : array.elements) {
                elements.push_back(recurse(element));
                require_subtype_relationship(
                    context,
                    state,
                    context.hir.types[elements.back().type],
                    context.hir.types[element_type.id]);
            }

            return {
                hir::expression::Array_literal { std::move(elements) },
                context.hir.types.push(hir::type::Array {
                    .element_type = element_type,
                    .length       = context.hir.expressions.push(hir::Expression {
                        kieli::Integer { elements.size() },
                        context.constants.u64_type,
                        hir::Expression_kind::value,
                        this_expression.range,
                    }),
                }),
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Self const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Variable const& variable) -> hir::Expression
        {
            if (variable.path.is_simple_name()) {
                if (Variable_bind const* const bind
                    = scope.find_variable(variable.path.head.identifier)) {
                    return hir::Expression {
                        hir::expression::Variable_reference {
                            .name = bind->name,
                            .tag  = bind->tag,
                        },
                        bind->type,
                        hir::Expression_kind::place,
                        this_expression.range,
                    };
                }
            }
            if (auto const lookup_result
                = lookup_lower(context, state, scope, environment, variable.path)) {
                return std::visit(
                    utl::Overload {
                        [&](hir::Function_id const function) {
                            auto& info      = context.info.functions[function];
                            auto& signature = resolve_function_signature(context, info);

                            return hir::Expression {
                                hir::expression::Function_reference { info.name, function },
                                signature.function_type.id,
                                hir::Expression_kind::value,
                                this_expression.range,
                            };
                        },
                        [&](hir::Module_id const module) {
                            return error(
                                this_expression.range,
                                std::format(
                                    "Expected an expression, but found a reference to module '{}'",
                                    context.info.modules[module].name));
                        },
                    },
                    lookup_result.value().variant);
            }
            return error(this_expression.range, "Use of an undeclared identifier");
        }

        auto operator()(ast::expression::Tuple const& tuple) -> hir::Expression
        {
            auto fields = std::views::transform(tuple.fields, recurse()) //
                        | std::ranges::to<std::vector>();
            auto types = std::views::transform(fields, hir::expression_type)
                       | std::ranges::to<std::vector>();
            return {
                hir::expression::Tuple { .fields = std::move(fields) },
                context.hir.types.push(hir::type::Tuple { std::move(types) }),
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Loop const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Break const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Continue const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Block const& block) -> hir::Expression
        {
            Scope block_scope = scope.child();

            auto const resolve_effect = [&](ast::Expression const& expression) {
                hir::Expression effect
                    = resolve_expression(context, state, block_scope, environment, expression);
                require_subtype_relationship(
                    context,
                    state,
                    context.hir.types[effect.type],
                    context.hir.types[context.constants.unit_type]);
                return effect;
            };

            auto side_effects = std::views::transform(block.side_effects, resolve_effect)
                              | std::ranges::to<std::vector>();

            hir::Expression result
                = resolve_expression(context, state, scope, environment, *block.result);

            auto const result_type = result.type;

            return {
                hir::expression::Block {
                    .side_effects = std::move(side_effects),
                    .result       = context.hir.expressions.push(std::move(result)),
                },
                result_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Invocation const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Unit_initializer const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Tuple_initializer const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Struct_initializer const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Binary_operator_invocation const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Struct_field_access const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Tuple_field_access const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Array_index_access const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Method_invocation const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Conditional const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Match const& match) -> hir::Expression
        {
            hir::Expression matched_expression = recurse(*match.expression);

            hir::Type const result_type
                = state.fresh_general_type_variable(context.hir, this_expression.range);

            auto const resolve_case = [&](ast::expression::Match::Case const& match_case) {
                hir::Pattern pattern
                    = resolve_pattern(context, state, scope, environment, *match_case.pattern);
                require_subtype_relationship(
                    context,
                    state,
                    context.hir.types[matched_expression.type],
                    context.hir.types[pattern.type]);
                hir::Expression expression = recurse(*match_case.expression);
                require_subtype_relationship(
                    context,
                    state,
                    context.hir.types[expression.type],
                    context.hir.types[result_type.id]);
                return hir::expression::Match::Case {
                    .pattern    = context.hir.patterns.push(std::move(pattern)),
                    .expression = context.hir.expressions.push(std::move(expression)),
                };
            };

            return {
                hir::expression::Match {
                    .cases = std::views::transform(match.cases, resolve_case)
                           | std::ranges::to<std::vector>(),
                    .expression = context.hir.expressions.push(std::move(matched_expression)),
                },
                result_type.id,
                hir::Expression_kind::value, // TODO
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Template_application const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Type_cast const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Type_ascription const& ascription) -> hir::Expression
        {
            hir::Expression expression = recurse(*ascription.expression);
            hir::Type const ascribed
                = resolve_type(context, state, scope, environment, *ascription.ascribed_type);
            require_subtype_relationship(
                context, state, context.hir.types[expression.type], context.hir.types[ascribed.id]);
            return expression;
        }

        auto operator()(ast::expression::Let_binding const& let) -> hir::Expression
        {
            hir::Pattern pattern
                = resolve_pattern(context, state, scope, environment, *let.pattern);

            std::optional<hir::Type> type;
            if (let.type.has_value()) {
                type = resolve_type(context, state, scope, environment, *let.type.value());
                require_subtype_relationship(
                    context,
                    state,
                    context.hir.types[pattern.type],
                    context.hir.types[type.value().id]);
            }
            type = type.value_or(hir::pattern_type(pattern));

            hir::Expression initializer = recurse(*let.initializer);
            require_subtype_relationship(
                context,
                state,
                context.hir.types[initializer.type],
                context.hir.types[type.value().id]);

            return {
                hir::expression::Let_binding {
                    .pattern     = context.hir.patterns.push(std::move(pattern)),
                    .type        = type.value(),
                    .initializer = context.hir.expressions.push(std::move(initializer)),
                },
                context.constants.unit_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Local_type_alias const& alias) -> hir::Expression
        {
            scope.bind_type(
                alias.name.identifier,
                Type_bind {
                    .name = alias.name,
                    .type = resolve_type(context, state, scope, environment, *alias.type).id,
                });
            return unit_expression(context.constants, this_expression.range);
        }

        auto operator()(ast::expression::Ret const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Sizeof const& sizeof_) -> hir::Expression
        {
            return {
                hir::expression::Sizeof {
                    resolve_type(context, state, scope, environment, *sizeof_.inspected_type),
                },
                state.fresh_integral_type_variable(context.hir, this_expression.range).id,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Addressof const& addressof) -> hir::Expression
        {
            hir::Expression    place_expression = recurse(*addressof.place_expression);
            hir::Type_id const referenced_type  = place_expression.type;
            hir::Mutability mutability = resolve_mutability(context, scope, addressof.mutability);
            if (place_expression.kind != hir::Expression_kind::place) {
                return error(
                    place_expression.range,
                    "This expression does not identify a place in memory, "
                    "so its address can not be taken");
            }
            return {
                hir::expression::Addressof {
                    .mutability       = mutability,
                    .place_expression = context.hir.expressions.push(std::move(place_expression)),
                },
                context.hir.types.push(hir::type::Reference {
                    .referenced_type { referenced_type, this_expression.range },
                    .mutability = mutability,
                }),
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Dereference const& dereference) -> hir::Expression
        {
            hir::Type const referenced_type
                = state.fresh_general_type_variable(context.hir, this_expression.range);
            hir::Mutability const reference_mutability
                = state.fresh_mutability_variable(context.hir, this_expression.range);
            hir::Type const reference_type {
                context.hir.types.push(hir::type::Reference {
                    .referenced_type = referenced_type,
                    .mutability      = reference_mutability,
                }),
                this_expression.range,
            };

            hir::Expression reference_expression = recurse(*dereference.reference_expression);
            require_subtype_relationship(
                context,
                state,
                context.hir.types[reference_expression.type],
                context.hir.types[reference_type.id]);

            return {
                hir::expression::Dereference {
                    context.hir.expressions.push(std::move(reference_expression)),
                },
                referenced_type.id,
                hir::Expression_kind::place,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Defer const& defer) -> hir::Expression
        {
            return {
                hir::expression::Defer {
                    context.hir.expressions.push(recurse(*defer.expression)),
                },
                context.constants.unit_type,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }

        auto operator()(ast::expression::Unsafe const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Move const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Meta const&) -> hir::Expression
        {
            cpputil::todo();
        }

        auto operator()(ast::expression::Hole const&) -> hir::Expression
        {
            return {
                hir::expression::Hole {},
                state.fresh_general_type_variable(context.hir, this_expression.range).id,
                hir::Expression_kind::value,
                this_expression.range,
            };
        }
    };
} // namespace

auto libresolve::resolve_expression(
    Context&               context,
    Inference_state&       state,
    Scope&                 scope,
    hir::Environment_id    environment,
    ast::Expression const& expression) -> hir::Expression
{
    return std::visit(
        Expression_resolution_visitor { context, state, scope, environment, expression },
        expression.variant);
}
