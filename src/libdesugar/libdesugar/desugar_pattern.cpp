#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    struct Pattern_desugaring_visitor {
        Context context;

        auto operator()(cst ::pattern ::Parenthesized const& parenthesized) const
            -> ast ::Pattern_variant
        {
            return std::visit(
                Pattern_desugaring_visitor { context },
                context.cst.patterns[parenthesized.pattern.value].variant);
        }

        auto operator()(kieli ::literal auto const& literal) const -> ast ::Pattern_variant
        {
            return literal;
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Pattern_variant
        {
            return desugar(context, wildcard);
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
            return ast::pattern::Tuple {
                .field_patterns = tuple.patterns.value.elements
                                | std::views::transform(deref_desugar(context))
                                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple) const -> ast::Pattern_variant
        {
            return ast::pattern::Tuple {
                .field_patterns = tuple.patterns.elements
                                | std::views::transform(deref_desugar(context))
                                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Slice const& slice) const -> ast::Pattern_variant
        {
            return ast::pattern::Slice {
                .element_patterns = slice.patterns.value.elements
                                  | std::views::transform(deref_desugar(context))
                                  | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Constructor const& constructor) const -> ast::Pattern_variant
        {
            return ast::pattern::Constructor {
                .path = desugar(context, constructor.path),
                .body = desugar(context, constructor.body),
            };
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& constructor) const
            -> ast::Pattern_variant
        {
            return ast::pattern::Abbreviated_constructor {
                .name = constructor.name,
                .body = desugar(context, constructor.body),
            };
        }

        auto operator()(cst::pattern::Alias const& alias) const -> ast::Pattern_variant
        {
            return ast::pattern::Alias {
                .name       = alias.name,
                .mutability = desugar_mutability(alias.mutability, alias.name.range),
                .pattern    = desugar(context, alias.pattern),
            };
        }

        auto operator()(cst::pattern::Guarded const& guarded) const -> ast::Pattern_variant
        {
            return ast::pattern::Guarded {
                .guarded_pattern  = desugar(context, guarded.guarded_pattern),
                .guard_expression = deref_desugar(context, guarded.guard_expression),
            };
        }
    };
} // namespace

auto libdesugar::desugar(Context const context, cst::Pattern const& pattern) -> ast::Pattern
{
    return { std::visit(Pattern_desugaring_visitor { context }, pattern.variant), pattern.range };
}
