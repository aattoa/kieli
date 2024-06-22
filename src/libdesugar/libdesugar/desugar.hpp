#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libparse/cst.hpp>
#include <libdesugar/ast.hpp>

namespace kieli {
    auto desugar(cst::Module const&, Compile_info&) -> ast::Module;
}
