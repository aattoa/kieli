#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

namespace kieli {
    struct Desugar_context {
        Database&         db;
        cst::Arena const& cst;
        ast::Arena&       ast;
        Document_id       document_id;
    };

    auto desugar_definition(Desugar_context context, cst::Definition const& definition)
        -> ast::Definition;

    auto desugar_function(Desugar_context context, cst::definition::Function const& function)
        -> ast::definition::Function;

    auto desugar_struct(Desugar_context context, cst::definition::Struct const& structure)
        -> ast::definition::Enumeration;

    auto desugar_enum(Desugar_context context, cst::definition::Enum const& enumeration)
        -> ast::definition::Enumeration;

    auto desugar_alias(Desugar_context context, cst::definition::Alias const& alias)
        -> ast::definition::Alias;

    auto desugar_concept(Desugar_context context, cst::definition::Concept const& concept_)
        -> ast::definition::Concept;

    auto desugar_impl(Desugar_context context, cst::definition::Impl const& impl)
        -> ast::definition::Impl;
} // namespace kieli
