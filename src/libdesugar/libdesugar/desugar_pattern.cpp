#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    struct Pattern_desugaring_visitor {
        Context& context;

        auto operator()(cst::pattern::Parenthesized const& parenthesized) -> ast::Pattern_variant
        {
            return std::visit(
                Pattern_desugaring_visitor { context }, parenthesized.pattern.value->variant);
        }

        auto operator()(kieli::literal auto const& literal) -> ast::Pattern_variant
        {
            return literal;
        }

        auto operator()(cst::Wildcard const& wildcard) -> ast::Pattern_variant
        {
            return context.desugar(wildcard);
        }

        auto operator()(cst::pattern::Name const& name) -> ast::Pattern_variant
        {
            return ast::pattern::Name {
                .name       = name.name,
                .mutability = context.desugar_mutability(name.mutability, name.name.range),
            };
        }

        auto operator()(cst::pattern::Tuple const& tuple) -> ast::Pattern_variant
        {
            return ast::pattern::Tuple {
                .field_patterns = tuple.patterns.value.elements
                                | std::views::transform(context.deref_desugar())
                                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple) -> ast::Pattern_variant
        {
            return ast::pattern::Tuple {
                .field_patterns = tuple.patterns.elements
                                | std::views::transform(context.deref_desugar())
                                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Slice const& slice) -> ast::Pattern_variant
        {
            return ast::pattern::Slice {
                .element_patterns = slice.patterns.value.elements
                                  | std::views::transform(context.deref_desugar())
                                  | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Constructor const& constructor) -> ast::Pattern_variant
        {
            return ast::pattern::Constructor {
                .path = context.desugar(constructor.path),
                .body = context.desugar(constructor.body),
            };
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& constructor)
            -> ast::Pattern_variant
        {
            return ast::pattern::Abbreviated_constructor {
                .name = constructor.name,
                .body = context.desugar(constructor.body),
            };
        }

        auto operator()(cst::pattern::Alias const& alias) -> ast::Pattern_variant
        {
            return ast::pattern::Alias {
                .name       = alias.name,
                .mutability = context.desugar_mutability(alias.mutability, alias.name.range),
                .pattern    = context.desugar(alias.pattern),
            };
        }

        auto operator()(cst::pattern::Guarded const& guarded) -> ast::Pattern_variant
        {
            return ast::pattern::Guarded {
                .guarded_pattern  = context.desugar(guarded.guarded_pattern),
                .guard_expression = context.desugar(*guarded.guard_expression),
            };
        }
    };
} // namespace

auto libdesugar::Context::desugar(cst::Pattern const& pattern) -> ast::Pattern
{
    return {
        std::visit(Pattern_desugaring_visitor { *this }, pattern.variant),
        pattern.range,
    };
}
