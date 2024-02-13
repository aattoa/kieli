#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::resolve_mutability(
    Context&               context,
    Unification_state&     state,
    Scope&                 scope,
    ast::Mutability const& mutability) -> hir::Mutability
{
    (void)context;
    (void)state;
    (void)scope;
    (void)mutability;
    cpputil::todo();
}
