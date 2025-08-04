#ifndef KIELI_LIBDESUGAR_DESUGAR
#define KIELI_LIBDESUGAR_DESUGAR

#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>

namespace ki::des {

    struct Context {
        cst::Arena const&   cst;
        ast::Arena          ast;
        db::Diagnostic_sink add_diagnostic;
    };

    auto desugar(Context& ctx, cst::Expression const& expression) -> ast::Expression;
    auto desugar(Context& ctx, cst::Pattern const& pattern) -> ast::Pattern;
    auto desugar(Context& ctx, cst::Type const& type) -> ast::Type;
    auto desugar(Context& ctx, cst::Function const& function) -> ast::Function;
    auto desugar(Context& ctx, cst::Struct const& structure) -> ast::Struct;
    auto desugar(Context& ctx, cst::Enum const& enumeration) -> ast::Enum;
    auto desugar(Context& ctx, cst::Alias const& alias) -> ast::Alias;
    auto desugar(Context& ctx, cst::Concept const& concept_) -> ast::Concept;

} // namespace ki::des

#endif // KIELI_LIBDESUGAR_DESUGAR
