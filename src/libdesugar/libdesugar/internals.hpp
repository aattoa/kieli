#ifndef KIELI_LIBDESUGAR_INTERNALS
#define KIELI_LIBDESUGAR_INTERNALS

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/cst/cst.hpp>
#include <libcompiler/ast/ast.hpp>
#include <libdesugar/desugar.hpp>

namespace ki::des {

    auto desugar(Context&, cst::Function_signature const&) -> ast::Function_signature;
    auto desugar(Context&, cst::Function_parameters const&) -> std::vector<ast::Function_parameter>;
    auto desugar(Context&, cst::Template_argument const&) -> ast::Template_argument;
    auto desugar(Context&, cst::Template_parameter const&) -> ast::Template_parameter;
    auto desugar(Context&, cst::Mutability const&) -> ast::Mutability;
    auto desugar(Context&, cst::Path_segment const&) -> ast::Path_segment;
    auto desugar(Context&, cst::Path const&) -> ast::Path;
    auto desugar(Context&, cst::Type_annotation const&) -> ast::Type_id;
    auto desugar(Context&, cst::Type_signature const&) -> ast::Type_signature;
    auto desugar(Context&, cst::Field_init const&) -> ast::Field_init;
    auto desugar(Context&, cst::patt::Field const&) -> ast::patt::Field;
    auto desugar(Context&, cst::patt::Constructor_body const&) -> ast::patt::Constructor_body;
    auto desugar(Context&, cst::Field const&) -> ast::Field;
    auto desugar(Context&, cst::Constructor_body const&) -> ast::Constructor_body;
    auto desugar(Context&, cst::Constructor const&) -> ast::Constructor;
    auto desugar(Context&, cst::Wildcard const&) -> ast::Wildcard;
    auto desugar(Context&, cst::Type_parameter_default_argument const&)
        -> ast::Template_type_parameter::Default;
    auto desugar(Context&, cst::Value_parameter_default_argument const&)
        -> ast::Template_value_parameter::Default;
    auto desugar(Context&, cst::Mutability_parameter_default_argument const&)
        -> ast::Template_mutability_parameter::Default;
    auto desugar(Context&, cst::Expression_id) -> ast::Expression_id;
    auto desugar(Context&, cst::Pattern_id) -> ast::Pattern_id;
    auto desugar(Context&, cst::Type_id) -> ast::Type_id;

    auto desugar_opt_mut(Context&, std::optional<cst::Mutability> const&, lsp::Range)
        -> ast::Mutability;

    template <typename T>
    auto desugar(Context&, std::vector<T> const&);
    template <typename T>
    auto desugar(Context&, cst::Separated<T> const&);
    template <typename T>
    auto desugar(Context&, cst::Surrounded<T> const&);

    inline auto desugar(Context& ctx) noexcept
    {
        return [&](auto const& sugared) { return desugar(ctx, sugared); };
    }

    template <typename T>
    auto desugar(Context& ctx, std::vector<T> const& vector)
    {
        return std::ranges::to<std::vector>(std::views::transform(vector, desugar(ctx)));
    }

    template <typename T>
    auto desugar(Context& ctx, cst::Separated<T> const& sequence)
    {
        return desugar(ctx, sequence.elements);
    }

    template <typename T>
    auto desugar(Context& ctx, cst::Surrounded<T> const& surrounded)
    {
        return desugar(ctx, surrounded.value);
    }

    auto wrap_desugar(Context&, cst::Expression const&) -> ast::Expression_id;
    auto wrap_desugar(Context&, cst::Pattern const&) -> ast::Pattern_id;
    auto wrap_desugar(Context&, cst::Type const&) -> ast::Type_id;

    auto deref_desugar(Context&, cst::Expression_id) -> ast::Expression;
    auto deref_desugar(Context&, cst::Pattern_id) -> ast::Pattern;
    auto deref_desugar(Context&, cst::Type_id) -> ast::Type;

    inline auto wrap_desugar(Context& ctx) noexcept
    {
        return [&](auto const& sugar) { return wrap_desugar(ctx, sugar); };
    }

    inline auto deref_desugar(Context& ctx) noexcept
    {
        return [&](auto const& sugar) { return deref_desugar(ctx, sugar); };
    }

    auto unit_type(lsp::Range) -> ast::Type;
    auto wildcard_type(lsp::Range) -> ast::Type;
    auto unit_value(lsp::Range) -> ast::Expression;
    auto wildcard_pattern(lsp::Range) -> ast::Pattern;

} // namespace ki::des

#endif // KIELI_LIBDESUGAR_INTERNALS
