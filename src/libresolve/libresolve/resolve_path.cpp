#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto environment_name(db::Database& db, Context& ctx, hir::Environment_id env_id) -> std::string
    {
        if (auto name_id = ctx.hir.environments[env_id].name_id) {
            return std::format("Module '{}'", db.string_pool.get(name_id.value()));
        }
        return "The root module";
    }

    auto environment_root(Context& ctx, hir::Environment_id env_id) -> hir::Environment_id
    {
        for (;;) {
            auto const& env = ctx.hir.environments[env_id];
            if (not env.parent_id.has_value()) {
                return env_id;
            }
            env_id = env.parent_id.value();
        }
    }

    auto symbol_environment(Context& ctx, hir::Symbol symbol) -> std::optional<hir::Environment_id>
    {
        if (auto const* module_id = std::get_if<hir::Module_id>(&symbol)) {
            return ctx.hir.modules[*module_id].mod_env_id;
        }
        return std::nullopt; // TODO: associated modules
    }

    auto apply_segment(
        db::Database&            db,
        Context&                 ctx,
        hir::Environment_id      env_id,
        ast::Path_segment const& segment) -> std::optional<hir::Symbol>
    {
        if (segment.template_arguments.has_value()) {
            db::add_error(db, ctx.doc_id, segment.name.range, "Template arguments are unsupported");
            return db::Error {};
        }
        auto& map = ctx.hir.environments[env_id].map;
        if (auto const it = map.find(segment.name.id); it != map.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    auto lookup(
        db::Database&                      db,
        Context&                           ctx,
        hir::Environment_id                env_id,
        std::span<ast::Path_segment const> segments) -> hir::Symbol
    {
        cpputil::always_assert(not segments.empty());
        for (;;) {
            auto const& segment = segments.front();
            segments            = segments.subspan(1);

            if (auto symbol = apply_segment(db, ctx, env_id, segment)) {
                if (segments.empty()) {
                    return symbol.value();
                }
                else if (auto next_env_id = symbol_environment(ctx, symbol.value())) {
                    env_id = next_env_id.value();
                }
                else {
                    auto message = std::format(
                        "Expected a module, but '{}' is {}",
                        db.string_pool.get(segment.name.id),
                        hir::describe_symbol_kind(symbol.value()));
                    db::add_error(db, ctx.doc_id, segment.name.range, std::move(message));
                    return db::Error {};
                }
            }
            else {
                auto message = std::format(
                    "{} does not contain '{}'",
                    environment_name(db, ctx, env_id),
                    db.string_pool.get(segment.name.id));
                db::add_error(db, ctx.doc_id, segment.name.range, std::move(message));
                return db::Error {};
            }
        }
    }

    auto find_starting_point(Context& ctx, hir::Environment_id env_id, db::Name name)
        -> std::optional<hir::Environment_id>
    {
        for (;;) {
            auto& env = ctx.hir.environments[env_id];
            if (env.map.contains(name.id)) {
                return env_id;
            }
            if (not env.parent_id.has_value()) {
                return std::nullopt;
            }
            env_id = env.parent_id.value();
        }
    }

    auto find_local(Context& ctx, Scope_id scope_id, db::Name name) -> std::optional<hir::Symbol>
    {
        if (auto local_id = find_local_variable(ctx, scope_id, name.id)) {
            return local_id.value();
        }
        if (auto local_id = find_local_type(ctx, scope_id, name.id)) {
            return local_id.value();
        }
        return std::nullopt;
    }
} // namespace

auto ki::res::resolve_path(
    db::Database&       db,
    Context&            ctx,
    Inference_state&    state,
    Scope_id            scope_id,
    hir::Environment_id env_id,
    ast::Path const&    path) -> hir::Symbol
{
    (void)state; // Inference state will be needed for template argument resolution.

    auto const visitor = utl::Overload {
        [&](std::monostate) -> hir::Symbol {
            cpputil::always_assert(not path.segments.empty());
            db::Name front = path.segments.front().name;

            if (path.segments.size() == 1) {
                if (auto local = find_local(ctx, scope_id, front)) {
                    return local.value();
                }
            }
            if (auto start_env_id = find_starting_point(ctx, env_id, front)) {
                return lookup(db, ctx, start_env_id.value(), path.segments);
            }

            auto message = std::format("Undeclared identifier: '{}'", db.string_pool.get(front.id));
            db::add_error(db, ctx.doc_id, front.range, std::move(message));
            return db::Error {};
        },
        [&](ast::Path_root_global) -> hir::Symbol {
            return lookup(db, ctx, environment_root(ctx, env_id), path.segments);
        },
        [&](ast::Type_id) -> hir::Symbol { cpputil::todo(); },
    };

    return std::visit(visitor, path.root);
}
