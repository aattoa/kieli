#pragma once

#include "utl/utilities.hpp"
#include "lexer/lexer.hpp"
#include "ast/ast.hpp"


namespace compiler {

    struct Parse_result {
        ast::Node_context        node_context;
        utl::diagnostics::Builder diagnostics;
        utl::Source               source;
        Program_string_pool&     string_pool;

        ast::Module module;
    };

    [[nodiscard]]
    auto parse(Lex_result&&) -> Parse_result;

}