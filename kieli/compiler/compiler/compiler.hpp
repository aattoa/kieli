#pragma once

#include <libcompiler-pipeline/compiler-pipeline.hpp>


namespace compiler {

    struct [[nodiscard]] Compilation_result {};

    auto compile(Compile_arguments&&) -> Compilation_result;

}
