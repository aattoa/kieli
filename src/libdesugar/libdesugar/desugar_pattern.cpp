#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

namespace {

    using namespace libdesugar;

    struct Pattern_desugaring_visitor {
        Context& context;

        auto operator()(cst::pattern::Parenthesized const& parenthesized) -> ast::Pattern::Variant
        {
            return std::visit(
                Pattern_desugaring_visitor { context }, parenthesized.pattern.value->value);
        }

        auto operator()(kieli::literal auto const& literal) -> ast::Pattern::Variant
        {
            return literal;
        }

        auto operator()(cst::Wildcard const& wildcard) -> ast::Pattern::Variant
        {
            return context.desugar(wildcard);
        }

        auto operator()(cst::pattern::Name const& name) -> ast::Pattern::Variant
        {
            return ast::pattern::Name {
                .name       = name.name,
                .mutability = context.desugar_mutability(name.mutability, name.name.source_range),
            };
        }

        auto operator()(cst::pattern::Tuple const& tuple) -> ast::Pattern::Variant
        {
            return ast::pattern::Tuple {
                .field_patterns = tuple.patterns.value.elements
                                | std::views::transform(context.deref_desugar())
                                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple) -> ast::Pattern::Variant
        {
            return ast::pattern::Tuple {
                .field_patterns = tuple.patterns.elements
                                | std::views::transform(context.deref_desugar())
                                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Slice const& slice) -> ast::Pattern::Variant
        {
            return ast::pattern::Slice {
                .element_patterns = slice.patterns.value.elements
                                  | std::views::transform(context.deref_desugar())
                                  | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Constructor const& constructor) -> ast::Pattern::Variant
        {
            return ast::pattern::Constructor {
                .name = context.desugar(constructor.name),
                .body = context.desugar(constructor.body),
            };
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& constructor)
            -> ast::Pattern::Variant
        {
            return ast::pattern::Abbreviated_constructor {
                .name = constructor.name,
                .body = context.desugar(constructor.body),
            };
        }

        auto operator()(cst::pattern::Alias const& alias) -> ast::Pattern::Variant
        {
            return ast::pattern::Alias {
                .alias_name = alias.alias_name,
                .alias_mutability
                = context.desugar_mutability(alias.alias_mutability, alias.alias_name.source_range),
                .aliased_pattern = context.desugar(alias.aliased_pattern),
            };
        }

        auto operator()(cst::pattern::Guarded const& guarded) -> ast::Pattern::Variant
        {
            return ast::pattern::Guarded {
                .guarded_pattern = context.desugar(guarded.guarded_pattern),
                .guard           = context.desugar(*guarded.guard_expression),
            };
        }
    };

} // namespace

auto libdesugar::Context::desugar(cst::Pattern const& pattern) -> ast::Pattern
{
    return {
        .value        = std::visit(Pattern_desugaring_visitor { *this }, pattern.value),
        .source_range = pattern.source_range,
    };
}
