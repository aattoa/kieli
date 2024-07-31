#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
    auto integer_type(Constants const& constants, kieli::built_in_type::Integer const integer)
        -> hir::Type_id
    {
        switch (integer) {
        case kieli::built_in_type::Integer::i8:  return constants.i8_type;
        case kieli::built_in_type::Integer::i16: return constants.i16_type;
        case kieli::built_in_type::Integer::i32: return constants.i32_type;
        case kieli::built_in_type::Integer::i64: return constants.i64_type;
        case kieli::built_in_type::Integer::u8:  return constants.u8_type;
        case kieli::built_in_type::Integer::u16: return constants.u16_type;
        case kieli::built_in_type::Integer::u32: return constants.u32_type;
        case kieli::built_in_type::Integer::u64: return constants.u64_type;
        default:                                 cpputil::unreachable();
        }
    }

    struct Type_resolution_visitor {
        Context&            context;
        Inference_state&    state;
        Scope&              scope;
        hir::Environment_id environment;
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
            return { integer_type(context.constants, integer), this_type.range };
        }

        auto operator()(kieli::built_in_type::Floating const&) -> hir::Type
        {
            return { context.constants.floating_type, this_type.range };
        }

        auto operator()(kieli::built_in_type::Character const&) -> hir::Type
        {
            return { context.constants.character_type, this_type.range };
        }

        auto operator()(kieli::built_in_type::Boolean const&) -> hir::Type
        {
            return { context.constants.boolean_type, this_type.range };
        }

        auto operator()(kieli::built_in_type::String const&) -> hir::Type
        {
            return { context.constants.string_type, this_type.range };
        }

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            return state.fresh_general_type_variable(context.hir, this_type.range);
        }

        auto operator()(ast::type::Self const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::type::Typename const& type) -> hir::Type
        {
            if (type.path.is_simple_name()) {
                if (auto* const bind = scope.find_type(type.path.head.identifier)) {
                    return { bind->type, this_type.range };
                }
            }
            if (auto const lookup_result
                = lookup_upper(context, state, scope, environment, type.path)) {
                return std::visit(
                    utl::Overload {
                        [&](hir::Enumeration_id const enumeration) -> hir::Type {
                            return context.info.enumerations[enumeration].type;
                        },
                        [&](hir::Alias_id const alias) -> hir::Type {
                            return resolve_alias(context, context.info.aliases[alias]).type;
                        },
                        [](hir::Typeclass_id) -> hir::Type { cpputil::todo(); },
                    },
                    lookup_result.value().variant);
            }
            kieli::emit_diagnostic(
                cppdiag::Severity::error,
                context.db,
                scope.source(),
                this_type.range,
                "Use of an undeclared identifier");
            return { context.constants.error_type, this_type.range };
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type
        {
            hir::type::Tuple type {
                .types = std::views::transform(tuple.field_types, recurse())
                       | std::ranges::to<std::vector>(),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Array const& array) -> hir::Type
        {
            hir::type::Array type {
                .element_type = recurse(*array.element_type),
                .length       = context.hir.expressions.push(
                    resolve_expression(context, state, scope, environment, *array.length)),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { .element_type = recurse(*slice.element_type) };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = std::views::transform(function.parameter_types, recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(*function.return_type),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            auto       typeof_scope = scope.child();
            auto const expression   = resolve_expression(
                context, state, typeof_scope, environment, *typeof_.inspected_expression);
            return { expression.type, this_type.range };
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(*reference.referenced_type),
                .mutability      = resolve_mutability(context, scope, reference.mutability),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(*pointer.pointee_type),
                .mutability   = resolve_mutability(context, scope, pointer.mutability),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
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
    hir::Environment_id environment,
    ast::Type const&    type) -> hir::Type
{
    return std::visit(
        Type_resolution_visitor { context, state, scope, environment, type }, type.variant);
}
