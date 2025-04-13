#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&       db;
        Context&            ctx;
        Inference_state&    state;
        Scope_id            scope_id;
        hir::Environment_id env_id;
        lsp::Range          this_range;

        auto ast() -> ast::Arena&
        {
            return db.documents[state.doc_id].ast;
        }

        auto unsupported() -> hir::Type
        {
            db::add_error(db, state.doc_id, this_range, "Unsupported type");
            return error_type(ctx.constants, this_range);
        }

        auto recurse()
        {
            return [&](ast::Type const& type) -> hir::Type {
                return resolve_type(db, ctx, state, scope_id, env_id, type);
            };
        }

        auto recurse(ast::Type const& type) -> hir::Type
        {
            return recurse()(type);
        }

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            auto type = fresh_general_type_variable(state, ctx.hir, this_range);
            db::add_type_hint(db, state.doc_id, this_range.stop, type.id);
            return type;
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
                    return { .id = bind->type, .range = this_range };
                }
            }
            return unsupported();
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type
        {
            hir::type::Tuple type { std::ranges::to<std::vector>(
                std::views::transform(tuple.field_types, recurse())) };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Array const& array) -> hir::Type
        {
            auto length = resolve_expression(
                db, ctx, state, scope_id, env_id, ast().expressions[array.length]);
            hir::type::Array type {
                .element_type = recurse(ast().types[array.element_type]),
                .length       = ctx.hir.expressions.push(std::move(length)),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { recurse(ast().types[slice.element_type]) };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = std::views::transform(function.parameter_types, recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(ast().types[function.return_type]),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            return child_scope(ctx, scope_id, [&](Scope_id scope_id) {
                auto expression = resolve_expression(
                    db, ctx, state, scope_id, env_id, ast().expressions[typeof_.expression]);
                return hir::Type { .id = expression.type, .range = this_range };
            });
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(ast().types[reference.referenced_type]),
                .mutability      = resolve_mutability(db, ctx, scope_id, reference.mutability),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(ast().types[pointer.pointee_type]),
                .mutability   = resolve_mutability(db, ctx, scope_id, pointer.mutability),
            };
            return { .id = ctx.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Impl const&) -> hir::Type
        {
            return unsupported();
        }

        auto operator()(db::Error const&) const -> hir::Type
        {
            return error_type(ctx.constants, this_range);
        }
    };
} // namespace

auto ki::res::resolve_type(
    db::Database&             db,
    Context&                  ctx,
    Inference_state&          state,
    Scope_id const            scope_id,
    hir::Environment_id const env_id,
    ast::Type const&          type) -> hir::Type
{
    Visitor visitor {
        .db         = db,
        .ctx        = ctx,
        .state      = state,
        .scope_id   = scope_id,
        .env_id     = env_id,
        .this_range = type.range,
    };
    return std::visit(visitor, type.variant);
}
