#include <libutl/utilities.hpp>
#include <libdesugar/desugaring_internals.hpp>

using namespace libdesugar;

namespace {
    struct Type_desugaring_visitor {
        Context&         context;
        cst::Type const& this_type;

        auto operator()(cst::type::Parenthesized const& parenthesized) -> ast::Type_variant
        {
            return std::visit(*this, context.cst.types[parenthesized.type.value].variant);
        }

        auto operator()(cst::Path const& path) -> ast::Type_variant
        {
            return context.desugar(path);
        }

        auto operator()(cst::Wildcard const& wildcard) -> ast::Type_variant
        {
            return context.desugar(wildcard);
        }

        auto operator()(cst::type::Never const&) -> ast::Type_variant
        {
            return ast::type::Never {};
        }

        auto operator()(cst::type::Tuple const& tuple) -> ast::Type_variant
        {
            return ast::type::Tuple {
                .field_types = tuple.field_types.value.elements
                             | std::views::transform(context.deref_desugar())
                             | std::ranges::to<std::vector>(),
            };
        }

        auto operator()(cst::type::Array const& array) -> ast::Type_variant
        {
            return ast::type::Array {
                .element_type = context.desugar(array.element_type),
                .length       = context.desugar(array.length),
            };
        }

        auto operator()(cst::type::Slice const& slice) -> ast::Type_variant
        {
            return ast::type::Slice { .element_type = context.desugar(slice.element_type.value) };
        }

        auto operator()(cst::type::Function const& function) -> ast::Type_variant
        {
            return ast::type::Function {
                .parameter_types = function.parameter_types.value.elements
                                 | std::views::transform(context.deref_desugar())
                                 | std::ranges::to<std::vector>(),
                .return_type = context.desugar(function.return_type),
            };
        }

        auto operator()(cst::type::Typeof const& typeof_) -> ast::Type_variant
        {
            return ast::type::Typeof { context.desugar(typeof_.inspected_expression.value) };
        }

        auto operator()(cst::type::Reference const& reference) -> ast::Type_variant
        {
            auto const ampersand_range = context.cst.tokens[reference.ampersand_token].range;
            return ast::type::Reference {
                .referenced_type = context.desugar(reference.referenced_type),
                .mutability = context.desugar_mutability(reference.mutability, ampersand_range),
            };
        }

        auto operator()(cst::type::Pointer const& pointer) -> ast::Type_variant
        {
            auto const asterisk_range = context.cst.tokens[pointer.asterisk_token].range;
            return ast::type::Pointer {
                .pointee_type = context.desugar(pointer.pointee_type),
                .mutability   = context.desugar_mutability(pointer.mutability, asterisk_range),
            };
        }

        auto operator()(cst::type::Implementation const& implementation) -> ast::Type_variant
        {
            return ast::type::Implementation { context.desugar(implementation.concepts.elements) };
        }
    };
} // namespace

auto libdesugar::Context::desugar(cst::Type const& type) -> ast::Type
{
    return { std::visit(Type_desugaring_visitor { *this, type }, type.variant), type.range };
}
