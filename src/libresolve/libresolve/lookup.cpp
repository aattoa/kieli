#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::lookup_lower(
    Context&                   context,
    Inference_state&           state,
    Scope&                     scope,
    hir::Environment_id const  environment,
    ast::Qualified_name const& name) -> std::optional<Lower_info>
{
    cpputil::always_assert(!name.is_upper());
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    cpputil::todo();
}

auto libresolve::lookup_upper(
    Context&                   context,
    Inference_state&           state,
    Scope&                     scope,
    hir::Environment_id const  environment,
    ast::Qualified_name const& name) -> std::optional<Upper_info>
{
    cpputil::always_assert(name.is_upper());
    (void)context;
    (void)state;
    (void)scope;
    (void)environment;
    cpputil::todo();
}
