#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

namespace kieli {
    struct Desugar_context {
        Database&         db;
        ast::Arena&       ast;
        cst::Arena const& cst;
        Document_id       document_id;
    };

    auto desugar_definition(Desugar_context context, cst::Definition const& definition)
        -> ast::Definition;
} // namespace kieli
