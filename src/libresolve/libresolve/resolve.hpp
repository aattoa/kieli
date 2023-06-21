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
        ast::Node_arena            ast_node_arena; // FIXME: the AST has to be kept alive because MIR template parameters store the AST representation of their default arguments
        std::vector<mir::Function> functions;
    };

    auto resolve(Desugar_result&&) -> Resolve_result;

}
