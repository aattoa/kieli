#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

using namespace ki;
using namespace ki::fmt;

namespace {
    void format_constructor_body(State& state, cst::patt::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](cst::patt::Struct_constructor const& constructor) {
                format(state, " {{ ");
                format_comma_separated(state, constructor.fields.value.elements);
                format(state, " }}");
            },
            [&](cst::patt::Tuple_constructor const& constructor) {
                format(state, "(");
                format_comma_separated(state, constructor.fields.value.elements);
                format(state, ")");
            },
            [](cst::patt::Unit_constructor const&) {},
        };
        std::visit(visitor, body);
    }

    struct Pattern_format_visitor {
        State& state;

        void operator()(utl::one_of<db::Integer, db::Floating, db::Boolean> auto const literal)
        {
            format(state, "{}", literal.value);
        }

        void operator()(db::String const string)
        {
            format(state, "{:?}", state.ctx.db.string_pool.get(string.id));
        }

        void operator()(cst::patt::Paren const& paren)
        {
            format(state, "(");
            format(state, paren.pattern.value);
            format(state, ")");
        }

        void operator()(cst::patt::Tuple const& tuple)
        {
            format(state, "(");
            format_comma_separated(state, tuple.fields.value.elements);
            format(state, ")");
        }

        void operator()(cst::patt::Slice const& slice)
        {
            format(state, "[");
            format_comma_separated(state, slice.elements.value.elements);
            format(state, "]");
        }

        void operator()(cst::Wildcard const& wildcard)
        {
            format(state, wildcard);
        }

        void operator()(cst::patt::Name const& name)
        {
            format_mutability_with_whitespace(state, name.mutability);
            format(state, "{}", state.ctx.db.string_pool.get(name.name.id));
        }

        void operator()(cst::patt::Guarded const& guarded)
        {
            format(state, guarded.pattern);
            format(state, " if ");
            format(state, guarded.guard);
        }

        void operator()(cst::patt::Constructor const& constructor)
        {
            format(state, constructor.path);
            format_constructor_body(state, constructor.body);
        }

        void operator()(cst::patt::Top_level_tuple const& tuple)
        {
            format_comma_separated(state, tuple.fields.elements);
        }
    };
} // namespace

void ki::fmt::format(State& state, cst::Pattern const& pattern)
{
    std::visit(Pattern_format_visitor { state }, pattern.variant);
}
