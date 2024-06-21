#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>

namespace {
    [[noreturn]] auto report_import_error(
        kieli::Compile_info&            info,
        utl::Source_id const            source,
        libresolve::Import_error const& error) -> void
    {
        auto const message = std::format(
            "No {} `{}` exists",
            error.expected_module ? "module" : "directory",
            error.erroneous_segment);
        kieli::fatal_error(info, source, error.erroneous_segment.source_range, message);
    }

    auto collect_import_info(
        libresolve::Context&                  context,
        utl::Source_id const                  source,
        libresolve::Environment_wrapper const environment,
        libresolve::Import&&                  import) -> void
    {
        libresolve::add_to_environment(
            context,
            source,
            environment.as_mutable(),
            import.name,
            context.arenas.info_arena.wrap_mutable<libresolve::Module_info>(
                std::move(import), environment, import.name));
    }

    auto collect_import(
        libresolve::Context&                  context,
        utl::Source_id const                  source,
        libresolve::Environment_wrapper const environment,
        cst::Module::Import const&            import) -> void
    {
        auto resolved_import
            = libresolve::resolve_import(context.project_configuration, import.segments.elements);
        if (resolved_import.has_value()) {
            collect_import_info(context, source, environment, std::move(resolved_import.value()));
        }
        else {
            report_import_error(context.compile_info, source, resolved_import.error());
        }
    }

    auto read_import_source(libresolve::Import const& import) -> std::string
    {
        cpputil::always_assert(exists(import.module_path));
        cpputil::always_assert(last_write_time(import.module_path) == import.last_write_time);
        return utl::read_file(import.module_path).value(); // TODO
    }

    auto import_environment(libresolve::Context& context, libresolve::Import&& import)
        -> libresolve::Environment_wrapper
    {
        return libresolve::make_environment(
            context, context.compile_info.source_vector.push(read_import_source(import)));
    }

    auto resolve_submodule(
        libresolve::Context&         context,
        utl::Source_id const         source,
        ast::definition::Submodule&& submodule) -> libresolve::Environment_wrapper
    {
        if (submodule.template_parameters.has_value()) {
            cpputil::todo();
        }
        return libresolve::collect_environment(context, source, std::move(submodule.definitions));
    }
} // namespace

auto libresolve::make_environment(libresolve::Context& context, utl::Source_id const source)
    -> Environment_wrapper
{
    cst::Module const cst = kieli::parse(source, context.compile_info);
    ast::Module       ast = kieli::desugar(cst, context.compile_info);
    context.arenas.ast_node_arena.merge_with(std::move(ast.node_arena));

    auto const environment = collect_environment(context, source, std::move(ast.definitions));
    std::ranges::for_each(
        cst.imports, std::bind_front(collect_import, std::ref(context), source, environment));
    return environment;
}

auto libresolve::resolve_module(Context& context, Module_info& module_info) -> Environment_wrapper
{
    if (auto* const submodule = std::get_if<ast::definition::Submodule>(&module_info.variant)) {
        module_info.variant = hir::Module { resolve_submodule(
            context, module_info.environment->source, std::move(*submodule)) };
    }
    else if (auto* const import = std::get_if<libresolve::Import>(&module_info.variant)) {
        module_info.variant = hir::Module { import_environment(context, std::move(*import)) };
    }
    return std::get<hir::Module>(module_info.variant).environment;
}

auto libresolve::resolve_import(
    kieli::Project_configuration const&      configuration,
    std::span<kieli::Name_lower const> const path_segments) -> std::expected<Import, Import_error>
{
    cpputil::always_assert(!path_segments.empty());
    auto const middle_segments = path_segments.subspan(0, path_segments.size() - 1);
    auto const module_segment  = path_segments.back();

    auto path = configuration.root_directory;

    for (kieli::Name_lower const& segment : middle_segments) {
        path /= segment.identifier.string.view();
        if (!is_directory(path)) {
            return std::unexpected(libresolve::Import_error {
                .erroneous_segment = segment,
                .expected_module   = false,
            });
        }
    }

    path /= std::format("{}{}", module_segment, configuration.file_extension);

    if (!is_regular_file(path)) {
        return std::unexpected(libresolve::Import_error {
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
