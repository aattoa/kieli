#pragma once

#include "utl/utilities.hpp"
#include "compiler/compiler.hpp"
#include "representation/hir/hir.hpp"
#include "representation/mir/mir.hpp"
#include "phase/desugar/desugar.hpp"


namespace compiler {

    struct Resolve_result {
        compiler::Compilation_info compilation_info;
        utl::Source                source;
        mir::Node_context          node_context;
        mir::Namespace_context     namespace_context;
        mir::Module                module;
    };

    auto resolve(Desugar_result&&) -> Resolve_result;

}
