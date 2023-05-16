#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;


namespace {

    auto require_addressability(
        Context               & context,
        mir::Expression  const& expression,
        std::string_view const  explanation) -> void
    {
        if (!expression.is_addressable) {
            context.error(expression.source_view, {
                .message   = "This expression is not addressable",
                .help_note = explanation
            });
        }
    }

    auto take_reference(
        Context&               context,
        mir::Expression&&      referenced_expression,
        mir::Mutability  const requested_mutability,
        utl::Source_view const source_view) -> mir::Expression
    {
        require_addressability(context, referenced_expression, "A temporary object can not be referenced");

        mir::Type       const referenced_type   = referenced_expression.type;
        mir::Mutability const actual_mutability = referenced_expression.mutability;

        auto const mutability_error = [&](std::string_view const message, utl::Pair<std::string_view> const notes) {
            context.diagnostics().emit_error({
                .sections = utl::to_vector<utl::diagnostics::Text_section>({
                    {
                        .source_view = actual_mutability.source_view(),
                        .note        = notes.first,
                        .note_color  = utl::diagnostics::warning_color
                    },
                    {
                        .source_view = requested_mutability.source_view(),
                        .note        = notes.second,
                        .note_color  = utl::diagnostics::error_color
                    }
                }),
                .message = message
            });
        };

        auto const solve_mutability_quality_constraint = [&] {
            context.solve(constraint::Mutability_equality{
                .constrainer_mutability = actual_mutability,
                .constrained_mutability = requested_mutability,
                .constrainer_note {
                    requested_mutability.source_view(),
                    "Requested mutability ({1})"
                },
                .constrained_note {
                    actual_mutability.source_view(),
                    "Actual mutability ({0})"
                }
            });
        };

        // Just solving the mutability equality constraint would be sufficient,
        // but this improves the error messages for some of the common cases.

        std::visit(utl::Overload {
            [&](mir::Mutability::Concrete const actual, mir::Mutability::Concrete const requested) {
                if (!actual.is_mutable.get() && requested.is_mutable.get()) {
                    mutability_error(
                        "Can not acquire a mutable reference to an immutable object",
                        {
                            "Immutable due to this",
                            "Attempted to acquire mutable reference here"
                        });
                }
            },
            [&](mir::Mutability::Parameterized const actual, mir::Mutability::Parameterized const requested) {
                if (actual.tag != requested.tag) {
                    mutability_error(
                        "Mutabilities parameterized by different template parameters",
                        {
                            "Mutability parameterized by one template parameter here",
                            "Mutability parameterized by a different template parameter here"
                        });
                }
            },
            [&](mir::Mutability::Parameterized, mir::Mutability::Concrete const requested) {
                if (requested.is_mutable.get()) {
                    mutability_error(
                        "Can not acquire mutable reference to object of parameterized mutability",
                        {
                            "Parameterized mutability here",
                            "Attempted to acquire mutable reference here"
                        });
                }
            },
            [&](mir::Mutability::Concrete const actual, mir::Mutability::Parameterized) {
                if (!actual.is_mutable.get()) {
                    mutability_error(
                        "Can not acquire reference of parameterized mutability to immutable object",
                        {
                            "Immutable due to this",
                            "Attempted to acquire a reference of parameterized mutability here"
                        });
                }
            },
            [&](mir::Mutability::Variable const actual, mir::Mutability::Variable const requested) {
                if (actual.state.is_not(requested.state))
                    solve_mutability_quality_constraint();
            },
            [&](mir::Mutability::Concrete const actual, auto) {
                if (!actual.is_mutable.get())
                    solve_mutability_quality_constraint();
            },
            [&](auto, auto) {
                solve_mutability_quality_constraint();
            }
        }, *actual_mutability.flattened_value(), *requested_mutability.flattened_value());

        return {
            .value = mir::expression::Reference {
                .mutability            = requested_mutability,
                .referenced_expression = context.wrap(std::move(referenced_expression)),
            },
            .type = mir::Type {
                context.wrap_type(mir::type::Reference {
                    .mutability      = requested_mutability,
                    .referenced_type = referenced_type,
                }),
                source_view,
            },
            .source_view = source_view,
            .mutability  = context.immut_constant(source_view),
        };
    }


    struct Expression_resolution_visitor {
        Context        & context;
        Scope          & scope;
        Namespace      & space;
        hir::Expression& this_expression;

        auto recurse(hir::Expression& expression, Scope* const new_scope = nullptr) {
            return context.resolve_expression(expression, new_scope != nullptr ? *new_scope : scope, space);
        }

        [[nodiscard]]
        auto recurse() noexcept {
            return [this](hir::Expression& expression) -> mir::Expression {
                return recurse(expression);
            };
        }


        auto resolve_direct_invocation(
            mir::expression::Function_reference&& function,
            std::vector<mir::Expression>       && arguments) -> mir::Expression
        {
            mir::Function::Signature& signature = context.resolve_function_signature(*function.info);
            utl::always_assert(!signature.is_template());

            utl::Usize const argument_count  = arguments.size();
            utl::Usize const parameter_count = signature.parameters.size();

            if (argument_count != parameter_count) {
                context.error(this_expression.source_view, {
                    .message   = "The function has {} parameters, but {} arguments were supplied"_format(parameter_count, argument_count),
                    .help_note = "The function is of type {}"_format(signature.function_type),
                });
            }

            for (auto [parameter, argument] : ranges::views::zip(signature.parameters, arguments)) {
                context.solve(constraint::Type_equality {
                    .constrainer_type = parameter.type,
                    .constrained_type = argument.type,
                    .constrainer_note = constraint::Explanation {
                        parameter.type.source_view(),
                        "The parameter is specified to be of type {0}",
                    },
                    .constrained_note = constraint::Explanation {
                        argument.source_view,
                        "But the argument is of type {1}",
                    },
                });
            }

            return {
                .value = mir::expression::Direct_invocation {
                    .function  = { .info = function.info },
                    .arguments = std::move(arguments)
                },
                .type        = signature.return_type.with(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view)
            };
        }

        auto resolve_indirect_invocation(
            mir::Expression             && invocable,
            std::vector<mir::Expression>&& arguments) -> mir::Expression
        {
            mir::Type const return_type =
                context.fresh_general_unification_type_variable(this_expression.source_view);

            context.solve(constraint::Type_equality {
                .constrainer_type = mir::Type {
                    context.wrap_type(mir::type::Function {
                        .parameter_types = utl::map(&mir::Expression::type, arguments),
                        .return_type     = return_type,
                    }),
                    this_expression.source_view,
                },
                .constrained_type = invocable.type,
                .constrainer_note = constraint::Explanation {
                    this_expression.source_view,
                    "The invocable should be of type {0}",
                },
                .constrained_note {
                    invocable.source_view,
                    "But it is of type {1}",
                }
            });

            return {
                .value = mir::expression::Indirect_invocation {
                    .arguments = std::move(arguments),
                    .invocable = context.wrap(std::move(invocable))
                },
                .type        = return_type,
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view)
            };
        }

        auto resolve_invocation(
            mir::Expression             && invocable,
            std::vector<mir::Expression>&& arguments) -> mir::Expression
        {
            if (auto* const function = std::get_if<mir::expression::Function_reference>(&invocable.value))
                return resolve_direct_invocation(std::move(*function), std::move(arguments));
            else
                return resolve_indirect_invocation(std::move(invocable), std::move(arguments));
        }

        auto resolve_arguments(std::vector<hir::Function_argument>& arguments)
            -> std::vector<mir::Expression>
        {
            auto const resolve_argument = [&](hir::Function_argument& argument) {
                if (argument.name.has_value())
                    context.error(argument.name->source_view, { "Named arguments are not supported yet" });
                else
                    return recurse(argument.expression);
            };
            return utl::map(resolve_argument, arguments);
        }


        auto try_resolve_local_variable_reference(compiler::Identifier const identifier)
            -> tl::optional<mir::Expression>
        {
            if (auto* const binding = scope.find_variable(identifier)) {
                binding->has_been_mentioned = true;
                return mir::Expression {
                    .value          = mir::expression::Local_variable_reference { binding->variable_tag, identifier },
                    .type           = binding->type.with(this_expression.source_view),
                    .source_view    = this_expression.source_view,
                    .mutability     = binding->mutability,
                    .is_addressable = true,
                    .is_pure        = true,
                };
            }
            return tl::nullopt;
        }


        template <class T>
        auto operator()(hir::expression::Literal<T>& literal) -> mir::Expression {
            return {
                .value       = mir::expression::Literal<T> { literal.value },
                .type        = context.literal_type<T>(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
                .is_pure     = true,
            };
        }

        auto operator()(hir::expression::Array_literal& array) -> mir::Expression {
            mir::expression::Array_literal mir_array;
            mir_array.elements.reserve(array.elements.size());

            if (!array.elements.empty()) {
                mir_array.elements.push_back(recurse(array.elements.front()));

                for (utl::Usize i = 1; i != array.elements.size(); ++i) {
                    hir::Expression& previous_element = array.elements[i - 1];
                    hir::Expression& current_element  = array.elements[i];

                    mir_array.elements.push_back(recurse(current_element));

                    context.solve(constraint::Type_equality {
                        .constrainer_type = mir_array.elements.front().type,
                        .constrained_type = mir_array.elements.back().type,
                        .constrainer_note = constraint::Explanation {
                            array.elements.front().source_view.combine_with(previous_element.source_view),
                            i == 1 ? "The previous element was of type {0}"
                                   : "The previous elements were of type {0}",
                        },
                        .constrained_note {
                            current_element.source_view,
                            "But this element is of type {1}",
                        }
                    });
                }
            }

            mir::Type const element_type = mir_array.elements.empty()
                ? context.fresh_general_unification_type_variable(this_expression.source_view)
                : mir_array.elements.front().type;

            auto const array_length = mir_array.elements.size();
            bool const is_pure = ranges::all_of(mir_array.elements, &mir::Expression::is_pure);

            return {
                .value = std::move(mir_array),
                .type  = mir::Type {
                    context.wrap_type(mir::type::Array {
                        .element_type = element_type,
                        .array_length = context.wrap(mir::Expression {
                            .value       = mir::expression::Literal<kieli::Unsigned_integer> { array_length },
                            .type        = context.size_type(this_expression.source_view),
                            .source_view = this_expression.source_view,
                            .mutability  = context.immut_constant(this_expression.source_view),
                            .is_pure     = true,
                        })
                    }),
                    this_expression.source_view,
                },
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
                .is_pure     = is_pure,
            };
        }

        auto operator()(hir::expression::Move& move) -> mir::Expression {
            mir::Expression lvalue = recurse(*move.lvalue);
            mir::Type const type   = lvalue.type;
            require_addressability(context, lvalue, "Temporaries are moved by default, and may not be explicitly moved");
            return {
                .value       = mir::expression::Move { context.wrap(std::move(lvalue)) },
                .type        = type,
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
            };
        }

        auto operator()(hir::expression::Variable& variable) -> mir::Expression {
            if (variable.name.is_unqualified()) {
                if (auto local_variable = try_resolve_local_variable_reference(variable.name.primary_name.identifier))
                    return std::move(*local_variable);
            }

            auto const handle_function_reference = [&](utl::Wrapper<Function_info> const info, bool const is_application) -> mir::Expression {
                return {
                    .value       = mir::expression::Function_reference { info, is_application },
                    .type        = context.resolve_function_signature(*info).function_type.with(this_expression.source_view),
                    .source_view = this_expression.source_view,
                    .mutability  = context.immut_constant(this_expression.source_view),
                    .is_pure     = true,
                };
            };

            return utl::match(context.find_lower(variable.name, scope, space),
                [&](utl::Wrapper<Function_info> const info) -> mir::Expression {
                    if (context.resolve_function_signature(*info).is_template()) {
                        return handle_function_reference(
                            context.instantiate_function_template_with_synthetic_arguments(info, this_expression.source_view),
                            /*is_application=*/ true);
                    }
                    else {
                        return handle_function_reference(info, /*is_application=*/ false);
                    }
                },
                [this](mir::Enum_constructor const constructor) -> mir::Expression {
                    return {
                        .value       = mir::expression::Enum_constructor_reference { constructor },
                        .type        = constructor.function_type.value_or(constructor.enum_type).with(this_expression.source_view),
                        .source_view = this_expression.source_view,
                        .mutability  = context.immut_constant(this_expression.source_view)
                    };
                },
                [this](utl::Wrapper<Namespace>) -> mir::Expression {
                    context.error(this_expression.source_view, { "Expected an expression, but found a namespace" });
                }
            );
        }

        auto operator()(hir::expression::Tuple& tuple) -> mir::Expression {
            mir::expression::Tuple mir_tuple {
                .fields = utl::map(recurse(), tuple.fields) };
            mir::type::Tuple mir_tuple_type {
                .field_types = utl::map(&mir::Expression::type, mir_tuple.fields) };
            bool const is_pure = ranges::all_of(mir_tuple.fields, &mir::Expression::is_pure);
            return {
                .value = std::move(mir_tuple),
                .type = mir::Type {
                    context.wrap_type(std::move(mir_tuple_type)),
                    this_expression.source_view,
                },
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
                .is_pure     = is_pure,
            };
        }

        auto operator()(hir::expression::Loop& loop) -> mir::Expression {
            auto const old_loop_info  = context.current_loop_info;
            context.current_loop_info = Loop_info { .loop_kind = loop.kind };
            mir::Expression loop_body = recurse(*loop.body);
            Loop_info const loop_info = utl::get(context.current_loop_info);
            context.current_loop_info = old_loop_info;
            return {
                .value = mir::expression::Loop { .body = context.wrap(std::move(loop_body)) },
                .type  = loop_info.break_return_type.has_value()
                    ? loop_info.break_return_type->with(this_expression.source_view)
                    : context.unit_type(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
            };
        }

        auto operator()(hir::expression::Break& break_) -> mir::Expression {
            if (break_.label.has_value())
                utl::todo();
            if (!context.current_loop_info.has_value())
                context.error(this_expression.source_view, { "a break expression can not appear outside of a loop" });

            mir::Expression break_result = recurse(*break_.result);
            Loop_info& loop_info = utl::get(context.current_loop_info);

            if (loop_info.loop_kind.get() == hir::expression::Loop::Kind::plain_loop) {
                if (!loop_info.break_return_type.has_value()) {
                    loop_info.break_return_type = break_result.type;
                }
                else {
                    context.solve(constraint::Type_equality {
                        .constrainer_type = *loop_info.break_return_type,
                        .constrained_type = break_result.type,
                        .constrainer_note = constraint::Explanation {
                            loop_info.break_return_type->source_view(),
                            "Previous break expressions had results of type {0}"
                        },
                        .constrained_note = constraint::Explanation {
                            break_result.type.source_view(),
                            "But this break expressions's result is of type {1}"
                        }
                    });
                }
            }
            else {
                context.solve(constraint::Type_equality {
                    .constrainer_type = context.unit_type(this_expression.source_view),
                    .constrained_type = break_result.type,
                    .constrained_note = constraint::Explanation {
                        break_result.source_view,
                        "This break expression's result type is {1}, but only break "
                        "expressions within plain loops can have results of non-unit types"
                    }
                });
            }

            return {
                .value       = mir::expression::Break { .result = context.wrap(std::move(break_result)) },
                .type        = context.unit_type(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
            };
        }

        auto operator()(hir::expression::Continue&) -> mir::Expression {
            if (!context.current_loop_info.has_value())
                context.error(this_expression.source_view, { "a continue expression can not appear outside of a loop" });
            return {
                .value       = mir::expression::Continue {},
                .type        = context.unit_type(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
            };
        }

        auto operator()(hir::expression::Block& block) -> mir::Expression {
            Scope block_scope = scope.make_child();

            std::vector<mir::Expression> side_effects;
            side_effects.reserve(block.side_effect_expressions.size());

            for (hir::Expression& hir_side_effect : block.side_effect_expressions) {
                mir::Expression side_effect = recurse(hir_side_effect, &block_scope);
                if (side_effect.is_pure) {
                    context.diagnostics().emit_warning(side_effect.source_view, {
                        .message   = "This block side-effect expression is pure, so it does not have any side-effects",
                        .help_note = "Pure side effect-expressions have no effect on program execution, but they are still evaluated. This may lead to performance degradation."
                    });
                }
                context.solve(constraint::Type_equality {
                    .constrainer_type = context.unit_type(this_expression.source_view),
                    .constrained_type = side_effect.type,
                    .constrained_note = constraint::Explanation {
                        side_effect.source_view,
                        "This expression is of type {1}, but side-effect expressions must be of the unit type"
                    }
                });
                side_effects.push_back(std::move(side_effect));
            }

            mir::Expression block_result = recurse(*block.result_expression, &block_scope);
            mir::Type const result_type  = block_result.type;

            block_scope.warn_about_unused_bindings(context);

            bool const is_pure = block_result.is_pure
                && ranges::all_of(side_effects, &mir::Expression::is_pure);

            return {
                .value = mir::expression::Block {
                    .side_effect_expressions = std::move(side_effects),
                    .result_expression       = context.wrap(std::move(block_result))
                },
                .type        = result_type,
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
                .is_pure     = is_pure,
            };
        }

        auto operator()(hir::expression::Local_type_alias& alias) -> mir::Expression {
            scope.bind_type(context, alias.identifier, {
                .type               = context.resolve_type(*alias.aliased_type, scope, space),
                .has_been_mentioned = false,
                .source_view        = this_expression.source_view
            });
            return {
                .value          = mir::expression::Tuple {},
                .type           = context.unit_type(this_expression.source_view),
                .source_view    = this_expression.source_view,
                .mutability     = context.immut_constant(this_expression.source_view),
                .is_addressable = false,
                .is_pure        = true,
            };
        }

        auto operator()(hir::expression::Let_binding& let) -> mir::Expression {
            mir::Expression initializer = recurse(*let.initializer);
            mir::Pattern pattern = context.resolve_pattern(*let.pattern, initializer.type, scope, space);

            mir::Type const type = std::invoke([&] {
                if (!let.type.has_value())
                    return initializer.type;
                mir::Type const explicit_type = context.resolve_type(**let.type, scope, space);
                context.solve(constraint::Type_equality {
                    .constrainer_type = explicit_type,
                    .constrained_type = initializer.type,
                    .constrainer_note = constraint::Explanation {
                        explicit_type.source_view(),
                        "The explicitly specified type is {0}",
                    },
                    .constrained_note = constraint::Explanation {
                        pattern.source_view,
                        "But the pattern is of type {1}",
                    },
                });
                return explicit_type;
            });

            context.solve(constraint::Type_equality {
                .constrainer_type = type,
                .constrained_type = initializer.type,
                .constrainer_note = constraint::Explanation {
                    type.source_view(),
                    let.type.has_value()
                        ? "The explicitly specified type is {0}"
                        : "The pattern is of type {0}",
                },
                .constrained_note = constraint::Explanation {
                    initializer.type.source_view(),
                    "But the initializer is of type {1}",
                },
            });

            if (!pattern.is_exhaustive_by_itself) {
                context.error(pattern.source_view, {
                    .message   = "An inexhaustive pattern can not be used in a let-binding",
                    .help_note = "If you wish to conditionally bind the expression when the pattern matches, use 'if let'",
                });
            }

            return {
                .value = mir::expression::Let_binding {
                    .pattern     = context.wrap(std::move(pattern)),
                    .type        = type,
                    .initializer = context.wrap(std::move(initializer)),
                },
                .type        = context.unit_type(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
            };
        }

        auto operator()(hir::expression::Conditional& conditional) -> mir::Expression {
            mir::Expression condition = recurse(*conditional.condition);

            context.solve(constraint::Type_equality {
                .constrainer_type = context.boolean_type(condition.source_view),
                .constrained_type = condition.type,
                .constrained_note {
                    condition.source_view,
                    "This should be of type {0}, not {1}"
                }
            });

            mir::Expression true_branch  = recurse(*conditional.true_branch);
            mir::Expression false_branch = recurse(*conditional.false_branch);

            if (conditional.has_explicit_false_branch.get()) {
                switch (conditional.kind.get()) {
                case hir::expression::Conditional::Kind::normal_conditional:
                    context.solve(constraint::Type_equality {
                        .constrainer_type = true_branch.type,
                        .constrained_type = false_branch.type,
                        .constrainer_note = constraint::Explanation {
                            true_branch.type.source_view(),
                            "The true branch is of type {0}"
                        },
                        .constrained_note {
                            false_branch.type.source_view(),
                            "But the false branch is of type {1}"
                        }
                    });
                    break;
                case hir::expression::Conditional::Kind::while_loop_body:
                    context.solve(constraint::Type_equality {
                        .constrainer_type = context.unit_type(true_branch.source_view),
                        .constrained_type = true_branch.type,
                        .constrained_note = constraint::Explanation {
                            true_branch.type.source_view(),
                            "The body of a while loop must be of the unit type, not {1}"
                        }
                    });
                    break;
                default:
                    utl::unreachable();
                }
            }
            else { // no explicit false branch
                context.solve(constraint::Type_equality {
                    .constrainer_type = context.unit_type(this_expression.source_view),
                    .constrained_type = true_branch.type,
                    .constrainer_note = constraint::Explanation {
                        this_expression.source_view,
                        "This `if` expression has no `else` block, so the true branch must be of the unit type"
                    },
                    .constrained_note = constraint::Explanation {
                        true_branch.type.source_view(),
                        "But the true branch is of type {1}"
                    }
                });
            }

            mir::Type const result_type = true_branch.type;
            bool const is_pure = condition.is_pure && true_branch.is_pure && false_branch.is_pure;

            return {
                .value = mir::expression::Conditional {
                    .condition    = context.wrap(std::move(condition)),
                    .true_branch  = context.wrap(std::move(true_branch)),
                    .false_branch = context.wrap(std::move(false_branch))
                },
                .type           = result_type.with(this_expression.source_view),
                .source_view    = this_expression.source_view,
                .mutability     = context.immut_constant(this_expression.source_view),
                .is_pure        = is_pure,
            };
        }

        auto operator()(hir::expression::Match& match) -> mir::Expression {
            utl::always_assert(!match.cases.empty());

            mir::Expression matched_expression = recurse(*match.matched_expression);
            auto cases = utl::vector_with_capacity<mir::expression::Match::Case>(match.cases.size());

            tl::optional<mir::Type> previous_case_result_type;

            for (hir::expression::Match::Case& match_case : match.cases) {
                Scope case_scope = scope.make_child();

                mir::Pattern    pattern = context.resolve_pattern(*match_case.pattern, matched_expression.type, case_scope, space);
                mir::Expression handler = recurse(*match_case.handler, &case_scope);

                if (previous_case_result_type.has_value()) {
                    context.solve(constraint::Type_equality {
                        .constrainer_type = *previous_case_result_type,
                        .constrained_type = handler.type,
                        .constrained_note {
                            handler.source_view,
                            "The previous case handlers were of type {0}, but this is of type {1}"
                        }
                    });
                }
                previous_case_result_type = handler.type;

                cases.push_back(mir::expression::Match::Case {
                    .pattern = context.wrap(std::move(pattern)),
                    .handler = context.wrap(std::move(handler)),
                });
            }

            return {
                .value = mir::expression::Match {
                    .cases              = std::move(cases),
                    .matched_expression = context.wrap(std::move(matched_expression))
                },
                .type        = utl::get(previous_case_result_type),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view)
            };
        }

        auto operator()(hir::expression::Struct_initializer& struct_initializer) -> mir::Expression {
            mir::Type const struct_type =
                context.resolve_type(*struct_initializer.struct_type, scope, space);

            if (auto* const structure_ptr = std::get_if<mir::type::Structure>(&*struct_type.flattened_value())) {
                mir::Struct& structure = context.resolve_struct(structure_ptr->info);
                auto initializers = utl::vector_with_capacity<mir::Expression>(structure.members.size());

                for (auto const& [name, _] : struct_initializer.member_initializers) {
                    if (!ranges::contains(structure.members, name, &mir::Struct::Member::name))
                        context.error(name.source_view, { "{} does not have a member '{}'"_format(struct_type, name) });
                }

                for (mir::Struct::Member& member : structure.members) {
                    if (utl::wrapper auto* const member_initializer_ptr = struct_initializer.member_initializers.find(member.name)) {
                        mir::Expression member_initializer = recurse(**member_initializer_ptr);
                        context.solve(constraint::Type_equality {
                            .constrainer_type = member.type,
                            .constrained_type = member_initializer.type,
                            .constrainer_note = constraint::Explanation {
                                member.name.source_view,
                                "This member is of type {0}",
                            },
                            .constrained_note {
                                member_initializer.source_view,
                                "But the given initializer is of type {1}",
                            }
                        });
                        initializers.push_back(std::move(member_initializer));
                    }
                    else {
                        context.error(this_expression.source_view, { "Field '{}' is not initialized"_format(member.name) });
                    }
                }

                bool const is_pure = ranges::all_of(initializers, &mir::Expression::is_pure);

                return {
                    .value = mir::expression::Struct_initializer {
                        .initializers = std::move(initializers),
                        .struct_type  = struct_type,
                    },
                    .type        = struct_type,
                    .source_view = this_expression.source_view,
                    .mutability  = context.immut_constant(this_expression.source_view),
                    .is_pure     = is_pure,
                };
            }
            else {
                utl::todo();
            }
        }

        auto operator()(hir::expression::Type_cast& cast) -> mir::Expression {
            if (cast.cast_kind == ast::expression::Type_cast::Kind::ascription) {
                mir::Expression result = recurse(*cast.expression);
                context.solve(constraint::Type_equality {
                    .constrainer_type = context.resolve_type(*cast.target_type, scope, space),
                    .constrained_type = result.type,
                    .constrainer_note = constraint::Explanation {
                        cast.target_type->source_view,
                        "The ascribed type is {0}"
                    },
                    .constrained_note {
                        cast.expression->source_view,
                        "But the actual type is {1}"
                    }
                });
                result.type = result.type.with(cast.target_type->source_view);
                return result;
            }
            else {
                utl::todo();
            }
        }

        auto operator()(hir::expression::Template_application& application) -> mir::Expression {
            return utl::match(context.find_lower(application.name, scope, space),
                [&](utl::Wrapper<Function_info> const info) -> mir::Expression {
                    if (!context.resolve_function_signature(*info).is_template()) {
                        context.error(application.name.primary_name.source_view, {
                            .message   = "'{}' is a concrete function, not a function template"_format(application.name),
                            .help_note = "If you did mean to refer to '{}', simply remove the template argument list"_format(application.name),
                        });
                    }
                    utl::Wrapper<Function_info> const concrete =
                        context.instantiate_function_template(info, application.template_arguments, this_expression.source_view, scope, space);
                    return {
                        .value       = mir::expression::Function_reference { .info = concrete, .is_application = true },
                        .type        = context.resolve_function_signature(*concrete).function_type.with(this_expression.source_view),
                        .source_view = this_expression.source_view,
                        .mutability  = context.immut_constant(this_expression.source_view),
                        .is_pure     = true,
                    };
                },
                [](mir::Enum_constructor const) -> mir::Expression {
                    utl::todo();
                },
                [](utl::Wrapper<Namespace> const) -> mir::Expression {
                    utl::todo();
                }
            );
        }

        auto operator()(hir::expression::Invocation& invocation) -> mir::Expression {
            return resolve_invocation(recurse(*invocation.invocable), resolve_arguments(invocation.arguments));
        }

        auto operator()(hir::expression::Method_invocation &invocation) -> mir::Expression {
            mir::Expression base_expression = recurse(*invocation.base_expression);

            utl::Wrapper<Function_info> const method_info = context.resolve_method(
                invocation.method_name,
                invocation.template_arguments,
                base_expression.type,
                scope,
                space);
            mir::Function& method = context.resolve_function(method_info);

            auto arguments = resolve_arguments(invocation.arguments);
            arguments.insert(
                arguments.begin(),
                std::invoke([&] {
                    if (!method.signature.self_parameter.has_value() || !method.signature.self_parameter->is_reference.get()) {
                        return std::move(base_expression);
                    }
                    else {
                        utl::Source_view const base_source_view = base_expression.source_view;
                        return take_reference(
                            context,
                            std::move(base_expression),
                            method.signature.self_parameter->mutability.with(base_source_view),
                            base_source_view);
                    }
                })
            );

            return resolve_direct_invocation(
                mir::expression::Function_reference {
                    .info           = method_info,
                    .is_application = invocation.template_arguments.has_value()
                },
                std::move(arguments));
        }

        auto operator()(hir::expression::Struct_field_access& access) -> mir::Expression {
            mir::Expression       base_expression = recurse(*access.base_expression);
            mir::Mutability const mutability      = base_expression.mutability;
            bool            const is_addressable  = base_expression.is_addressable;
            bool            const is_pure         = base_expression.is_pure;

            mir::Type const field_type = context.fresh_general_unification_type_variable(
                this_expression.source_view);

            context.solve(constraint::Struct_field {
                .struct_type      = base_expression.type,
                .field_type       = field_type,
                .field_identifier = access.field_name.identifier,
                .explanation {
                    access.field_name.source_view,
                    "Invalid named field access"
                }
            });
            return {
                .value = mir::expression::Struct_field_access {
                    .base_expression = context.wrap(std::move(base_expression)),
                    .field_name      = access.field_name,
                },
                .type           = field_type,
                .source_view    = this_expression.source_view,
                .mutability     = mutability,
                .is_addressable = is_addressable,
                .is_pure        = is_pure,
            };
        }

        auto operator()(hir::expression::Tuple_field_access& access) -> mir::Expression {
            mir::Expression       base_expression = recurse(*access.base_expression);
            mir::Mutability const mutability      = base_expression.mutability;
            bool            const is_addressable  = base_expression.is_addressable;
            bool            const is_pure         = base_expression.is_pure;

            mir::Type const field_type = context.fresh_general_unification_type_variable(
                this_expression.source_view);

            context.solve(constraint::Tuple_field {
                .tuple_type  = base_expression.type,
                .field_type  = field_type,
                .field_index = access.field_index.get(),
                .explanation {
                    access.field_index_source_view,
                    "Invalid indexed field access"
                }
            });
            return {
                .value = mir::expression::Tuple_field_access {
                    .base_expression         = context.wrap(std::move(base_expression)),
                    .field_index             = access.field_index.get(),
                    .field_index_source_view = access.field_index_source_view
                },
                .type           = field_type,
                .source_view    = this_expression.source_view,
                .mutability     = mutability,
                .is_addressable = is_addressable,
                .is_pure        = is_pure,
            };
        }

        auto operator()(hir::expression::Sizeof& sizeof_) -> mir::Expression {
            return {
                .value = mir::expression::Sizeof {
                    .inspected_type = context.resolve_type(*sizeof_.inspected_type, scope, space)
                },
                .type           = context.size_type(this_expression.source_view),
                .source_view    = this_expression.source_view,
                .mutability     = context.immut_constant(this_expression.source_view),
                .is_pure        = true,
            };
        }

        auto operator()(hir::expression::Reference& reference) -> mir::Expression {
            return take_reference(
                context,
                recurse(*reference.referenced_expression),
                context.resolve_mutability(reference.mutability, scope),
                this_expression.source_view);
        }

        auto operator()(hir::expression::Dereference& dereference) -> mir::Expression {
            mir::Expression dereferenced_expression = recurse(*dereference.dereferenced_expression);
            bool const is_pure = dereferenced_expression.is_pure;

            if (auto const* const reference = std::get_if<mir::type::Reference>(&*dereferenced_expression.type.flattened_value())) {
                // If the type of the dereferenced expression is already known to
                // be mir::type::Reference, there is no need to solve constraints.

                return {
                    .value          = mir::expression::Dereference { context.wrap(std::move(dereferenced_expression)) },
                    .type           = reference->referenced_type,
                    .source_view    = this_expression.source_view,
                    .mutability     = reference->mutability,
                    .is_addressable = true,
                    .is_pure        = is_pure,
                };
            }
            else {
                mir::Type       const referenced_type      = context.fresh_general_unification_type_variable(dereferenced_expression.source_view);
                mir::Mutability const reference_mutability = context.fresh_unification_mutability_variable(this_expression.source_view);

                mir::Type const reference_type {
                    context.wrap_type(mir::type::Reference {
                        .mutability      = reference_mutability,
                        .referenced_type = referenced_type
                    }),
                    referenced_type.source_view(),
                };

                context.solve(constraint::Type_equality {
                    .constrainer_type = reference_type,
                    .constrained_type = dereferenced_expression.type,
                    .constrainer_note = constraint::Explanation {
                        this_expression.source_view,
                        "Only expressions of reference types (&T or &mut T) can be dereferenced"
                    },
                    .constrained_note {
                        dereferenced_expression.source_view,
                        "But this expression is of type {0}"
                    }
                });

                return {
                    .value          = mir::expression::Dereference { context.wrap(std::move(dereferenced_expression)) },
                    .type           = referenced_type,
                    .source_view    = this_expression.source_view,
                    .mutability     = reference_mutability,
                    .is_addressable = true,
                    .is_pure        = is_pure,
                };
            }
        }

        auto operator()(hir::expression::Addressof& addressof) -> mir::Expression {
            mir::Expression lvalue = recurse(*addressof.lvalue);
            bool const is_pure = lvalue.is_pure;
            require_addressability(context, lvalue, "The address of a temporary object can not be taken");

            mir::Type const pointer_type {
                context.wrap_type(mir::type::Pointer {
                    .mutability      = lvalue.mutability,
                    .pointed_to_type = lvalue.type
                }),
                this_expression.source_view,
            };

            return {
                .value       = mir::expression::Addressof { context.wrap(std::move(lvalue)) },
                .type        = pointer_type,
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
                .is_pure     = is_pure,
            };
        }

        auto operator()(hir::expression::Unsafe_dereference& dereference) -> mir::Expression {
            mir::Expression pointer = recurse(*dereference.pointer);
            bool const is_pure = pointer.is_pure;

            mir::Type       const lvalue_type       = context.fresh_general_unification_type_variable(this_expression.source_view);
            mir::Mutability const lvalue_mutability = context.fresh_unification_mutability_variable(this_expression.source_view);

            mir::Type const pointer_type {
                context.wrap_type(mir::type::Pointer {
                    .mutability      = lvalue_mutability,
                    .pointed_to_type = lvalue_type
                }),
                pointer.source_view,
            };

            context.solve(constraint::Type_equality {
                .constrainer_type = pointer_type,
                .constrained_type = pointer.type,
                .constrainer_note = constraint::Explanation {
                    this_expression.source_view,
                    "The operand of unsafe_dereference must be of a pointer type"
                },
                .constrained_note {
                    pointer.source_view,
                    "But this expression is of type {1}"
                }
            });

            return {
                .value          = mir::expression::Unsafe_dereference { context.wrap(std::move(pointer)) },
                .type           = lvalue_type,
                .source_view    = this_expression.source_view,
                .mutability     = lvalue_mutability,
                .is_addressable = true,
                .is_pure        = is_pure,
            };
        }

        auto operator()(hir::expression::Self) -> mir::Expression {
            if (auto self = try_resolve_local_variable_reference(context.self_variable_id))
                return std::move(*self);
            context.error(this_expression.source_view, {
                .message   = "'self' can only be used within a method",
                .help_note = "A method is a function that takes 'self', '&self', or '&mut self' as its first parameter"
            });
        }

        auto operator()(hir::expression::Hole&) -> mir::Expression {
            // Workaround for a bug in GCC 13.1.1.
            // Directly constructing `mir::Expression::Variant` from a `mir::Expression::Hole` causes GCC to get stuck and its memory usage to grow without bound.

            mir::Expression::Variant value;
            value.emplace<mir::expression::Hole>();
            return {
                .value       = std::move(value),
                .type        = context.fresh_general_unification_type_variable(this_expression.source_view),
                .source_view = this_expression.source_view,
                .mutability  = context.immut_constant(this_expression.source_view),
                .is_pure     = true,
            };
        }


        auto operator()(hir::expression::Array_index_access&) -> mir::Expression {
            utl::todo();
        }
        auto operator()(hir::expression::Ret&) -> mir::Expression {
            utl::todo();
        }
        auto operator()(hir::expression::Binary_operator_invocation&) -> mir::Expression {
            utl::todo();
        }
        auto operator()(hir::expression::Placement_init&) -> mir::Expression {
            utl::todo();
        }
        auto operator()(hir::expression::Meta&) -> mir::Expression {
            utl::todo();
        }
    };

}


auto libresolve::Context::resolve_expression(hir::Expression& expression, Scope& scope, Namespace& space) -> mir::Expression {
    return std::visit(Expression_resolution_visitor { *this, scope, space, expression }, expression.value);
}
