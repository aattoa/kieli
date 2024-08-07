#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli {
    auto desugar(Database& db, CST const& cst) -> AST;
}
