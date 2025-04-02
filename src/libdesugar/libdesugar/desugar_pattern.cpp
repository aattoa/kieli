#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>

using namespace ki::desugar;

namespace {
    struct Pattern_desugaring_visitor {
        Context ctx;

        auto operator()(cst::pattern::Paren const& paren) const -> ast::Pattern_variant
        {
            return std::visit(
                Pattern_desugaring_visitor { ctx }, ctx.cst.patt[paren.pattern.value].variant);
        }

        auto operator()(utl::one_of<ki::Integer, ki::Floating, ki::Boolean, ki::String> auto const&
                            literal) const -> ast::Pattern_variant
        {
            return literal;
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Pattern_variant
        {
            return desugar(ctx, wildcard);
        }

        auto operator()(cst::pattern::Name const& name) const -> ast::Pattern_variant
        {
            return ast::pattern::Name {
                .name       = name.name,
                .mutability = desugar_mutability(name.mutability, name.name.range),
            };
        }

        auto operator()(cst::pattern::Tuple const& tuple) const -> ast::Pattern_variant
        {
            return ast::pattern::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.patterns.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple) const -> ast::Pattern_variant
        {
            return ast::pattern::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.patterns.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::pattern::Slice const& slice) const -> ast::Pattern_variant
        {
            return ast::pattern::Slice { std::ranges::to<std::vector>(
                std::views::transform(slice.patterns.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::pattern::Constructor const& constructor) const -> ast::Pattern_variant
        {
            return ast::pattern::Constructor {
                .path = desugar(ctx, constructor.path),
                .body = desugar(ctx, constructor.body),
            };
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& constructor) const
            -> ast::Pattern_variant
        {
            return ast::pattern::Abbreviated_constructor {
                .name = constructor.name,
                .body = desugar(ctx, constructor.body),
            };
        }

        auto operator()(cst::pattern::Guarded const& guarded) const -> ast::Pattern_variant
        {
            return ast::pattern::Guarded {
                .guarded_pattern  = desugar(ctx, guarded.guarded_pattern),
                .guard_expression = deref_desugar(ctx, guarded.guard_expression),
            };
        }
    };
} // namespace

auto ki::desugar::desugar(Context const ctx, cst::Pattern const& pattern) -> ast::Pattern
{
    Pattern_desugaring_visitor visitor { ctx };
    return { .variant = std::visit(visitor, pattern.variant), .range = pattern.range };
}
