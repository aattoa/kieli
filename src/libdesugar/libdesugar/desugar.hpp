#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

namespace kieli {
    auto desugar(cst::Module const&, Compile_info&) -> ast::Module;
}
