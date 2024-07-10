#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::lookup_lower(
    Context&                  context,
    Inference_state&          state,
    Scope&                    scope,
    hir::Environment_id const environment,
    ast::Path const&          path) -> std::optional<Lower_info>
{
    cpputil::always_assert(!path.head.is_upper());
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    cpputil::todo();
}

auto libresolve::lookup_upper(
    Context&                  context,
    Inference_state&          state,
    Scope&                    scope,
    hir::Environment_id const environment,
    ast::Path const&          path) -> std::optional<Upper_info>
{
    cpputil::always_assert(path.head.is_upper());
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    cpputil::todo();
}
