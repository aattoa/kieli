#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    struct Visitor {
        db::Database&      db;
        Context&           ctx;
        Block_state&       state;
        db::Environment_id env_id;
        lsp::Range         this_range;

        auto recurse(ast::Type_id type_id) -> hir::Type_id
        {
            return resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[type_id]);
        }

        auto operator()(ast::Wildcard const&) -> hir::Type_id
        {
            auto type_id = fresh_general_type_variable(ctx, state, this_range);
            db::add_type_hint(db, ctx.doc_id, this_range.stop, type_id);
            return type_id;
        }

        auto operator()(ast::Builtin const& builtin) -> hir::Type_id
        {
            db::add_completion(db, ctx.doc_id, builtin.name, db::Builtin_completion::Type);
            if (auto type = hir::get_builtin_type(db.string_pool.get(builtin.name.id))) {
                return builtin_type_id(ctx.builtins, type.value());
            }
            ctx.add_diagnostic(lsp::error(builtin.name.range, "Unknown built-in type"));
            return ctx.builtins.type_error;
        }

        auto operator()(ast::Path const& path) -> hir::Type_id
        {
            db::Symbol& symbol = ctx.arena.symbols[resolve_path(db, ctx, state, env_id, path)];

            if (auto const* local_id = std::get_if<hir::Local_type_id>(&symbol.variant)) {
                return ctx.arena.hir.local_types[*local_id].type_id;
            }
            if (auto const* struct_id = std::get_if<hir::Structure_id>(&symbol.variant)) {
                return ctx.arena.hir.structures[*struct_id].type_id;
            }
            if (auto const* enum_id = std::get_if<hir::Enumeration_id>(&symbol.variant)) {
                return ctx.arena.hir.enumerations[*enum_id].type_id;
            }
            if (auto const* alias_id = std::get_if<hir::Alias_id>(&symbol.variant)) {
                return resolve_alias(db, ctx, *alias_id).type_id;
            }
            if (std::holds_alternative<db::Error>(symbol.variant)) {
                return ctx.builtins.type_error;
            }

            auto kind    = db::describe_symbol_kind(symbol.variant);
            auto message = std::format("Expected a type, but found {}", kind);
            ctx.add_diagnostic(lsp::error(this_range, std::move(message)));
            return ctx.builtins.type_error;
        }

        auto operator()(ast::type::Tuple const& tuple) -> hir::Type_id
        {
            return ctx.arena.hir.types.push(
                hir::type::Tuple {
                    .types = tuple.fields
                           | std::views::transform(std::bind_front(&Visitor::recurse, this))
                           | std::ranges::to<std::vector>(),
                });
        }

        auto operator()(ast::type::Array const& array) -> hir::Type_id
        {
            auto length = resolve_expression(
                db, ctx, state, env_id, ctx.arena.ast.expressions[array.length]);
            return ctx.arena.hir.types.push(
                hir::type::Array {
                    .element_type = recurse(array.element_type),
                    .length       = ctx.arena.hir.expressions.push(std::move(length)),
                });
        }

        auto operator()(ast::type::Slice const& slice) -> hir::Type_id
        {
            return ctx.arena.hir.types.push(
                hir::type::Slice { .element_type = recurse(slice.element_type) });
        }

        auto operator()(ast::type::Function const& function) -> hir::Type_id
        {
            return ctx.arena.hir.types.push(
                hir::type::Function {
                    .parameter_types
                    = function.parameter_types
                    | std::views::transform(std::bind_front(&Visitor::recurse, this))
                    | std::ranges::to<std::vector>(),
                    .return_type = recurse(function.return_type),
                });
        }

        auto operator()(ast::type::Typeof const& typeof_) -> hir::Type_id
        {
            auto typeof_env_id = new_scope(ctx, env_id);
            auto expression    = resolve_expression(
                db, ctx, state, typeof_env_id, ctx.arena.ast.expressions[typeof_.expression]);
            report_unused(db, ctx, typeof_env_id);
            db::add_type_hint(db, ctx.doc_id, expression.range.stop, expression.type_id);
            return expression.type_id;
        }

        auto operator()(ast::type::Reference const& reference) -> hir::Type_id
        {
            return ctx.arena.hir.types.push(
                hir::type::Reference {
                    .referenced_type = recurse(reference.referenced_type),
                    .mutability      = resolve_mutability(db, ctx, env_id, reference.mutability),
                });
        }

        auto operator()(ast::type::Pointer const& pointer) -> hir::Type_id
        {
            return ctx.arena.hir.types.push(
                hir::type::Pointer {
                    .pointee_type = recurse(pointer.pointee_type),
                    .mutability   = resolve_mutability(db, ctx, env_id, pointer.mutability),
                });
        }

        auto operator()(ast::type::Impl const&) -> hir::Type_id
        {
            ctx.add_diagnostic(lsp::error(this_range, "Impl resolution has not been implemented"));
            return ctx.builtins.type_error;
        }

        auto operator()(db::Error const&) const -> hir::Type_id
        {
            return ctx.builtins.type_error;
        }
    };
} // namespace

auto ki::res::resolve_type(
    db::Database&            db,
    Context&                 ctx,
    Block_state&             state,
    db::Environment_id const env_id,
    ast::Type const&         type) -> hir::Type_id
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
