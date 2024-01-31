#include <libutl/common/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    struct Pattern_desugaring_visitor {
        Context& context;

        auto operator()(cst::pattern::Parenthesized const& parenthesized) -> ast::Pattern::Variant
        {
            return std::visit(
                Pattern_desugaring_visitor { context }, parenthesized.pattern.value->value);
        }

        template <kieli::literal Literal>
        auto operator()(Literal const& literal) -> ast::Pattern::Variant
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
                .field_patterns
                = std::views::transform(tuple.patterns.value.elements, context.deref_desugar())
                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple) -> ast::Pattern::Variant
        {
            return ast::pattern::Tuple {
                .field_patterns
                = std::views::transform(tuple.patterns.elements, context.deref_desugar())
                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Slice const& slice) -> ast::Pattern::Variant
        {
            return ast::pattern::Slice {
                .element_patterns
                = std::views::transform(slice.patterns.value.elements, context.deref_desugar())
                | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::pattern::Constructor const& ctor) -> ast::Pattern::Variant
        {
            return ast::pattern::Constructor {
                .constructor_name = context.desugar(ctor.constructor_name),
                .payload_pattern  = ctor.payload_pattern.transform(context.desugar()),
            };
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& ctor) -> ast::Pattern::Variant
        {
            return ast::pattern::Abbreviated_constructor {
                .constructor_name = ctor.constructor_name,
                .payload_pattern  = ctor.payload_pattern.transform(context.desugar()),
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
