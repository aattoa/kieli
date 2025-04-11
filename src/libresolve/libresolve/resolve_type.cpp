#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        Context&            ctx;
        Inference_state&    state;
        Scope_id            scope_id;
        hir::Environment_id env_id;
        ast::Type const&    this_type;

        auto ast() -> ast::Arena&
        {
            return ctx.db.documents[state.doc_id].ast;
        }

        auto unsupported() -> hir::Type
        {
            db::add_error(ctx.db, state.doc_id, this_type.range, "Unsupported type");
            return error_type(ctx.constants, this_type.range);
        }

        auto recurse()
        {
            return [&](ast::Type const& type) -> hir::Type {
                return resolve_type(ctx, state, scope_id, env_id, type);
            };
        }

        auto recurse(ast::Type const& type) -> hir::Type
        {
            return recurse()(type);
        }

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            return fresh_general_type_variable(state, ctx.hir, this_type.range);
        }

        auto operator()(ast::type::Never const&) -> hir::Type
        {
            return unsupported();
        }

        auto operator()(ast::Path const& path) -> hir::Type
        {
            if (path.is_unqualified()) {
                if (auto* bind = find_type(ctx, scope_id, path.head().name.id)) {
                    bind->unused = false;
                    return { .id = bind->type, .range = this_type.range };
                }
            }
            return unsupported();
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type
        {
            hir::type::Tuple type { std::ranges::to<std::vector>(
                std::views::transform(tuple.field_types, recurse())) };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_type.range };
        }

        auto operator()(ast::type::Array const& array) -> hir::Type
        {
            auto length
                = resolve_expression(ctx, state, scope_id, env_id, ast().expressions[array.length]);
            hir::type::Array type {
                .element_type = recurse(ast().types[array.element_type]),
                .length       = ctx.hir.expressions.push(std::move(length)),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_type.range };
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { recurse(ast().types[slice.element_type]) };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_type.range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = std::views::transform(function.parameter_types, recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(ast().types[function.return_type]),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_type.range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            return child_scope(ctx, scope_id, [&](Scope_id scope_id) {
                auto expression = resolve_expression(
                    ctx, state, scope_id, env_id, ast().expressions[typeof_.expression]);
                return hir::Type { .id = expression.type, .range = this_type.range };
            });
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(ast().types[reference.referenced_type]),
                .mutability      = resolve_mutability(ctx, scope_id, reference.mutability),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_type.range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(ast().types[pointer.pointee_type]),
                .mutability   = resolve_mutability(ctx, scope_id, pointer.mutability),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_type.range };
        }

        auto operator()(ast::type::Impl const&) -> hir::Type
        {
            return unsupported();
        }

        auto operator()(db::Error const&) -> hir::Type
        {
            return error_type(ctx.constants, this_type.range);
        }
    };
} // namespace

auto ki::res::resolve_type(
    Context&                  ctx,
    Inference_state&          state,
    Scope_id const            scope_id,
    hir::Environment_id const env_id,
    ast::Type const&          type) -> hir::Type
{
    Visitor visitor {
        .ctx       = ctx,
        .state     = state,
        .scope_id  = scope_id,
        .env_id    = env_id,
        .this_type = type,
    };
    return std::visit(visitor, type.variant);
}
