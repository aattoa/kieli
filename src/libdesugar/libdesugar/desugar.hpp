#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

namespace kieli {

    auto desugar(
        Database&              db,
        Document_id            document_id,
        ast::Arena&            ast,
        cst::Arena const&      cst,
        cst::Definition const& definition) -> ast::Definition;

} // namespace kieli
