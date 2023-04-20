#pragma once

#include "compiler/compiler.hpp"
#include "representation/mir/mir.hpp"
#include "representation/cir/cir.hpp"
#include "phase/resolve/resolve.hpp"


namespace compiler {

    struct Reify_result {
        Compilation_info           compilation_info;
        cir::Node_arena            node_arena;
        std::vector<cir::Function> functions;
    };

    auto reify(compiler::Resolve_result&&) -> Reify_result;

}