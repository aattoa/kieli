#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

using namespace libresolve;

namespace {
    auto resolve_submodule(
        Context&                     context,
        kieli::Document_id const     source,
        ast::definition::Submodule&& submodule) -> hir::Environment_id
    {
        if (submodule.template_parameters.has_value()) {
            cpputil::todo();
        }
        return collect_environment(context, source, std::move(submodule.definitions));
    }
} // namespace

auto libresolve::make_environment(Context& context, kieli::Document_id const document_id)
    -> hir::Environment_id
{
    (void)context;
    (void)document_id;
    cpputil::todo();
}

auto libresolve::resolve_module(Context& context, Module_info& module_info) -> hir::Environment_id
{
    if (auto* const submodule = std::get_if<ast::definition::Submodule>(&module_info.variant)) {
        auto const document_id = context.info.environments[module_info.environment].document_id;
        auto const environment = resolve_submodule(context, document_id, std::move(*submodule));
        module_info.variant    = hir::Module { environment };
    }
    return std::get<hir::Module>(module_info.variant).environment;
}
