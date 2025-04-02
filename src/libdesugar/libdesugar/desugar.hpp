#ifndef KIELI_LIBDESUGAR_DESUGAR
#define KIELI_LIBDESUGAR_DESUGAR

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>

namespace ki::desugar {

    struct Context {
        Database&         db;
        cst::Arena const& cst;
        ast::Arena&       ast;
        Document_id       doc_id;
    };

    auto desugar_definition(Context ctx, cst::Definition const& definition) -> ast::Definition;

    auto desugar_function(Context ctx, cst::Function const& function) -> ast::Function;

    auto desugar_struct(Context ctx, cst::Struct const& structure) -> ast::Enumeration;

    auto desugar_enum(Context ctx, cst::Enum const& enumeration) -> ast::Enumeration;

    auto desugar_alias(Context ctx, cst::Alias const& alias) -> ast::Alias;

    auto desugar_concept(Context ctx, cst::Concept const& concept_) -> ast::Concept;

    auto desugar_impl(Context ctx, cst::Impl const& impl) -> ast::Impl;

} // namespace ki::desugar

#endif // KIELI_LIBDESUGAR_DESUGAR
