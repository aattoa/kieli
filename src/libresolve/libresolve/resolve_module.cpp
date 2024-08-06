#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

using namespace libresolve;

namespace {
    auto read_import_source(Import&& import, kieli::Source_vector& sources) -> kieli::Source_id
    {
        // TODO
        cpputil::always_assert(exists(import.module_path));
        cpputil::always_assert(last_write_time(import.module_path) == import.last_write_time);
        return kieli::read_source(std::move(import.module_path), sources).value();
    }

    auto import_environment(Context& context, Import&& import) -> hir::Environment_id
    {
        return make_environment(context, read_import_source(std::move(import), context.db.sources));
    }

    auto resolve_submodule(
        Context&                     context,
        kieli::Source_id const       source,
        ast::definition::Submodule&& submodule) -> hir::Environment_id
    {
        if (submodule.template_parameters.has_value()) {
            cpputil::todo();
        }
        return collect_environment(context, source, std::move(submodule.definitions));
    }
} // namespace

auto libresolve::make_environment(Context& context, kieli::Source_id const source)
    -> hir::Environment_id
{
    (void)context;
    (void)source;
    cpputil::todo();
}

auto libresolve::resolve_module(Context& context, Module_info& module_info) -> hir::Environment_id
{
    if (auto* const submodule = std::get_if<ast::definition::Submodule>(&module_info.variant)) {
        auto const source      = context.info.environments[module_info.environment].source;
        auto const environment = resolve_submodule(context, source, std::move(*submodule));
        module_info.variant    = hir::Module { environment };
    }
    else if (auto* const import = std::get_if<Import>(&module_info.variant)) {
        module_info.variant = hir::Module { import_environment(context, std::move(*import)) };
    }
    return std::get<hir::Module>(module_info.variant).environment;
}

auto libresolve::resolve_import(
    kieli::Project_configuration const& configuration,
    std::span<kieli::Lower const> const path_segments) -> std::expected<Import, Import_error>
{
    cpputil::always_assert(!path_segments.empty());
    auto const segments = path_segments.subspan(0, path_segments.size() - 1);
    auto const name     = path_segments.back();

    auto path = configuration.root_directory;

    for (kieli::Lower const& segment : segments) {
        path /= segment.identifier.view();
        if (!is_directory(path)) {
            return std::unexpected(Import_error {
                .erroneous_segment = segment,
                .expected_module   = false,
            });
        }
    }

    path /= std::format("{}{}", name, configuration.file_extension);

    if (!is_regular_file(path)) {
        return std::unexpected(Import_error {
            .erroneous_segment = name,
            .expected_module   = true,
        });
    }

    return Import {
        .last_write_time = last_write_time(path),
        .module_path     = std::move(path),
        .name            = name,
    };
};
