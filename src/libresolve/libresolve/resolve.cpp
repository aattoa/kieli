#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    auto main_path(kieli::Project_configuration const& config) -> std::filesystem::path
    {
        return config.root_directory / std::format("{}{}", config.main_name, config.file_extension);
    }

    auto make_main_environment(libresolve::Context& context) -> kieli::hir::Environment_id
    {
        std::filesystem::path path = main_path(context.configuration);
        if (auto const document_id = kieli::read_document(context.db, std::move(path))) {
            return libresolve::make_environment(context, document_id.value());
        }
        // TODO: figure out how to properly to handle this
        throw std::runtime_error(std::format("Project main file not found: '{}'", path.c_str()));
    }
} // namespace

auto kieli::resolve_project(Project_configuration configuration) -> Resolved_project
{
    auto db        = Database {};
    auto hir       = hir::Arena {};
    auto constants = libresolve::Constants::make_with(hir);

    libresolve::Context context {
        .db            = db,
        .hir           = std::move(hir),
        .constants     = std::move(constants),
        .configuration = std::move(configuration),
    };

    auto const main = make_main_environment(context);
    libresolve::resolve_definitions_in_order(context, main);
    libresolve::resolve_function_bodies(context, main);

    return Resolved_project {};
}
