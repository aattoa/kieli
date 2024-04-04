#include <libutl/common/utilities.hpp>
#include <libformat/format_internals.hpp>

namespace {
    auto format_constructor_body(
        libformat::State& state, cst::pattern::Constructor_body const& body)
    {
        std::visit(
            utl::Overload {
                [&](cst::pattern::Struct_constructor const& constructor) {
                    state.format(" {{ ");
                    state.format_comma_separated(constructor.fields.value.elements);
                    state.format(" }}");
                },
                [&](cst::pattern::Tuple_constructor const& constructor) {
                    state.format("(");
                    state.format(constructor.pattern.value);
                    state.format(")");
                },
                [](cst::pattern::Unit_constructor const&) {},
            },
            body);
    }

    struct Pattern_format_visitor {
        libformat::State& state;

        auto operator()(kieli::literal auto const& literal)
        {
            state.format("{}", literal);
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

        auto operator()(cst::Wildcard const& wildcard)
        {
            state.format(wildcard);
        }

        auto operator()(cst::pattern::Name const& name)
        {
            state.format_mutability_with_trailing_whitespace(name.mutability);
            state.format("{}", name.name);
        }

        auto operator()(cst::pattern::Alias const& alias)
        {
            state.format(alias.pattern);
            state.format(" as ");
            state.format_mutability_with_trailing_whitespace(alias.mutability);
            state.format("{}", alias.name);
        }

        auto operator()(cst::pattern::Guarded const& guarded)
        {
            state.format(guarded.guarded_pattern);
            state.format(" if ");
            state.format(guarded.guard_expression);
        }

        auto operator()(cst::pattern::Constructor const& constructor)
        {
            state.format(constructor.name);
            format_constructor_body(state, constructor.body);
        }

        auto operator()(cst::pattern::Abbreviated_constructor const& constructor)
        {
            state.format("::{}", constructor.name);
            format_constructor_body(state, constructor.body);
        }

        auto operator()(cst::pattern::Top_level_tuple const& tuple)
        {
            state.format_comma_separated(tuple.patterns.elements);
        }
    };
} // namespace

auto libformat::State::format(cst::Pattern const& pattern) -> void
{
    std::visit(Pattern_format_visitor { *this }, pattern.variant);
}
