#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

using namespace ki;
using namespace ki::fmt;

namespace {
    void format_constructor_body(Context& ctx, cst::patt::Constructor_body const& body)
    {
        auto const visitor = utl::Overload {
            [&](cst::patt::Struct_constructor const& constructor) {
                std::print(ctx.stream, " {{ ");
                format_comma_separated(ctx, constructor.fields.value.elements);
                std::print(ctx.stream, " }}");
            },
            [&](cst::patt::Tuple_constructor const& constructor) {
                std::print(ctx.stream, "(");
                format_comma_separated(ctx, constructor.fields.value.elements);
                std::print(ctx.stream, ")");
            },
            [](cst::patt::Unit_constructor const&) {},
        };
        std::visit(visitor, body);
    }

    struct Pattern_format_visitor {
        Context& ctx;

        void operator()(utl::one_of<db::Integer, db::Floating, db::Boolean> auto const literal)
        {
            std::print(ctx.stream, "{}", literal.value);
        }

        void operator()(db::String const string)
        {
            std::print(ctx.stream, "{:?}", ctx.db.string_pool.get(string.id));
        }

        void operator()(cst::patt::Paren const& paren)
        {
            std::print(ctx.stream, "(");
            format(ctx, paren.pattern.value);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::patt::Tuple const& tuple)
        {
            std::print(ctx.stream, "(");
            format_comma_separated(ctx, tuple.fields.value.elements);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::patt::Slice const& slice)
        {
            std::print(ctx.stream, "[");
            format_comma_separated(ctx, slice.elements.value.elements);
            std::print(ctx.stream, "]");
        }

        void operator()(cst::Wildcard const& wildcard)
        {
            format(ctx, wildcard);
        }

        void operator()(cst::patt::Name const& name)
        {
            format_mutability_with_whitespace(ctx, name.mutability);
            std::print(ctx.stream, "{}", ctx.db.string_pool.get(name.name.id));
        }

        void operator()(cst::patt::Guarded const& guarded)
        {
            format(ctx, guarded.pattern);
            std::print(ctx.stream, " if ");
            format(ctx, guarded.guard);
        }

        void operator()(cst::patt::Constructor const& constructor)
        {
            format(ctx, constructor.path);
            format_constructor_body(ctx, constructor.body);
        }

        void operator()(cst::patt::Top_level_tuple const& tuple)
        {
            format_comma_separated(ctx, tuple.fields.elements);
        }
    };
} // namespace

void ki::fmt::format(Context& ctx, cst::Pattern const& pattern)
{
    std::visit(Pattern_format_visitor { ctx }, pattern.variant);
}
