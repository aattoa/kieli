#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto is_unused(db::Database& db, auto const& local) -> bool
    {
        return local.unused and not db.string_pool.get(local.name.id).starts_with('_');
    }

    void do_report_unused_bindings(
        db::Database& db, db::Document_id doc_id, auto& locals, auto& map)
    {
        for (auto const& [_, id] : map) {
            if (not is_unused(db, locals[id])) {
                continue;
            }
            auto message = std::format(
                "'{0}' is unused. If this is intentional, prefix it with an underscore: '_{0}'",
                db.string_pool.get(locals[id].name.id));
            db::add_diagnostic(db, doc_id, lsp::warning(locals[id].name.range, std::move(message)));
        }
    }

    template <typename T, hir::Identifier_map<T> Scope::* map>
    auto do_find_local(Context& ctx, auto& locals, Scope_id scope_id, utl::String_id name_id)
        -> std::optional<T>
    {
        for (;;) {
            Scope& scope = ctx.scopes.index_vector[scope_id];
            if (auto it = (scope.*map).find(name_id); it != (scope.*map).end()) {
                locals[it->second].unused = false;
                return it->second;
            }
            if (not scope.parent_id.has_value()) {
                return std::nullopt;
            }
            scope_id = scope.parent_id.value();
        }
    }

    void warn_shadowed(db::Database& db, db::Name shadowed, db::Name name, db::Document_id doc_id)
    {
        lsp::Diagnostic_related related {
            .message  = "This binding is shadowed",
            .location = lsp::Location { .doc_id = doc_id, .range = shadowed.range },
        };
        lsp::Diagnostic warning {
            .message  = std::format("'{}' shadows an unused binding", db.string_pool.get(name.id)),
            .range    = name.range,
            .severity = lsp::Severity::Warning,
            .related_info = utl::to_vector({ std::move(related) }),
        };
        db::add_diagnostic(db, doc_id, std::move(warning));
    }

    auto do_bind_local(
        db::Database&   db,
        db::Document_id doc_id,
        auto&           locals,
        auto&           map,
        db::Name        name,
        auto            local)
    {
        if (auto it = map.find(name.id); it != map.end()) {
            if (auto const& shadowed = locals[it->second]; shadowed.unused) {
                warn_shadowed(db, shadowed.name, name, doc_id);
            }
        }
        auto id = locals.push(std::move(local));
        map.insert_or_assign(name.id, id);
        db::add_reference(db, doc_id, lsp::write(name.range), id);
        return id;
    }
} // namespace

auto ki::res::bind_local_mutability(
    db::Database& db, Context& ctx, Scope_id scope_id, db::Lower name, hir::Local_mutability mut)
    -> hir::Local_mutability_id
{
    Scope& scope = ctx.scopes.index_vector[scope_id];
    return do_bind_local(
        db, scope.doc_id, ctx.hir.local_mutabilities, scope.mutabilities, name, std::move(mut));
}

auto ki::res::bind_local_variable(
    db::Database& db, Context& ctx, Scope_id scope_id, db::Lower name, hir::Local_variable var)
    -> hir::Local_variable_id
{
    Scope& scope = ctx.scopes.index_vector[scope_id];
    return do_bind_local(
        db, scope.doc_id, ctx.hir.local_variables, scope.variables, name, std::move(var));
}

auto ki::res::bind_local_type(
    db::Database& db, Context& ctx, Scope_id scope_id, db::Upper name, hir::Local_type type)
    -> hir::Local_type_id
{
    Scope& scope = ctx.scopes.index_vector[scope_id];
    return do_bind_local(db, scope.doc_id, ctx.hir.local_types, scope.types, name, std::move(type));
}

auto ki::res::find_local_variable(Context& ctx, Scope_id scope_id, utl::String_id name_id)
    -> std::optional<hir::Local_variable_id>
{
    return do_find_local<hir::Local_variable_id, &Scope::variables>(
        ctx, ctx.hir.local_variables, scope_id, name_id);
}

auto ki::res::find_local_mutability(Context& ctx, Scope_id scope_id, utl::String_id name_id)
    -> std::optional<hir::Local_mutability_id>
{
    return do_find_local<hir::Local_mutability_id, &Scope::mutabilities>(
        ctx, ctx.hir.local_mutabilities, scope_id, name_id);
}

auto ki::res::find_local_type(Context& ctx, Scope_id scope_id, utl::String_id name_id)
    -> std::optional<hir::Local_type_id>
{
    return do_find_local<hir::Local_type_id, &Scope::types>(
        ctx, ctx.hir.local_types, scope_id, name_id);
}

void ki::res::report_unused(db::Database& db, Context& ctx, Scope_id scope_id)
{
    Scope& scope = ctx.scopes.index_vector[scope_id];
    do_report_unused_bindings(db, scope.doc_id, ctx.hir.local_types, scope.types);
    do_report_unused_bindings(db, scope.doc_id, ctx.hir.local_variables, scope.variables);
    do_report_unused_bindings(db, scope.doc_id, ctx.hir.local_mutabilities, scope.mutabilities);
}

auto ki::res::make_scope(db::Document_id doc_id) -> Scope
{
    return Scope {
        .variables    = {},
        .types        = {},
        .mutabilities = {},
        .doc_id       = doc_id,
        .parent_id    = std::nullopt,
    };
}
