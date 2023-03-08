#pragma once

#include "utl/utilities.hpp"
#include "ast/ast.hpp"
#include "hir/hir.hpp"
#include "parser/parser.hpp"


namespace compiler {

    struct Lower_result {
        hir::Node_context        node_context;
        utl::diagnostics::Builder diagnostics;
        utl::Source               source;
        Program_string_pool&     string_pool;

        hir::Module module;
    };

    auto lower(Parse_result&&) -> Lower_result;

}