#include <libutl/utilities.hpp>
#include <libformat/format_internals.hpp>

using namespace libformat;

namespace {
    void format_constructor_body(State& state, cst::pattern::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](cst::pattern::Struct_constructor const& constructor) {
                format(state, " {{ ");
                format_comma_separated(state, constructor.fields.value.elements);
                format(state, " }}");
            },
            [&](cst::pattern::Tuple_constructor const& constructor) {
                format(state, "(");
                format(state, constructor.pattern.value);
                format(state, ")");
            },
            [](cst::pattern::Unit_constructor const&) {},
        };
        std::visit(visitor, body);
    }

    struct Pattern_format_visitor {
        State& state;

        void operator()(kieli::literal auto const& literal)
        {
            format(state, "{}", literal);
        }

        void operator()(cst::pattern::Paren const& paren)
        {
            format(state, "(");
            format(state, paren.pattern.value);
            format(state, ")");
        }

        void operator()(cst::pattern::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.patterns.value.elements);
            format(state, ")");
        }

        void operator()(cst::pattern::Slice const& slice)
        {
            format(state, "[");
            format_comma_separated(state, slice.patterns.value.elements);
            format(state, "]");
        }

        void operator()(cst::Wildcard const& wildcard)
        {
            format(state, wildcard);
        }

        void operator()(cst::pattern::Name const& name)
        {
            format_mutability_with_whitespace(state, name.mutability);
            format(state, "{}", name.name);
        }

        void operator()(cst::pattern::Guarded const& guarded)
        {
            format(state, guarded.guarded_pattern);
            format(state, " if ");
            format(state, guarded.guard_expression);
        }

        void operator()(cst::pattern::Constructor const& constructor)
        {
            format(state, constructor.path);
            format_constructor_body(state, constructor.body);
        }

        void operator()(cst::pattern::Abbreviated_constructor const& constructor)
        {
            format(state, "::{}", constructor.name);
            format_constructor_body(state, constructor.body);
        }

        void operator()(cst::pattern::Top_level_tuple const& tuple)
        {
            format_comma_separated(state, tuple.patterns.elements);
        }
    };
} // namespace

void libformat::format(State& state, cst::Pattern const& pattern)
{
    std::visit(Pattern_format_visitor { state }, pattern.variant);
}
