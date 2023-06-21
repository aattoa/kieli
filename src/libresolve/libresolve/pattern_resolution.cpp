#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;


namespace {
    struct Pattern_resolution_visitor {
        Context     & context;
        Scope       & scope;
        Namespace   & space;
        ast::Pattern& this_pattern;
        mir::Type     matched_type;

        auto recurse(ast::Pattern& pattern, mir::Type const type) -> mir::Pattern {
            return context.resolve_pattern(pattern, type, scope, space);
        }

        auto solve_pattern_type_constraint(mir::Type const pattern_type) -> void {
            context.solve(constraint::Type_equality {
                .constrainer_type = pattern_type,
                .constrained_type = matched_type,
                .constrainer_note = constraint::Explanation {
                    pattern_type.source_view(),
                    "This pattern is of type {0}",
                },
                .constrained_note = constraint::Explanation {
                    matched_type.source_view(),
                    "But this is of type {1}",
                },
            });
        }


        auto operator()(ast::pattern::Wildcard) -> mir::Pattern {
            return {
                .value                   = mir::pattern::Wildcard {},
                .is_exhaustive_by_itself = true,
                .source_view             = this_pattern.source_view,
            };
        }

        template <class T>
        auto operator()(ast::pattern::Literal<T>& literal) -> mir::Pattern {
            solve_pattern_type_constraint(context.literal_type<T>(this_pattern.source_view));
            return {
                .value                   = mir::pattern::Literal<T> { literal.value },
                .is_exhaustive_by_itself = false,
                .source_view             = this_pattern.source_view,
            };
        }

        auto operator()(ast::pattern::Name& name) -> mir::Pattern {
            mir::Mutability const mutability = context.resolve_mutability(name.mutability, scope);
            auto const variable_tag = context.fresh_local_variable_tag();

            scope.bind_variable(context, name.name.identifier, {
                .type               = matched_type,
                .mutability         = mutability,
                .variable_tag       = variable_tag,
                .has_been_mentioned = false,
                .source_view        = this_pattern.source_view,
            });

            return {
                .value = mir::pattern::Name {
                    .variable_tag = variable_tag,
                    .identifier   = name.name.identifier,
                    .mutability   = mutability,
                },
                .is_exhaustive_by_itself = true,
                .source_view             = this_pattern.source_view,
            };
        }

        auto operator()(ast::pattern::Tuple& tuple) -> mir::Pattern {
            mir::Type const mir_tuple_type {
                context.wrap_type(mir::type::Tuple {
                    .field_types = utl::map([&](ast::Pattern& pattern) {
                        return context.fresh_general_unification_type_variable(pattern.source_view);
                    }, tuple.field_patterns)
                }),
                this_pattern.source_view,
            };
            solve_pattern_type_constraint(mir_tuple_type);

            std::span<mir::Type const> const field_types =
                utl::get<mir::type::Tuple>(*mir_tuple_type.pure_value()).field_types;

            mir::pattern::Tuple mir_tuple_pattern {
                .field_patterns = utl::vector_with_capacity(tuple.field_patterns.size())
            };
            for (auto&& [pattern, type] : ranges::views::zip(tuple.field_patterns, field_types))
                mir_tuple_pattern.field_patterns.push_back(recurse(pattern, type));

            bool const is_exhaustive =
                ranges::all_of(mir_tuple_pattern.field_patterns, &mir::Pattern::is_exhaustive_by_itself);

            return {
                .value                   = std::move(mir_tuple_pattern),
                .is_exhaustive_by_itself = is_exhaustive,
                .source_view             = this_pattern.source_view,
            };
        }

        auto operator()(ast::pattern::Alias& alias) -> mir::Pattern {
            mir::Pattern          aliased_pattern = recurse(*alias.aliased_pattern, matched_type);
            mir::Mutability const mutability      = context.resolve_mutability(alias.alias_mutability, scope);

            scope.bind_variable(context, alias.alias_name.identifier, {
                .type               = matched_type,
                .mutability         = mutability,
                .variable_tag       = context.fresh_local_variable_tag(),
                .has_been_mentioned = false,
                .source_view        = this_pattern.source_view,
            });
            return aliased_pattern;
        }


        auto handle_constructor_pattern(
            mir::Enum_constructor                    const constructor,
            tl::optional<utl::Wrapper<ast::Pattern>> const ast_payload_pattern) -> mir::Pattern
        {
            solve_pattern_type_constraint(constructor.enum_type);
            tl::optional<mir::Pattern> mir_payload_pattern;

            if (ast_payload_pattern.has_value()) {
                if (constructor.payload_type.has_value()) {
                    mir_payload_pattern = recurse(*utl::get(ast_payload_pattern), *constructor.payload_type);
                }
                else {
                    context.error(this_pattern.source_view, {
                        .message = std::format(
                            "Constructor '{}' has no fields to be handled",
                            constructor.name)
                    });
                }
            }
            else if (constructor.payload_type.has_value()) {
                context.error(this_pattern.source_view, {
                    .message = std::format(
                        "Constructor '{}' has fields which must be handled",
                        constructor.name)
                });
            }

            bool const is_exhaustive = (!mir_payload_pattern || mir_payload_pattern->is_exhaustive_by_itself)
                && utl::get<mir::type::Enumeration>(*constructor.enum_type.flattened_value()).info->constructor_count() == 1;

            return {
                .value = mir::pattern::Enum_constructor {
                    .payload_pattern = std::move(mir_payload_pattern).transform(context.wrap()),
                    .constructor     = constructor
                },
                .is_exhaustive_by_itself = is_exhaustive,
                .source_view             = this_pattern.source_view,
            };
        }

        auto operator()(ast::pattern::Constructor& ast_constructor) -> mir::Pattern {
            return utl::match(context.find_lower(ast_constructor.constructor_name, scope, space),
                [&](utl::Wrapper<Function_info>) -> mir::Pattern {
                    context.error(this_pattern.source_view, { "Expected a constructor, but found a function" });
                },
                [&](utl::Wrapper<Namespace>) -> mir::Pattern {
                    context.error(this_pattern.source_view, { "Expected a constructor, but found a namespace" });
                },
                [&](mir::Enum_constructor constructor) -> mir::Pattern {
                    return handle_constructor_pattern(constructor, ast_constructor.payload_pattern);
                });
        }

        auto operator()(ast::pattern::Abbreviated_constructor& ast_constructor) -> mir::Pattern {
            if (auto const* const enumeration_ptr = std::get_if<mir::type::Enumeration>(&*matched_type.flattened_value())) {
                mir::Enum& enumeration = context.resolve_enum(enumeration_ptr->info);
                auto const it = ranges::find(enumeration.constructors, ast_constructor.constructor_name, &mir::Enum_constructor::name);
                if (it == enumeration.constructors.end()) {
                    context.error(ast_constructor.constructor_name.source_view, {
                        .message = std::format(
                            "{} does not have a constructor '{}'",
                            mir::to_string(matched_type),
                            ast_constructor.constructor_name),
                    });
                }
                return handle_constructor_pattern(*it, ast_constructor.payload_pattern);
            }
            else if (std::holds_alternative<mir::type::Unification_variable>(*matched_type.pure_value())) {
                context.error(this_pattern.source_view, {
                    .message   = "Abbreviated constructor pattern used with an unsolved unification type variable",
                    .help_note = "This can likely be solved by providing additional type annotations, "
                                 "so that the matched type is solved before this pattern is resolved",
                });
            }
            context.error(this_pattern.source_view, {
                .message = std::format(
                    "Abbreviated constructor pattern used with non-enum type {}",
                    mir::to_string(matched_type)),
            });
        }

        auto operator()(ast::pattern::Slice& slice) -> mir::Pattern {
            mir::Type const element_type = context.fresh_general_unification_type_variable(this_pattern.source_view);

            solve_pattern_type_constraint(mir::Type {
                context.wrap_type(mir::type::Slice {
                    .element_type = element_type,
                }),
                this_pattern.source_view,
            });

            return {
                .value = mir::pattern::Slice {
                    .element_patterns = utl::map([&](ast::Pattern& element_pattern) {
                        return recurse(element_pattern, element_type);
                    }, slice.element_patterns),
                },
                .is_exhaustive_by_itself = false,
                .source_view             = this_pattern.source_view,
            };
        }

        auto operator()(ast::pattern::Guarded& guarded) -> mir::Pattern {
            mir::Pattern    guarded_pattern = recurse(*guarded.guarded_pattern, matched_type);
            mir::Expression guard           = context.resolve_expression(guarded.guard, scope, space);

            context.solve(constraint::Type_equality {
                .constrainer_type = context.boolean_type(this_pattern.source_view),
                .constrained_type = guard.type,
                .constrained_note {
                    guard.source_view,
                    "The pattern guard expression must be of type Bool, but found {1}",
                }
            });

            return {
                .value = mir::pattern::Guarded {
                    .guarded_pattern = context.wrap(std::move(guarded_pattern)),
                    .guard           = std::move(guard),
                },
                .is_exhaustive_by_itself = false,
                .source_view             = this_pattern.source_view,
            };
        }
    };
}


auto libresolve::Context::resolve_pattern(ast::Pattern& pattern, mir::Type const type, Scope& scope, Namespace& space) -> mir::Pattern {
    return std::visit(Pattern_resolution_visitor { *this, scope, space, pattern, type }, pattern.value);
}
