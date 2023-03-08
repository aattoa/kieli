#pragma once

#include "mir/mir.hpp"
#include "cir/cir.hpp"
#include "resolution/resolution.hpp"


namespace compiler {

    struct Reify_result {
        cir::Program      program;
        cir::Node_context node_context;
    };

    auto reify(compiler::Resolve_result&&) -> Reify_result;

}