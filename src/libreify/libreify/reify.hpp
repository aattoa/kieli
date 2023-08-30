#pragma once

#include <libcompiler-pipeline/compiler-pipeline.hpp>
#include <libresolve/resolve.hpp>
#include <libreify/cir.hpp>

namespace kieli {

    struct [[nodiscard]] Reify_result {
        compiler::Compilation_info compilation_info;
        cir::Node_arena            node_arena;
        std::vector<cir::Function> functions;
    };

    auto reify(Resolve_result&&) -> Reify_result;

} // namespace kieli
