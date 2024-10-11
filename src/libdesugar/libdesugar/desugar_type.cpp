#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    struct Type_desugaring_visitor {
        Context          context;
        cst::Type const& this_type;

        auto operator()(cst::type::Parenthesized const& parenthesized) const -> ast::Type_variant
        {
            cst::Type const& type = context.cst.types[parenthesized.type.value];
            return std::visit(Type_desugaring_visitor { context, type }, type.variant);
        }

        auto operator()(cst::Path const& path) const -> ast::Type_variant
        {
            return desugar(context, path);
        }

        auto operator()(cst::Wildcard const& wildcard) const -> ast::Type_variant
        {
            return desugar(context, wildcard);
        }

        auto operator()(cst::type::Never const&) const -> ast::Type_variant
        {
            return ast::type::Never {};
        }

        auto operator()(cst::type::Tuple const& tuple) const -> ast::Type_variant
        {
            return ast::type::Tuple {
                .field_types = tuple.field_types.value.elements
                             | std::views::transform(deref_desugar(context))
                             | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::type::Array const& array) const -> ast::Type_variant
        {
            return ast::type::Array {
                .element_type = desugar(context, array.element_type),
                .length       = desugar(context, array.length),
            };
        }

        auto operator()(cst::type::Slice const& slice) const -> ast::Type_variant
        {
            return ast::type::Slice { desugar(context, slice.element_type.value) };
        }

        auto operator()(cst::type::Function const& function) const -> ast::Type_variant
        {
            return ast::type::Function {
                .parameter_types = function.parameter_types.value.elements
                                 | std::views::transform(deref_desugar(context))
                                 | std::ranges::to<std::vector>(),
                .return_type = desugar(context, function.return_type),
            };
        }

        auto operator()(cst::type::Typeof const& typeof_) const -> ast::Type_variant
        {
            return ast::type::Typeof { desugar(context, typeof_.inspected_expression.value) };
        }

        auto operator()(cst::type::Reference const& reference) const -> ast::Type_variant
        {
            auto const ampersand_range = context.cst.tokens[reference.ampersand_token].range;
            return ast::type::Reference {
                .referenced_type = desugar(context, reference.referenced_type),
                .mutability      = desugar_mutability(reference.mutability, ampersand_range),
            };
        }

        auto operator()(cst::type::Pointer const& pointer) const -> ast::Type_variant
        {
            auto const asterisk_range = context.cst.tokens[pointer.asterisk_token].range;
            return ast::type::Pointer {
                .pointee_type = desugar(context, pointer.pointee_type),
                .mutability   = desugar_mutability(pointer.mutability, asterisk_range),
            };
        }

        auto operator()(cst::type::Impl const& implementation) const -> ast::Type_variant
        {
            return ast::type::Impl { desugar(context, implementation.concepts.elements) };
        }
    };
} // namespace

auto libdesugar::desugar(Context const context, cst::Type const& type) -> ast::Type
{
    return { std::visit(Type_desugaring_visitor { context, type }, type.variant), type.range };
}
