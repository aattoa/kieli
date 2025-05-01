#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&      db;
        Context&           ctx;
        Inference_state&   state;
        db::Environment_id env_id;
        lsp::Range         this_range;

        auto unsupported() -> hir::Type
        {
            db::add_error(db, ctx.doc_id, this_range, "Unsupported type");
            return error_type(ctx.constants, this_range);
        }

        auto recurse()
        {
            return [&](ast::Type const& type) -> hir::Type {
                return resolve_type(db, ctx, state, env_id, type);
            };
        }

        auto recurse(ast::Type const& type) -> hir::Type
        {
            return recurse()(type);
        }

        auto operator()(ast::Wildcard const&) -> hir::Type
        {
            auto type = fresh_general_type_variable(state, ctx.arena.hir, this_range);
            db::add_type_hint(db, ctx.doc_id, this_range.stop, type.id);
            return type;
        }

        auto operator()(ast::type::Never const&) -> hir::Type
        {
            return unsupported();
        }

        auto operator()(ast::Path const& path) -> hir::Type
        {
            db::Symbol_id symbol_id = resolve_path(db, ctx, state, env_id, path);
            db::Symbol&   symbol    = ctx.arena.symbols[symbol_id];

            if (auto const* local_id = std::get_if<hir::Local_type_id>(&symbol.variant)) {
                return { .id = ctx.arena.hir.local_types[*local_id].type_id, .range = this_range };
            }
            if (auto const* enum_id = std::get_if<hir::Enumeration_id>(&symbol.variant)) {
                return { .id = ctx.arena.hir.enumerations[*enum_id].type_id, .range = this_range };
            }
            if (auto const* alias_id = std::get_if<hir::Alias_id>(&symbol.variant)) {
                return { .id = resolve_alias(db, ctx, *alias_id).type.id, .range = this_range };
            }
            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return error_type(ctx.constants, this_range);
            }
            auto kind    = db::describe_symbol_kind(symbol.variant);
            auto message = std::format("Expected a type, but found {}", kind);
            db::add_error(db, ctx.doc_id, this_range, std::move(message));
            return error_type(ctx.constants, this_range);
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type
        {
            hir::type::Tuple type { std::ranges::to<std::vector>(
                std::views::transform(tuple.field_types, recurse())) };
            return { .id = ctx.arena.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Array const& array) -> hir::Type
        {
            auto length = resolve_expression(
                db, ctx, state, env_id, ctx.arena.ast.expressions[array.length]);
            hir::type::Array type {
                .element_type = recurse(ctx.arena.ast.types[array.element_type]),
                .length       = ctx.arena.hir.expressions.push(std::move(length)),
            };
            return { .id = ctx.arena.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type
        {
            hir::type::Slice type { recurse(ctx.arena.ast.types[slice.element_type]) };
            return { .id = ctx.arena.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Function const& function) -> hir::Type
        {
            hir::type::Function type {
                .parameter_types = std::views::transform(function.parameter_types, recurse())
                                 | std::ranges::to<std::vector>(),
                .return_type = recurse(ctx.arena.ast.types[function.return_type]),
            };
            return { .id = ctx.arena.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type
        {
            auto typeof_env_id = new_scope(ctx, env_id);
            auto expression    = resolve_expression(
                db, ctx, state, typeof_env_id, ctx.arena.ast.expressions[typeof_.expression]);

            report_unused(db, ctx, typeof_env_id);
            recycle_environment(ctx, typeof_env_id);

            db::add_type_hint(db, ctx.doc_id, expression.range.stop, expression.type_id);
            return hir::Type { .id = expression.type_id, .range = this_range };
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type
        {
            hir::type::Reference type {
                .referenced_type = recurse(ctx.arena.ast.types[reference.referenced_type]),
                .mutability      = resolve_mutability(db, ctx, env_id, reference.mutability),
            };
            return { .id = ctx.arena.hir.types.push(std::move(type)), .range = this_range };
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type
        {
            hir::type::Pointer type {
                .pointee_type = recurse(ctx.arena.ast.types[pointer.pointee_type]),
                .mutability   = resolve_mutability(db, ctx, env_id, pointer.mutability),
            };
            return { .id = ctx.arena.hir.types.push(std::move(type)), .range = this_range };
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
    db::Database&            db,
    Context&                 ctx,
    Inference_state&         state,
    db::Environment_id const env_id,
    ast::Type const&         type) -> hir::Type
{
    Visitor visitor {
        .db         = db,
        .ctx        = ctx,
        .state      = state,
        .env_id     = env_id,
        .this_range = type.range,
    };
    return std::visit(visitor, type.variant);
}
