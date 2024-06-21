#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    // TODO: rewrite this function
    auto read_main_source(kieli::Project_configuration const& config) -> std::string
    {
        std::filesystem::path main_file_path
            = config.root_directory / std::format("{}{}", config.main_name, config.file_extension);
        if (!is_regular_file(main_file_path)) {
            throw std::runtime_error(
                std::format("Project main file not found: '{}'", main_file_path.c_str()));
        }
        return utl::read_file(main_file_path).value();
    }

    auto make_main_environment(libresolve::Context& context) -> libresolve::Environment_wrapper
    {
        utl::Source_id const source = context.compile_info.source_vector.push(
            read_main_source(context.project_configuration));
        return libresolve::make_environment(context, source);
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

    auto const main_environment = make_main_environment(context);
    libresolve::resolve_definitions_in_order(context, main_environment);
    libresolve::resolve_function_bodies(context, main_environment);

    return Resolved_project {};
}
