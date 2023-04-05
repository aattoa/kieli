#pragma once

#include "utl/utilities.hpp"
#include "compiler/compiler.hpp"
#include "representation/ast/ast.hpp"
#include "representation/hir/hir.hpp"
#include "phase/parse/parse.hpp"


namespace compiler {

    struct Desugar_result {
        Compilation_info  compilation_info;
        hir::Node_context node_context;
        utl::Source       source;
        hir::Module       module;
    };

    auto desugar(Parse_result&&) -> Desugar_result;

}