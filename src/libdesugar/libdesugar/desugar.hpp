#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>
#include <libdesugar/ast.hpp>
#include <libparse/parse.hpp>

namespace kieli {

    struct [[nodiscard]] Desugar_result {
        Compilation_info compilation_info;
        ast::Node_arena  node_arena;
        ast::Module      module;
    };

    auto desugar(Parse_result&&) -> Desugar_result;

} // namespace kieli
