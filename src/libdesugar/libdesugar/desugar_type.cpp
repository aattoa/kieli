#include <libutl/utilities.hpp>
#include <libdesugar/internals.hpp>

using namespace ki;
using namespace ki::des;

namespace {
    struct Visitor {
        Context& ctx;

        auto operator()(cst::type::Paren const& paren) const -> ast::Type_variant
        {
            return std::visit(*this, ctx.cst.types[paren.type.value].variant);
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
            return ast::type::Tuple { .fields = desugar(ctx, tuple.field_types) };
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
                .parameter_types = desugar(ctx, function.parameter_types),
                .return_type     = desugar(ctx, function.return_type),
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
                .mutability = desugar_opt_mut(ctx, reference.mutability, reference.ampersand_token),
            };
        }

        auto operator()(cst::type::Pointer const& pointer) const -> ast::Type_variant
        {
            return ast::type::Pointer {
                .pointee_type = desugar(ctx, pointer.pointee_type),
                .mutability   = desugar_opt_mut(ctx, pointer.mutability, pointer.asterisk_token),
            };
        }

        auto operator()(cst::type::Impl const& implementation) const -> ast::Type_variant
        {
            return ast::type::Impl { desugar(ctx, implementation.concepts.elements) };
        }
    };
} // namespace

auto ki::des::desugar(Context& ctx, cst::Type const& type) -> ast::Type
{
    return ast::Type {
        .variant = std::visit(Visitor { .ctx = ctx }, type.variant),
        .range   = type.range,
    };
}
