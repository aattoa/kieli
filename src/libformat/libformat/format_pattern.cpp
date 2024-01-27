#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>

namespace {
    struct Pattern_format_visitor {
        libformat::State& state;

        template <kieli::literal Literal>
        auto operator()(Literal const& literal)
        {
            if constexpr (std::is_same_v<Literal, kieli::String>) {
                state.format("\"{}\"", literal.value);
            }
            else if constexpr (std::is_same_v<Literal, kieli::Character>) {
                state.format("'{}'", literal.value);
            }
            else {
                state.format("{}", literal.value);
            }
        }

        auto operator()(cst::pattern::Parenthesized const& parenthesized)
        {
            state.format("(");
            state.format(parenthesized.pattern.value);
            state.format(")");
        }

        auto operator()(cst::pattern::Tuple const& tuple)
        {
            state.format("(");
            state.format_comma_separated(tuple.patterns.value.elements);
            state.format(")");
        }

        auto operator()(cst::pattern::Slice const& slice)
        {
            state.format("[");
            state.format_comma_separated(slice.patterns.value.elements);
            state.format("]");
        }

        auto operator()(cst::pattern::Wildcard const&)
        {
            state.format("_");
        }

        auto operator()(cst::pattern::Name const& name)
        {
            state.format_mutability_with_trailing_whitespace(name.mutability);
            state.format("{}", name.name);
        }

        auto operator()(cst::pattern::Alias const& alias)
        {
            state.format(alias.aliased_pattern);
            state.format(" as ");
            state.format_mutability_with_trailing_whitespace(alias.alias_mutability);
            state.format("{}", alias.alias_name);
        }

        auto operator()(cst::pattern::Guarded const& guarded)
        {
            state.format(guarded.guarded_pattern);
            state.format(" if ");
            state.format(guarded.guard_expression);
        }

        auto operator()(cst::pattern::Constructor const& constructor)
        {
            state.format(constructor.constructor_name);
            if (!constructor.payload_pattern.has_value()) {
                return;
            }
            state.format("(");
            state.format(constructor.payload_pattern->value);
            state.format(")");
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& constructor)
        {
            state.format("::{}", constructor.constructor_name);
            if (!constructor.payload_pattern.has_value()) {
                return;
            }
            state.format("(");
            state.format(constructor.payload_pattern->value);
            state.format(")");
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple)
        {
            state.format_comma_separated(tuple.patterns.elements);
        }
    };
} // namespace

auto libformat::State::format(cst::Pattern const& pattern) -> void
{
    std::visit(Pattern_format_visitor { *this }, pattern.value);
}
