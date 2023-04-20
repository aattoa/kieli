#pragma once

#include "utl/utilities.hpp"
#include "compiler/compiler.hpp"
#include "representation/hir/hir.hpp"
#include "representation/mir/mir.hpp"
#include "phase/desugar/desugar.hpp"


namespace compiler {

    struct Resolve_result {
        Compilation_info     compilation_info;
        mir::Node_arena      node_arena;
        mir::Namespace_arena namespace_arena;
        mir::Module          module;
    };

    auto resolve(Desugar_result&&) -> Resolve_result;

}
