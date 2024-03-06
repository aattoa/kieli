#include <libutl/common/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

namespace {
    using namespace libresolve;

    auto integer_type(Constants const& constants, kieli::built_in_type::Integer const integer)
        -> utl::Mutable_wrapper<hir::Type::Variant>
    {
        // clang-format off
        switch (integer) {
        case kieli::built_in_type::Integer::i8:  return constants.i8_type;
        case kieli::built_in_type::Integer::i16: return constants.i16_type;
        case kieli::built_in_type::Integer::i32: return constants.i32_type;
        case kieli::built_in_type::Integer::i64: return constants.i64_type;
        case kieli::built_in_type::Integer::u8:  return constants.u8_type;
        case kieli::built_in_type::Integer::u16: return constants.u16_type;
        case kieli::built_in_type::Integer::u32: return constants.u32_type;
        case kieli::built_in_type::Integer::u64: return constants.u64_type;
        default:
            cpputil::unreachable();
        }
        // clang-format on
    }

    struct Type_resolution_visitor {
        Context&            context;
        Inference_state&    state;
        Scope&              scope;
        Environment_wrapper environment;
        ast::Type const&    this_type;

        auto recurse()
        {
            return [&](ast::Type const& type) -> hir::Type {
                return resolve_type(context, state, scope, environment, type);
            };
        }

        auto recurse(ast::Type const& expression) -> hir::Type
        {
            return recurse()(expression);
        }

        auto operator()(kieli::built_in_type::Integer const& integer) -> hir::Type
        {
            return { integer_type(context.constants, integer), this_type.source_range };
        }

        auto operator()(kieli::built_in_type::Floating const&) -> hir::Type
        {
            return { context.constants.floating_type, this_type.source_range };
        }

        auto operator()(kieli::built_in_type::Character const&) -> hir::Type
        {
            return { context.constants.character_type, this_type.source_range };
        }

        auto operator()(kieli::built_in_type::Boolean const&) -> hir::Type
        {
            return { context.constants.boolean_type, this_type.source_range };
        }

        auto operator()(kieli::built_in_type::String const&) -> hir::Type
        {
            return { context.constants.string_type, this_type.source_range };
        }

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            return state.fresh_general_type_variable(context.arenas, this_type.source_range);
        }

        auto operator()(ast::type::Self const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::type::Typename const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type
        {
            hir::type::Tuple type {
                .types = tuple.field_types                //
                       | std::views::transform(recurse()) //
                       | std::ranges::to<std::vector>(),
            };
            return { context.arenas.type(std::move(type)), this_type.source_range };
        }

        auto operator()(ast::type::Array const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { .element_type = recurse(*slice.element_type) };
            return { context.arenas.type(std::move(type)), this_type.source_range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = function.parameter_types //
                                 | std::views::transform(recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(*function.return_type),
            };
            return { context.arenas.type(std::move(type)), this_type.source_range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            auto inspection_scope = scope.child();

            auto const expression = resolve_expression(
                context, state, inspection_scope, environment, *typeof_.inspected_expression);

            return expression.type;
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(*reference.referenced_type),
                .mutability      = resolve_mutability(context, state, scope, reference.mutability),
            };
            return { context.arenas.type(std::move(type)), this_type.source_range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(*pointer.pointee_type),
                .mutability   = resolve_mutability(context, state, scope, pointer.mutability),
            };
            return { context.arenas.type(std::move(type)), this_type.source_range };
        }

        auto operator()(ast::type::Instance_of const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::type::Template_application const&) -> hir::Type
        {
            cpputil::todo();
        }
    };
} // namespace

auto libresolve::resolve_type(
    Context&            context,
    Inference_state&    state,
    Scope&              scope,
    Environment_wrapper environment,
    ast::Type const&    type) -> hir::Type
{
    return std::visit(
        Type_resolution_visitor { context, state, scope, environment, type }, type.variant);
}
