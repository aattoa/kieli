#ifndef KIELI_LIBDESUGAR_DESUGAR
#define KIELI_LIBDESUGAR_DESUGAR

#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>

namespace ki::des {

    struct Context {
        db::Database&     db;
        db::Document_id   doc_id;
        cst::Arena const& cst;
        ast::Arena        ast;
    };

    auto context(db::Database& db, db::Document_id doc_id) -> Context;

    auto desugar_definition(Context& ctx, cst::Definition const& definition) -> ast::Definition;

    auto desugar_function(Context& ctx, cst::Function const& function) -> ast::Function;

    auto desugar_struct(Context& ctx, cst::Struct const& structure) -> ast::Struct;

    auto desugar_enum(Context& ctx, cst::Enum const& enumeration) -> ast::Enum;

    auto desugar_alias(Context& ctx, cst::Alias const& alias) -> ast::Alias;

    auto desugar_concept(Context& ctx, cst::Concept const& concept_) -> ast::Concept;

    auto desugar_impl(Context& ctx, cst::Impl const& impl) -> ast::Impl;

} // namespace ki::des

#endif // KIELI_LIBDESUGAR_DESUGAR
