#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>
#include <libparse/parse.hpp>
#include <libdesugar/desugar.hpp>

namespace {
    [[noreturn]] auto report_import_error(
        kieli::Diagnostics&             diagnostics,
        utl::Source::Wrapper const      source,
        libresolve::Import_error const& error) -> void
    {
        diagnostics.error(
            source,
            error.erroneous_segment.source_range,
            "No {} '{}' exists",
            error.expected_module ? "module" : "directory",
            error.erroneous_segment);
    }

    auto make_import_info(
        libresolve::Context&                  context,
        utl::Source::Wrapper const            source,
        libresolve::Environment_wrapper const environment,
        libresolve::Import&&                  import) -> libresolve::Lower_info
    {
        auto const name = import.name;
        return libresolve::Lower_info {
            name,
            source,
            context.arenas.info_arena.wrap<libresolve::Module_info, utl::Wrapper_mutability::yes>(
                std::move(import), environment, name),
        };
    }

    auto collect_import_info(
        libresolve::Context&                  context,
        utl::Source::Wrapper const            source,
        libresolve::Environment_wrapper const environment,
        libresolve::Import&&                  import) -> void
    {
        if (auto const* const existing = environment->lower_map.find(import.name.identifier)) {
            libresolve::report_duplicate_definitions_error(
                context.compile_info.diagnostics,
                source,
                import.name.as_dynamic(),
                existing->name.as_dynamic());
        }
        environment.as_mutable().lower_map.add_new_unchecked(
            import.name.identifier,
            make_import_info(context, source, environment, std::move(import)));
    }

    auto collect_import(
        libresolve::Context&                  context,
        utl::Source::Wrapper const            source,
        libresolve::Environment_wrapper const environment,
        cst::Module::Import const&            import) -> void
    {
        auto resolved_import
            = libresolve::resolve_import(context.project_root_directory, import.segments.elements);
        if (resolved_import.has_value()) {
            collect_import_info(context, source, environment, std::move(resolved_import.value()));
        }
        else {
            report_import_error(context.compile_info.diagnostics, source, resolved_import.error());
        }
    }

    auto import_environment(libresolve::Context& context, libresolve::Import&& import)
        -> utl::Mutable_wrapper<libresolve::Environment>
    {
        cpputil::always_assert(exists(import.module_path));
        cpputil::always_assert(last_write_time(import.module_path) == import.last_write_time);

        utl::Source::Wrapper const source = context.compile_info.source_arena.wrap(
            utl::Source::read(std::move(import.module_path)).value());

        cst::Module const cst = kieli::parse(source, context.compile_info);
        ast::Module       ast = kieli::desugar(cst, context.compile_info);

        context.arenas.ast_node_arena.merge_with(std::move(ast.node_arena));

        auto const environment
            = libresolve::collect_environment(context, std::move(ast.definitions));
        std::ranges::for_each(
            cst.imports, std::bind_front(collect_import, std::ref(context), source, environment));
        return environment;
    }

    auto resolve_submodule(libresolve::Context& context, ast::definition::Submodule&& submodule)
        -> utl::Mutable_wrapper<libresolve::Environment>
    {
        if (submodule.template_parameters.has_value()) {
            cpputil::todo();
        }
        return libresolve::collect_environment(context, std::move(submodule.definitions));
    }
} // namespace

auto libresolve::resolve_module(Context& context, Module_info& module_info)
    -> utl::Mutable_wrapper<Environment>
{
    if (auto* const submodule = std::get_if<ast::definition::Submodule>(&module_info.variant)) {
        module_info.variant = hir::Module { resolve_submodule(context, std::move(*submodule)) };
    }
    else if (auto* const import = std::get_if<libresolve::Import>(&module_info.variant)) {
        module_info.variant = hir::Module { import_environment(context, std::move(*import)) };
    }
    return std::get<hir::Module>(module_info.variant).environment;
}

auto libresolve::resolve_import(
    std::filesystem::path const&             project_root_directory,
    std::span<kieli::Name_lower const> const path_segments) -> std::expected<Import, Import_error>
{
    cpputil::always_assert(!path_segments.empty());
    auto const middle_segments = path_segments.subspan(0, path_segments.size() - 1);
    auto const module_segment  = path_segments.back();

    auto path = project_root_directory;

    for (kieli::Name_lower const& segment : middle_segments) {
        path /= segment.identifier.string.view();
        if (!is_directory(path)) {
            return std::unexpected(libresolve::Import_error {
                .erroneous_segment = segment,
                .expected_module   = false,
            });
        }
    }

    path /= std::format("{}.kieli", module_segment);
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
