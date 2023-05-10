#pragma once

#include <libutl/common/utilities.hpp>
#include <compiler/compiler.hpp>
#include <liblex/lex.hpp>
#include <libparse/ast.hpp>


namespace compiler {

    struct Parse_result {
        Compilation_info compilation_info;
        ast::Node_arena  node_arena;
        ast::Module      module;
    };

    [[nodiscard]]
    auto parse(Lex_result&&) -> Parse_result;

}
