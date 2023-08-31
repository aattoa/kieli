#pragma once

#include <libphase/phase.hpp>

namespace compiler {

    struct [[nodiscard]] Compilation_result {};

    struct [[nodiscard]] Compile_arguments {
        std::filesystem::path source_directory_path;
        std::string           main_file_name;
    };

    auto compile(Compile_arguments&&) -> Compilation_result;

} // namespace compiler
