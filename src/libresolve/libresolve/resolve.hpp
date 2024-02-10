#pragma once

#include <libutl/common/utilities.hpp>

namespace kieli {

    struct Project_configuration {
        std::filesystem::path root_directory;
        std::string           main_name      = "main";
        std::string           file_extension = ".kieli";
    };

    struct Resolved_project {};

    auto resolve_project(Project_configuration project_configuration) -> Resolved_project;

} // namespace kieli
