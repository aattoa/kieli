#pragma once

#include "utl/utilities.hpp"
#include "representation/ast/ast.hpp"
#include "representation/hir/hir.hpp"
#include "phase/parse/parse.hpp"


namespace compiler {

    struct Desugar_result {
        hir::Node_context        node_context;
        utl::diagnostics::Builder diagnostics;
        utl::Source               source;
        Program_string_pool&     string_pool;

        hir::Module module;
    };

    auto desugar(Parse_result&&) -> Desugar_result;

}