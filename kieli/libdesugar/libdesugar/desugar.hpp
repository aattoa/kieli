#pragma once

#include <libutl/common/utilities.hpp>
#include <compiler/compiler.hpp>
#include <libdesugar/hir.hpp>
#include <libparse/parse.hpp>


namespace compiler {

    struct Desugar_result {
        Compilation_info  compilation_info;
        hir::Node_arena   node_arena;
        hir::Module       module;
    };

    auto desugar(Parse_result&&) -> Desugar_result;

}
