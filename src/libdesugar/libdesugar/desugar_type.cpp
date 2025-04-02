#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>

using namespace ki::desugar;

namespace {
    struct Type_desugaring_visitor {
        Context          ctx;
        cst::Type const& this_type;

        auto operator()(cst::type::Paren const& paren) const -> ast::Type_variant
        {
            cst::Type const&        type = ctx.cst.type[paren.type.value];
            Type_desugaring_visitor visitor { .ctx = ctx, .this_type = type };
            return std::visit(visitor, type.variant);
        }

        auto operator()(cst::Path const& path) const -> ast::Type_variant
        {
            return desugar(ctx, path);
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Type_variant
        {
            return desugar(ctx, wildcard);
        }

        auto operator()(cst::type::Never const&) const -> ast::Type_variant
        {
            return ast::type::Never {};
        }

        auto operator()(cst::type::Tuple const& tuple) const -> ast::Type_variant
        {
            return ast::type::Tuple { std::ranges::to<std::vector>(
                std::views::transform(tuple.field_types.value.elements, deref_desugar(ctx))) };
        }

        auto operator()(cst::type::Array const& array) const -> ast::Type_variant
        {
            return ast::type::Array {
                .element_type = desugar(ctx, array.element_type),
                .length       = desugar(ctx, array.length),
            };
        }

        auto operator()(cst::type::Slice const& slice) const -> ast::Type_variant
        {
            return ast::type::Slice { desugar(ctx, slice.element_type.value) };
        }

        auto operator()(cst::type::Function const& function) const -> ast::Type_variant
        {
            return ast::type::Function {
                .parameter_types = function.parameter_types.value.elements
                                 | std::views::transform(deref_desugar(ctx))
                                 | std::ranges::to<std::vector>(),
                .return_type = desugar(ctx, function.return_type),
            };
        }

        auto operator()(cst::type::Typeof const& typeof_) const -> ast::Type_variant
        {
            return ast::type::Typeof { desugar(ctx, typeof_.expression.value) };
        }

        auto operator()(cst::type::Reference const& reference) const -> ast::Type_variant
        {
            return ast::type::Reference {
                .referenced_type = desugar(ctx, reference.referenced_type),
                .mutability      = desugar_mutability(
                    reference.mutability, ctx.cst.range[reference.ampersand_token]),
            };
        }

        auto operator()(cst::type::Pointer const& pointer) const -> ast::Type_variant
        {
            return ast::type::Pointer {
                .pointee_type = desugar(ctx, pointer.pointee_type),
                .mutability
                = desugar_mutability(pointer.mutability, ctx.cst.range[pointer.asterisk_token]),
            };
        }

        auto operator()(cst::type::Impl const& implementation) const -> ast::Type_variant
        {
            return ast::type::Impl { desugar(ctx, implementation.concepts.elements) };
        }
    };
} // namespace

auto ki::desugar::desugar(Context const ctx, cst::Type const& type) -> ast::Type
{
    Type_desugaring_visitor visitor { .ctx = ctx, .this_type = type };
    return { .variant = std::visit(visitor, type.variant), .range = type.range };
}
