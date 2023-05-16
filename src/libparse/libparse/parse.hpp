#pragma once

#include <libutl/common/utilities.hpp>
#include <libcompiler-pipeline/compiler-pipeline.hpp>
#include <liblex/lex.hpp>
#include <libparse/ast.hpp>


namespace kieli {

    struct [[nodiscard]] Parse_result {
        compiler::Compilation_info compilation_info;
        ast::Node_arena            node_arena;
        ast::Module                module;
    };

    auto parse(Lex_result&&) -> Parse_result;

}
