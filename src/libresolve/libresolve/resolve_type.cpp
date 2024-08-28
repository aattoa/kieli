#include <libutl/utilities.hpp>
#include <libresolve/resolution_internals.hpp>

using namespace libresolve;

namespace {
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

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            return state.fresh_general_type_variable(context.hir, this_type.range);
        }

        auto operator()(ast::type::Never const&) -> hir::Type
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
            // TODO: lookup
            kieli::add_error(
                context.db,
                scope.document_id(),
                this_type.range,
                "Use of an undeclared identifier");
            return { context.constants.error_type, this_type.range };
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type
        {
            hir::type::Tuple type {
                std::ranges::to<std::vector>(std::views::transform(tuple.field_types, recurse())),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Array const& array) -> hir::Type
        {
            hir::type::Array type {
                .element_type = recurse(context.ast.types[array.element_type]),
                .length       = context.hir.expressions.push(resolve_expression(
                    context, state, scope, environment, context.ast.expressions[array.length])),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { recurse(context.ast.types[slice.element_type]) };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = std::views::transform(function.parameter_types, recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(context.ast.types[function.return_type]),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            Scope      typeof_scope = scope.child();
            auto const expression   = resolve_expression(
                context,
                state,
                typeof_scope,
                environment,
                context.ast.expressions[typeof_.inspected_expression]);
            return { expression.type, this_type.range };
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(context.ast.types[reference.referenced_type]),
                .mutability      = resolve_mutability(context, scope, reference.mutability),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(context.ast.types[pointer.pointee_type]),
                .mutability   = resolve_mutability(context, scope, pointer.mutability),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Implementation const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::type::Template_application const&) -> hir::Type
        {
            cpputil::todo();
        }

        auto operator()(ast::Error const&) -> hir::Type
        {
            return error_type(context.constants, this_type.range);
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
    Type_resolution_visitor visitor { context, state, scope, environment, type };
    return std::visit(visitor, type.variant);
}
