#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace libresolve;

namespace {
    struct Pattern_resolution_visitor {
        Context&            context;
        Inference_state&    state;
        hir::Scope_id       scope_id;
        hir::Environment_id environment_id;
        ast::Pattern const& this_pattern;

        auto ast() -> ast::Arena&
        {
            return context.documents.at(state.document_id).ast;
        }

        auto recurse()
        {
            return [&](ast::Pattern const& pattern) -> hir::Pattern {
                return resolve_pattern(context, state, scope_id, environment_id, pattern);
            };
        }

        auto recurse(ast::Pattern const& pattern) -> hir::Pattern
        {
            return recurse()(pattern);
        }

        auto operator()(kieli::Integer const& integer) -> hir::Pattern
        {
            hir::Type type = fresh_integral_type_variable(state, context.hir, this_pattern.range);
            return { integer, type.id, this_pattern.range };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Pattern
        {
            return { floating, context.constants.floating_type, this_pattern.range };
        }

        auto operator()(kieli::Character const& character) -> hir::Pattern
        {
            return { character, context.constants.character_type, this_pattern.range };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Pattern
        {
            return { boolean, context.constants.boolean_type, this_pattern.range };
        }

        auto operator()(kieli::String const& string) -> hir::Pattern
        {
            return { string, context.constants.string_type, this_pattern.range };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            return {
                hir::Wildcard {},
                fresh_general_type_variable(state, context.hir, this_pattern.range).id,
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Name const& pattern) -> hir::Pattern
        {
            hir::Mutability const mutability
                = resolve_mutability(context, scope_id, pattern.mutability);
            hir::Type const type
                = fresh_general_type_variable(state, context.hir, pattern.name.range);
            hir::Local_variable_tag const tag = fresh_local_variable_tag(context.tags);

            Variable_bind bind {
                .name       = pattern.name,
                .type       = type.id,
                .mutability = mutability,
                .tag        = tag,
            };
            bind_variable(
                context.info.scopes.index_vector[scope_id],
                pattern.name.identifier,
                std::move(bind));

            return {
                hir::pattern::Name {
                    .mutability   = mutability,
                    .identifier   = pattern.name.identifier,
                    .variable_tag = tag,
                },
                type.id,
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
            auto types = std::views::transform(patterns, hir::pattern_type)
                       | std::ranges::to<std::vector>();
            return {
                hir::pattern::Tuple { std::move(patterns) },
                context.hir.types.push(hir::type::Tuple { std::move(types) }),
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Slice const& slice) -> hir::Pattern
        {
            auto patterns = std::views::transform(slice.element_patterns, recurse())
                          | std::ranges::to<std::vector>();

            hir::Type const element_type
                = fresh_general_type_variable(state, context.hir, this_pattern.range);

            for (hir::Pattern const& pattern : patterns) {
                require_subtype_relationship(
                    context,
                    state,
                    pattern.range,
                    context.hir.types[pattern.type],
                    context.hir.types[element_type.id]);
            }

            return {
                hir::pattern::Slice { std::move(patterns) },
                context.hir.types.push(hir::type::Slice { element_type }),
                this_pattern.range,
            };
        }

        auto operator()(ast::pattern::Guarded const& guarded) -> hir::Pattern
        {
            hir::Pattern    guarded_pattern  = recurse(ast().patterns[guarded.guarded_pattern]);
            hir::Expression guard_expression = resolve_expression(
                context, state, scope_id, environment_id, guarded.guard_expression);

            require_subtype_relationship(
                context,
                state,
                guard_expression.range,
                context.hir.types[guard_expression.type],
                context.hir.types[context.constants.boolean_type]);

            return {
                hir::pattern::Guarded {
                    .guarded_pattern  = context.hir.patterns.push(std::move(guarded_pattern)),
                    .guard_expression = context.hir.expressions.push(std::move(guard_expression)),
                },
                guarded_pattern.type,
                this_pattern.range,
            };
        }
    };
} // namespace

auto libresolve::resolve_pattern(
    Context&                  context,
    Inference_state&          state,
    hir::Scope_id const       scope_id,
    hir::Environment_id const environment_id,
    ast::Pattern const&       pattern) -> hir::Pattern
{
    Pattern_resolution_visitor visitor { context, state, scope_id, environment_id, pattern };
    return std::visit(visitor, pattern.variant);
}
