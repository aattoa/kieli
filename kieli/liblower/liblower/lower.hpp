#pragma once

#include <compiler/compiler.hpp>
#include <libreify/reify.hpp>
#include <liblower/lir.hpp>


namespace compiler {

    struct Lower_result {
        Compilation_info           compilation_info;
        lir::Node_arena            node_arena;
        std::vector<lir::Function> functions;
    };

    auto lower(Reify_result&&) -> Lower_result;

}
