#include <libutl/utilities.hpp>
#include <libformat/internals.hpp>

using namespace ki;
using namespace ki::fmt;

namespace {
    struct Type_format_visitor {
        Context& ctx;

        void operator()(cst::Wildcard const& wildcard)
        {
            format(ctx, wildcard);
        }

        void operator()(cst::Path const& path)
        {
            format(ctx, path);
        }

        void operator()(cst::type::Never const&)
        {
            std::print(ctx.stream, "!");
        }

        void operator()(cst::type::Paren const& paren)
        {
            std::print(ctx.stream, "(");
            format(ctx, paren.type.value);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::type::Tuple const& tuple)
        {
            std::print(ctx.stream, "(");
            format_comma_separated(ctx, tuple.field_types.value.elements);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::type::Reference const& reference)
        {
            std::print(ctx.stream, "&");
            format_mutability_with_whitespace(ctx, reference.mutability);
            format(ctx, reference.referenced_type);
        }

        void operator()(cst::type::Pointer const& pointer)
        {
            std::print(ctx.stream, "*");
            format_mutability_with_whitespace(ctx, pointer.mutability);
            format(ctx, pointer.pointee_type);
        }

        void operator()(cst::type::Function const& function)
        {
            std::print(ctx.stream, "fn(");
            format_comma_separated(ctx, function.parameter_types.value.elements);
            std::print(ctx.stream, ")");
            format(ctx, function.return_type);
        }

        void operator()(cst::type::Array const& array)
        {
            std::print(ctx.stream, "[");
            format(ctx, array.element_type);
            std::print(ctx.stream, "; ");
            format(ctx, array.length);
            std::print(ctx.stream, "]");
        }

        void operator()(cst::type::Slice const& slice)
        {
            std::print(ctx.stream, "[");
            format(ctx, slice.element_type.value);
            std::print(ctx.stream, "]");
        }

        void operator()(cst::type::Typeof const& typeof_)
        {
            std::print(ctx.stream, "typeof(");
            format(ctx, typeof_.expression.value);
            std::print(ctx.stream, ")");
        }

        void operator()(cst::type::Impl const& implementation)
        {
            std::print(ctx.stream, "impl ");
            format_separated(ctx, implementation.concepts.elements, " + ");
        }
    };
} // namespace

void ki::fmt::format(Context& ctx, cst::Type const& type)
{
    std::visit(Type_format_visitor { ctx }, type.variant);
}
