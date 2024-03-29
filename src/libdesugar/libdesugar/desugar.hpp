#pragma once

#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>
#include <libparse/cst.hpp>
#include <libdesugar/ast.hpp>

namespace kieli {
    auto desugar(cst::Module const&, Compile_info&) -> ast::Module;
}
