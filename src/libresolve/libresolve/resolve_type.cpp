#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace libresolve;

namespace {
    struct Type_resolution_visitor {
        Context&            context;
        Inference_state&    state;
        hir::Scope_id       scope_id;
        hir::Environment_id environment_id;
        ast::Type const&    this_type;

        auto ast() -> ast::Arena&
        {
            return context.documents.at(state.document_id).ast;
        }

        auto unsupported() -> hir::Type
        {
            kieli::add_error(context.db, state.document_id, this_type.range, "Unsupported type");
            return error_type(context.constants, this_type.range);
        }

        auto recurse()
        {
            return [&](ast::Type const& type) -> hir::Type {
                return resolve_type(context, state, scope_id, environment_id, type);
            };
        }

        auto recurse(ast::Type const& type) -> hir::Type
        {
            return recurse()(type);
        }

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            return fresh_general_type_variable(state, context.hir, this_type.range);
        }

        auto operator()(ast::type::Never const&) -> hir::Type
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::Path const&) -> hir::Type
        {
            return unsupported(); // TODO
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
            ast::Arena& ast = this->ast();

            hir::Expression length = resolve_expression(
                context, state, scope_id, environment_id, ast.expressions[array.length]);
            hir::type::Array type {
                .element_type = recurse(ast.types[array.element_type]),
                .length       = context.hir.expressions.push(std::move(length)),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { recurse(ast().types[slice.element_type]) };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = std::views::transform(function.parameter_types, recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(ast().types[function.return_type]),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            return child_scope(context, scope_id, [&](hir::Scope_id typeof_scope_id) {
                hir::Expression const expression = resolve_expression(
                    context,
                    state,
                    typeof_scope_id,
                    environment_id,
                    ast().expressions[typeof_.inspected_expression]);
                return hir::Type { expression.type, this_type.range };
            });
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(ast().types[reference.referenced_type]),
                .mutability      = resolve_mutability(context, scope_id, reference.mutability),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(ast().types[pointer.pointee_type]),
                .mutability   = resolve_mutability(context, scope_id, pointer.mutability),
            };
            return { context.hir.types.push(std::move(type)), this_type.range };
        }

        auto operator()(ast::type::Impl const&) -> hir::Type
        {
            return unsupported(); // TODO
        }

        auto operator()(ast::Error const&) -> hir::Type
        {
            return error_type(context.constants, this_type.range);
        }
    };
} // namespace

auto libresolve::resolve_type(
    Context&                  context,
    Inference_state&          state,
    hir::Scope_id const       scope_id,
    hir::Environment_id const environment_id,
    ast::Type const&          type) -> hir::Type
{
    Type_resolution_visitor visitor { context, state, scope_id, environment_id, type };
    return std::visit(visitor, type.variant);
}
