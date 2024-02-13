#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace hir = libresolve::hir;

namespace {
    struct Pattern_resolution_visitor {
        libresolve::Context&                  context;
        libresolve::Unification_state&        state;
        libresolve::Scope&                    scope;
        libresolve::Environment_wrapper const environment;
        ast::Pattern const&                   this_pattern;

        auto recurse()
        {
            return [&](ast::Pattern const& pattern) -> hir::Pattern {
                return libresolve::resolve_pattern(context, state, scope, environment, pattern);
            };
        }

        auto recurse(ast::Pattern const& pattern) -> hir::Pattern
        {
            return recurse()(pattern);
        }

        auto operator()(kieli::Integer const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(kieli::Floating const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(kieli::Character const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(kieli::Boolean const&) -> hir::Pattern
        {
            cpputil::todo();
        }

        auto operator()(kieli::String const&) -> hir::Pattern
        {
            cpputil::todo();
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
