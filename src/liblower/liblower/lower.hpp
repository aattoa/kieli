#pragma once

#include <compiler/compiler.hpp>
#include <libreify/reify.hpp>
#include <liblower/lir.hpp>


namespace kieli {

    struct [[nodiscard]] Lower_result {
        compiler::Compilation_info compilation_info;
        lir::Node_arena            node_arena;
        std::vector<lir::Function> functions;
    };

    auto lower(Reify_result&&) -> Lower_result;

}
