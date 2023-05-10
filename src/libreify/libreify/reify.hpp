#pragma once

#include <compiler/compiler.hpp>
#include <libresolve/resolve.hpp>
#include <libreify/cir.hpp>


namespace compiler {

    struct Reify_result {
        Compilation_info           compilation_info;
        cir::Node_arena            node_arena;
        std::vector<cir::Function> functions;
    };

    auto reify(compiler::Resolve_result&&) -> Reify_result;

}
