#pragma once

#include <liblex/lex.hpp>
#include <libparse/cst.hpp>
#include <libutl/common/utilities.hpp>

namespace kieli {

    struct [[nodiscard]] Parse_result {
        compiler::Compilation_info compilation_info;
        cst::Node_arena            node_arena;
        cst::Module                module;
    };

    auto parse(Lex_result&&) -> Parse_result;

} // namespace kieli
