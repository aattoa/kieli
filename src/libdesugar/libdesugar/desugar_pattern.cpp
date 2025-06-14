#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>

using namespace ki;
using namespace ki::des;

namespace {
    struct Visitor {
        Context& ctx;

        auto operator()(cst::patt::Paren const& paren) const -> ast::Pattern_variant
        {
            return std::visit(
                Visitor { .ctx = ctx }, ctx.cst.patterns[paren.pattern.value].variant);
        }

        auto operator()(utl::one_of<db::Integer, db::Floating, db::Boolean, db::String> auto const&
                            literal) const -> ast::Pattern_variant
        {
            return literal;
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Pattern_variant
        {
            return desugar(ctx, wildcard);
        }

        auto operator()(cst::patt::Name const& name) const -> ast::Pattern_variant
        {
            return ast::patt::Name {
                .name       = name.name,
                .mutability = desugar_opt_mut(ctx, name.mutability, name.name.range),
            };
        }

        auto operator()(cst::patt::Tuple const& tuple) const -> ast::Pattern_variant
        {
            return ast::patt::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.patterns.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::patt::Top_level_tuple const& tuple) const -> ast::Pattern_variant
        {
            return ast::patt::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.patterns.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::patt::Slice const& slice) const -> ast::Pattern_variant
        {
            return ast::patt::Slice { std::ranges::to<std::vector>(
                std::views::transform(slice.patterns.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::patt::Constructor const& constructor) const -> ast::Pattern_variant
        {
            return ast::patt::Constructor {
                .path = desugar(ctx, constructor.path),
                .body = desugar(ctx, constructor.body),
            };
        }

        auto operator()(cst::patt::Guarded const& guarded) const -> ast::Pattern_variant
        {
            return ast::patt::Guarded {
                .pattern = desugar(ctx, guarded.pattern),
                .guard   = desugar(ctx, guarded.guard),
            };
        }
    };
} // namespace

auto ki::des::desugar(Context& ctx, cst::Pattern const& pattern) -> ast::Pattern
{
    return ast::Pattern {
        .variant = std::visit(Visitor { .ctx = ctx }, pattern.variant),
        .range   = ctx.cst.ranges[pattern.range],
    };
}
