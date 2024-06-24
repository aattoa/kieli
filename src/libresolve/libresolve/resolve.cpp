#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    auto main_path(kieli::Project_configuration const& config) -> std::filesystem::path
    {
        return config.root_directory / std::format("{}{}", config.main_name, config.file_extension);
    }

    auto make_main_environment(libresolve::Context& context) -> libresolve::Environment_wrapper
    {
        auto path = main_path(context.configuration);
        if (auto const id = kieli::read_source(std::move(path), context.compile_info.source_vector))
        {
            return libresolve::make_environment(context, id.value());
        }
        // TODO: figure out how to properly to handle this
        throw std::runtime_error(std::format("Project main file not found: '{}'", path.c_str()));
    }
} // namespace

auto kieli::resolve_project(Project_configuration configuration) -> Resolved_project
{
    Compile_info compile_info;

    auto arenas    = libresolve::Arenas::defaults();
    auto constants = libresolve::Constants::make_with(arenas);

    libresolve::Context context {
        .arenas        = std::move(arenas),
        .constants     = std::move(constants),
        .configuration = std::move(configuration),
        .compile_info  = compile_info,
    };

    auto const main = make_main_environment(context);
    libresolve::resolve_definitions_in_order(context, main);
    libresolve::resolve_function_bodies(context, main);

    return Resolved_project {};
}
