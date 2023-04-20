#pragma once

#include "compiler/compiler.hpp"
#include "phase/reify/reify.hpp"
#include "representation/cir/cir.hpp"
#include "representation/lir/lir.hpp"


namespace compiler {

    struct Lower_result {
        Compilation_info           compilation_info;
        lir::Node_arena            node_arena;
        std::vector<lir::Function> functions;
    };

    auto lower(Reify_result&&) -> Lower_result;

}