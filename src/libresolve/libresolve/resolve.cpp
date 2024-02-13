#include <libutl/common/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    auto read_main_source(
        kieli::Diagnostics&                 diagnostics,
        kieli::Project_configuration const& configuration) -> utl::Source
    {
        std::filesystem::path main_file_path
            = configuration.root_directory
            / std::format("{}{}", configuration.main_name, configuration.file_extension);

        if (!is_regular_file(main_file_path)) {
            diagnostics.error("Project main file not found: '{}'", main_file_path.c_str());
        }
        return utl::Source::read(std::move(main_file_path)).value();
    }

    auto make_main_environment(libresolve::Context& context) -> libresolve::Environment_wrapper
    {
        auto main_source
            = read_main_source(context.compile_info.diagnostics, context.project_configuration);
        return libresolve::make_environment(
            context, context.compile_info.source_arena.wrap(std::move(main_source)));
    }
} // namespace

auto kieli::resolve_project(Project_configuration const& project_configuration) -> Resolved_project
{
    Compile_info compile_info;

    auto arenas    = libresolve::Arenas::defaults();
    auto constants = libresolve::Constants::make_with(arenas);

    libresolve::Context context {
        .arenas                = std::move(arenas),
        .constants             = std::move(constants),
        .project_configuration = project_configuration,
        .compile_info          = compile_info,
    };

    libresolve::resolve_definitions_in_order(context, make_main_environment(context));

    return Resolved_project {};
}
