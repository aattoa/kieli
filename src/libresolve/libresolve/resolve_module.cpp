#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

using namespace libresolve;

namespace {
    [[noreturn]] auto report_import_error(
        kieli::Compile_info& info, kieli::Source_id const source, Import_error const& error) -> void
    {
        auto const message = std::format(
            "No {} `{}` exists",
            error.expected_module ? "module" : "directory",
            error.erroneous_segment);
        kieli::fatal_error(info, source, error.erroneous_segment.range, message);
    }

    auto collect_import_info(
        Context&                  context,
        kieli::Source_id const    source,
        hir::Environment_id const environment,
        Import&&                  import) -> void
    {
        add_to_environment(
            context,
            source,
            context.info.environments[environment],
            import.name,
            context.info.modules.push(std::move(import), environment, source, import.name));
    }

    auto collect_import(
        Context&                  context,
        kieli::Source_id const    source,
        hir::Environment_id const environment,
        cst::Import const&        import) -> void
    {
        auto resolved_import = resolve_import(context.configuration, import.segments.elements);
        if (resolved_import.has_value()) {
            collect_import_info(context, source, environment, std::move(resolved_import.value()));
        }
        else {
            report_import_error(context.compile_info, source, resolved_import.error());
        }
    }

    auto read_import_source(Import&& import, kieli::Source_vector& sources) -> kieli::Source_id
    {
        // TODO
        cpputil::always_assert(exists(import.module_path));
        cpputil::always_assert(last_write_time(import.module_path) == import.last_write_time);
        return kieli::read_source(std::move(import.module_path), sources).value();
    }

    auto import_environment(Context& context, Import&& import) -> hir::Environment_id
    {
        return make_environment(
            context, read_import_source(std::move(import), context.compile_info.sources));
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
    auto const cst = kieli::parse(source, context.compile_info);
    auto       ast = kieli::desugar(cst, context.compile_info);
    context.ast.merge_with(std::move(ast.module->node_arena));

    hir::Environment_id const environment
        = collect_environment(context, source, std::move(ast.module->definitions));

    std::ranges::for_each(
        cst.module->imports,
        std::bind_front(collect_import, std::ref(context), source, environment));

    return environment;
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
    auto const middle_segments = path_segments.subspan(0, path_segments.size() - 1);
    auto const module_segment  = path_segments.back();

    auto path = configuration.root_directory;

    for (kieli::Lower const& segment : middle_segments) {
        path /= segment.identifier.view();
        if (!is_directory(path)) {
            return std::unexpected(Import_error {
                .erroneous_segment = segment,
                .expected_module   = false,
            });
        }
    }

    path /= std::format("{}{}", module_segment, configuration.file_extension);

    if (!is_regular_file(path)) {
        return std::unexpected(Import_error {
            .erroneous_segment = module_segment,
            .expected_module   = true,
        });
    }

    return Import {
        .last_write_time = last_write_time(path),
        .module_path     = std::move(path),
        .name            = module_segment,
    };
};
