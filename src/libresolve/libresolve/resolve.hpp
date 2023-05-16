#pragma once

#include <libutl/common/utilities.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>
#include <libdesugar/desugar.hpp>
#include <libresolve/mir.hpp>


namespace kieli {

    struct Resolve_result {
        compiler::Compilation_info compilation_info;
        mir::Node_arena            node_arena;
        mir::Namespace_arena       namespace_arena;
        hir::Node_arena            hir_node_arena; // FIXME: the HIR has to be kept alive because MIR template parameters store the HIR representation of their default arguments
        mir::Module                module;
    };

    auto resolve(Desugar_result&&) -> Resolve_result;

}
