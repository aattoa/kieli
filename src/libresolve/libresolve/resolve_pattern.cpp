#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    struct Pattern_resolution_visitor {
        Context&            context;
        Unification_state&  state;
        Scope&              scope;
        Environment_wrapper environment;
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
                state.fresh_integral_type_variable(context.arenas, this_pattern.source_range),
                this_pattern.source_range,
            };
        }

        auto operator()(kieli::Floating const& floating) -> hir::Pattern
        {
            return {
                floating,
                hir::Type { context.constants.floating_type, this_pattern.source_range },
                this_pattern.source_range,
            };
        }

        auto operator()(kieli::Character const& character) -> hir::Pattern
        {
            return {
                character,
                hir::Type { context.constants.character_type, this_pattern.source_range },
                this_pattern.source_range,
            };
        }

        auto operator()(kieli::Boolean const& boolean) -> hir::Pattern
        {
            return {
                boolean,
                hir::Type { context.constants.boolean_type, this_pattern.source_range },
                this_pattern.source_range,
            };
        }

        auto operator()(kieli::String const& string) -> hir::Pattern
        {
            return {
                string,
                hir::Type { context.constants.string_type, this_pattern.source_range },
                this_pattern.source_range,
            };
        }

        auto operator()(ast::Wildcard const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Name const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Abbreviated_constructor const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Tuple const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Slice const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Alias const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(ast::pattern::Guarded const&) -> hir::Pattern
        {
            cpputil::todo();
        }
    };
} // namespace

auto libresolve::resolve_pattern(
    Context&            context,
    Unification_state&  state,
    Scope&              scope,
    Environment_wrapper environment,
    ast::Pattern const& pattern) -> hir::Pattern
{
    return std::visit(
        Pattern_resolution_visitor { context, state, scope, environment, pattern }, pattern.value);
}
