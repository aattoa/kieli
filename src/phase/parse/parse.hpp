#pragma once

#include "utl/utilities.hpp"
#include "phase/lex/lex.hpp"
#include "compiler/compiler.hpp"
#include "representation/ast/ast.hpp"


namespace compiler {

    struct Parse_result {
        Compilation_info compilation_info;
        ast::Node_arena  node_arena;
        utl::Source      source;
        ast::Module      module;
    };

    [[nodiscard]]
    auto parse(Lex_result&&) -> Parse_result;

}