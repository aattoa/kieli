#include "utl/utilities.hpp"
#include "resolution_internals.hpp"


namespace {

    using namespace resolution;


    struct Pattern_resolution_visitor {
        Context     & context;
        Scope       & scope;
        Namespace   & space;
        hir::Pattern& this_pattern;

        auto recurse(hir::Pattern& pattern) -> mir::Pattern {
            return context.resolve_pattern(pattern, scope, space);
        }
        auto recurse() noexcept {
            return [this](hir::Pattern& pattern) -> mir::Pattern {
                return recurse(pattern);
            };
        }


        auto operator()(hir::pattern::Wildcard) -> mir::Pattern {
            return {
                .value                   = mir::pattern::Wildcard {},
                .type                    = context.fresh_general_unification_type_variable(this_pattern.source_view),
                .is_exhaustive_by_itself = true,
                .source_view             = this_pattern.source_view
            };
        }

        template <class T>
        auto operator()(hir::pattern::Literal<T>& literal) -> mir::Pattern {
            return {
                .value       = mir::pattern::Literal<T> { literal.value },
                .type        = context.literal_type<T>(this_pattern.source_view),
                .source_view = this_pattern.source_view
            };
        }

        auto operator()(hir::pattern::Name& name) -> mir::Pattern {
            mir::Type       const type       = context.fresh_general_unification_type_variable(this_pattern.source_view);
            mir::Mutability const mutability = context.resolve_mutability(name.mutability, scope);

            auto const variable_tag = context.fresh_local_variable_tag();

            scope.bind_variable(name.identifier, {
                .type               = type,
                .mutability         = mutability,
                .variable_tag       = variable_tag,
                .has_been_mentioned = false,
                .source_view        = this_pattern.source_view
            });

            return {
                .value = mir::pattern::Name {
                    .variable_tag = variable_tag,
                    .identifier   = name.identifier,
                    .mutability   = mutability
                },
                .type                    = type,
                .is_exhaustive_by_itself = true,
                .source_view             = this_pattern.source_view
            };
        }

        auto operator()(hir::pattern::Tuple& tuple) -> mir::Pattern {
            mir::pattern::Tuple mir_tuple {
                .field_patterns = utl::vector_with_capacity(tuple.field_patterns.size())
            };
            mir::type::Tuple mir_tuple_type {
                .field_types = utl::vector_with_capacity(tuple.field_patterns.size())
            };

            bool is_exhaustive = true;

            for (hir::Pattern& pattern : tuple.field_patterns) {
                mir::Pattern mir_pattern = recurse(pattern);

                if (!mir_pattern.is_exhaustive_by_itself)
                    is_exhaustive = false;

                mir_tuple_type.field_types.push_back(mir_pattern.type);
                mir_tuple.field_patterns.push_back(std::move(mir_pattern));
            }

            return {
                .value = std::move(mir_tuple),
                .type {
                    .value       = context.wrap_type(std::move(mir_tuple_type)),
                    .source_view = this_pattern.source_view
                },
                .is_exhaustive_by_itself = is_exhaustive,
                .source_view             = this_pattern.source_view
            };
        }

        auto operator()(hir::pattern::As& as) -> mir::Pattern {
            mir::Pattern          aliased_pattern = recurse(*as.aliased_pattern);
            mir::Mutability const mutability      = context.resolve_mutability(as.alias.mutability, scope);

            (void)scope.bind_variable(as.alias.identifier, {
                .type               = aliased_pattern.type,
                .mutability         = mutability,
                .variable_tag       = context.fresh_local_variable_tag(),
                .has_been_mentioned = false,
                .source_view        = this_pattern.source_view
            });
            return aliased_pattern;
        }

        auto operator()(hir::pattern::Constructor& hir_constructor) -> mir::Pattern {
            return utl::match(
                context.find_lower(hir_constructor.constructor_name, scope, space),

                [&, this](utl::Wrapper<resolution::Function_info>) -> mir::Pattern {
                    context.error(this_pattern.source_view, { "Expected a constructor, but found a function" });
                },
                [&, this](utl::Wrapper<resolution::Function_template_info>) -> mir::Pattern {
                    context.error(this_pattern.source_view, { "Expected a constructor, but found a function template" });
                },
                [&, this](utl::Wrapper<resolution::Namespace>) -> mir::Pattern {
                    context.error(this_pattern.source_view, { "Expected a constructor, but found a namespace" });
                },
                [&, this](mir::Enum_constructor constructor) -> mir::Pattern {
                    tl::optional<mir::Pattern> payload_pattern;

                    if (hir_constructor.payload_pattern.has_value()) {
                        if (constructor.payload_type.has_value()) {
                            payload_pattern = recurse(*utl::get(hir_constructor.payload_pattern));
                            context.solve(constraint::Type_equality {
                                .constrainer_type = *constructor.payload_type,
                                .constrained_type = payload_pattern->type,
                                .constrainer_note = constraint::Explanation {
                                    constructor.payload_type->source_view,
                                    "The constructor field is of type {0}"
                                },
                                .constrained_note {
                                    (*hir_constructor.payload_pattern)->source_view,
                                    "But the given pattern is of type {1}"
                                }
                            });
                        }
                        else {
                            context.error(this_pattern.source_view, { "This constructor has no fields" });
                        }
                    }
                    else if (constructor.payload_type.has_value()) {
                        context.error(this_pattern.source_view, { "This constructor has fields which must be handled in a pattern" });
                    }

                    bool const is_exhaustive = (!payload_pattern || payload_pattern->is_exhaustive_by_itself)
                        && utl::get<mir::type::Enumeration>(*constructor.enum_type.value).info->constructor_count() == 1;

                    return {
                        .value = mir::pattern::Enum_constructor {
                            .payload_pattern = std::move(payload_pattern).transform(context.wrap()),
                            .constructor     = constructor
                        },
                        .type                    = constructor.enum_type,
                        .is_exhaustive_by_itself = is_exhaustive,
                        .source_view             = this_pattern.source_view
                    };
                }
            );
        }

        auto operator()(hir::pattern::Slice& slice) -> mir::Pattern {
            if (slice.element_patterns.empty()) {
                return {
                    .value = mir::pattern::Slice {},
                    .type {
                        .value = context.wrap_type(mir::type::Slice {
                            .element_type = context.fresh_general_unification_type_variable(this_pattern.source_view)
                        }),
                        .source_view = this_pattern.source_view
                    },
                    .source_view = this_pattern.source_view
                };
            }
            else {
                mir::pattern::Slice mir_slice {
                    .element_patterns = utl::vector_with_capacity(slice.element_patterns.size())
                };

                mir_slice.element_patterns.push_back(recurse(slice.element_patterns.front()));
                mir::Type const element_type = mir_slice.element_patterns.front().type;

                for (utl::Usize i = 1; i != slice.element_patterns.size(); ++i) {
                    hir::Pattern& previous_pattern = slice.element_patterns[i - 1];
                    hir::Pattern& current_pattern  = slice.element_patterns[i];

                    mir::Pattern pattern = recurse(current_pattern);

                    context.solve(constraint::Type_equality {
                        .constrainer_type = element_type,
                        .constrained_type = pattern.type,
                        .constrainer_note = constraint::Explanation {
                            element_type.source_view + previous_pattern.source_view,
                            i == 1 ? "The previous pattern was of type {0}"
                                   : "The previous patterns were of type {0}"
                        },
                        .constrained_note {
                            current_pattern.source_view,
                            "But this pattern is of type {1}"
                        }
                    });
                    mir_slice.element_patterns.push_back(std::move(pattern));
                }

                return {
                    .value = std::move(mir_slice),
                    .type {
                        .value       = context.wrap_type(mir::type::Slice { element_type }),
                        .source_view = this_pattern.source_view
                    },
                    .source_view = this_pattern.source_view
                };
            }
        }

        auto operator()(hir::pattern::Guarded& guarded) -> mir::Pattern {
            mir::Pattern    guarded_pattern = recurse(*guarded.guarded_pattern);
            mir::Type       pattern_type    = guarded_pattern.type;
            mir::Expression guard           = context.resolve_expression(guarded.guard, scope, space);

            context.solve(constraint::Type_equality {
                .constrainer_type = context.boolean_type(this_pattern.source_view),
                .constrained_type = guard.type,
                .constrained_note {
                    guard.source_view,
                    "The pattern guard expression must be of type Bool, but found {1}"
                }
            });

            return {
                .value = mir::pattern::Guarded {
                    .guarded_pattern = context.wrap(std::move(guarded_pattern)),
                    .guard           = std::move(guard)
                },
                .type        = pattern_type,
                .source_view = this_pattern.source_view
            };
        }
    };

}


auto resolution::Context::resolve_pattern(
    hir::Pattern& pattern,
    Scope       & scope,
    Namespace   & space) -> mir::Pattern
{
    return std::visit(
        Pattern_resolution_visitor {
            .context      = *this,
            .scope        = scope,
            .space        = space,
            .this_pattern = pattern
        },
        pattern.value);
}
