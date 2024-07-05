#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    struct Pattern_resolution_visitor {
        Context&            context;
        Inference_state&    state;
        Scope&              scope;
        hir::Environment_id environment;
        ast::Pattern const& this_pattern;

        auto recurse()
        {
            return [&](ast::Pattern const& pattern) -> hir::Pattern {
                return resolve_pattern(context, state, scope, environment, pattern);
            };
        }

        auto recurse(ast::Pattern const& pattern) -> hir::Pattern
        {
            return recurse()(pattern);
        }

        auto operator()(kieli::Integer const& integer) -> hir::Pattern
        {
            return {
                integer,
                state.fresh_integral_type_variable(context.arenas, this_pattern.range),
                this_pattern.range,
            };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Pattern
        {
            return {
                floating,
                hir::Type { context.constants.floating_type, this_pattern.range },
                this_pattern.range,
            };
        }

        auto operator()(kieli::Character const& character) -> hir::Pattern
        {
            return {
                character,
                hir::Type { context.constants.character_type, this_pattern.range },
                this_pattern.range,
            };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Pattern
        {
            return {
                boolean,
                hir::Type { context.constants.boolean_type, this_pattern.range },
                this_pattern.range,
            };
        }

        auto operator()(kieli::String const& string) -> hir::Pattern
        {
            return {
                string,
                hir::Type { context.constants.string_type, this_pattern.range },
                this_pattern.range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            return {
                hir::pattern::Wildcard {},
                state.fresh_general_type_variable(context.arenas, this_pattern.range),
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Name const& pattern) -> hir::Pattern
        {
            hir::Mutability const mutability
                = resolve_mutability(context, scope, pattern.mutability);
            hir::Type const type
                = state.fresh_general_type_variable(context.arenas, pattern.name.range);
            hir::Local_variable_tag const tag = context.tag_state.fresh_local_variable_tag();

            scope.bind_variable(
                pattern.name.identifier,
                Variable_bind {
                    .name       = pattern.name,
                    .type       = type,
                    .mutability = mutability,
                    .tag        = tag,
                });

            return {
                hir::pattern::Name {
                    .mutability   = mutability,
                    .identifier   = pattern.name.identifier,
                    .variable_tag = tag,
                },
                type,
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Alias const& alias) -> hir::Pattern
        {
            hir::Pattern          pattern    = recurse(*alias.pattern);
            hir::Mutability const mutability = resolve_mutability(context, scope, alias.mutability);
            hir::Local_variable_tag const tag = context.tag_state.fresh_local_variable_tag();

            scope.bind_variable(
                alias.name.identifier,
                Variable_bind {
                    .name       = alias.name,
                    .type       = pattern.type,
                    .mutability = mutability,
                    .tag        = tag,
                });

            return {
                hir::pattern::Alias {
                    .mutability   = mutability,
                    .identifier   = alias.name.identifier,
                    .variable_tag = tag,
                    .pattern      = context.arenas.wrap(std::move(pattern)),
                },
                pattern.type,
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Abbreviated_constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Tuple const& tuple) -> hir::Pattern
        {
            auto patterns = std::views::transform(tuple.field_patterns, recurse())
                          | std::ranges::to<std::vector>();
            auto types = std::views::transform(patterns, &hir::Pattern::type)
                       | std::ranges::to<std::vector>();
            return {
                hir::pattern::Tuple {
                    .field_patterns = std::move(patterns),
                },
                hir::Type {
                    context.arenas.type(hir::type::Tuple { std::move(types) }),
                    this_pattern.range,
                },
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Slice const& slice) -> hir::Pattern
        {
            auto patterns = std::views::transform(slice.element_patterns, recurse())
                          | std::ranges::to<std::vector>();

            hir::Type const element_type
                = state.fresh_general_type_variable(context.arenas, this_pattern.range);

            for (hir::Pattern const& pattern : patterns) {
                require_subtype_relationship(
                    context.compile_info.diagnostics,
                    state,
                    *pattern.type.variant,
                    *element_type.variant);
            }

            return {
                hir::pattern::Slice {
                    .patterns = std::move(patterns),
                },
                hir::Type {
                    context.arenas.type(hir::type::Slice { element_type }),
                    this_pattern.range,
                },
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Guarded const& guarded) -> hir::Pattern
        {
            hir::Pattern    guarded_pattern = recurse(*guarded.guarded_pattern);
            hir::Expression guard_expression
                = resolve_expression(context, state, scope, environment, guarded.guard_expression);

            require_subtype_relationship(
                context.compile_info.diagnostics,
                state,
                *guard_expression.type.variant,
                *context.constants.boolean_type);

            return {
                hir::pattern::Guarded {
                    .guarded_pattern  = context.arenas.wrap(std::move(guarded_pattern)),
                    .guard_expression = context.arenas.wrap(std::move(guard_expression)),
                },
                guarded_pattern.type,
                this_pattern.range,
            };
        }
    };
} // namespace

auto libresolve::resolve_pattern(
    Context&            context,
    Inference_state&    state,
    Scope&              scope,
    hir::Environment_id environment,
    ast::Pattern const& pattern) -> hir::Pattern
{
    Pattern_resolution_visitor visitor { context, state, scope, environment, pattern };
    return std::visit(visitor, pattern.variant);
}
