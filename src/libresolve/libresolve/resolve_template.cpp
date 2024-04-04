#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

auto libresolve::resolve_template_parameters(
    Context&                        context,
    Scope&                          scope,
    ast::Template_parameters const& template_parameters) -> std::vector<hir::Template_parameter>
{
    (void)context;
    (void)scope;
    (void)template_parameters;
    return {}; // TODO
}

auto libresolve::resolve_template_arguments(
    Context&                                    context,
    std::vector<hir::Template_parameter> const& parameters,
    std::vector<ast::Template_argument> const&  arguments) -> std::vector<hir::Template_argument>
{
    (void)context;
    (void)parameters;
    (void)arguments;
    cpputil::todo();
}
