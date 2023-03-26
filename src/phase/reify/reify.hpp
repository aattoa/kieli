#pragma once

#include "representation/mir/mir.hpp"
#include "representation/cir/cir.hpp"
#include "phase/resolve/resolve.hpp"


namespace compiler {

    struct Reify_result {
        cir::Program      program;
        cir::Node_context node_context;
    };

    auto reify(compiler::Resolve_result&&) -> Reify_result;

}