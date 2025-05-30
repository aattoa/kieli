#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    auto environment_name(db::Database& db, Context& ctx, db::Environment_id env_id) -> std::string
    {
        if (auto name_id = ctx.arena.environments[env_id].name_id) {
            return std::format("Module '{}'", db.string_pool.get(name_id.value()));
        }
        return "The root module";
    }

    auto symbol_environment(db::Database& db, Context& ctx, db::Symbol_id symbol_id)
        -> std::optional<db::Environment_id>
    {
        db::Symbol& symbol = ctx.arena.symbols[symbol_id];
        if (auto const* module_id = std::get_if<hir::Module_id>(&symbol.variant)) {
            return ctx.arena.hir.modules[*module_id].mod_env_id;
        }
        if (auto const* enum_id = std::get_if<hir::Enumeration_id>(&symbol.variant)) {
            return resolve_enumeration(db, ctx, *enum_id).associated_env_id;
        }
        return std::nullopt; // TODO: associated modules
    }

    auto apply_segment(
        db::Database& db, Context& ctx, db::Environment_id env_id, ast::Path_segment const& segment)
        -> std::optional<db::Symbol_id>
    {
        if (segment.template_arguments.has_value()) {
            db::add_error(db, ctx.doc_id, segment.name.range, "Template arguments are unsupported");
            return new_symbol(ctx, segment.name, db::Error {});
        }
        auto& map = ctx.arena.environments[env_id].map;
        if (auto const it = map.find(segment.name.id); it != map.end()) {
            ++ctx.arena.symbols[it->second].use_count;
            db::add_reference(db, ctx.doc_id, lsp::read(segment.name.range), it->second);
            return it->second;
        }
        return std::nullopt;
    }

    auto lookup(
        db::Database&                      db,
        Context&                           ctx,
        db::Environment_id                 env_id,
        std::span<ast::Path_segment const> segments) -> db::Symbol_id
    {
        cpputil::always_assert(not segments.empty());
        for (;;) {
            auto const& segment = segments.front();
            segments            = segments.subspan(1);

            if (auto symbol = apply_segment(db, ctx, env_id, segment)) {
                if (segments.empty()) {
                    return symbol.value();
                }
                else if (auto next_env_id = symbol_environment(db, ctx, symbol.value())) {
                    env_id = next_env_id.value();
                }
                else {
                    auto message = std::format(
                        "Expected a module, but '{}' is {}",
                        db.string_pool.get(segment.name.id),
                        db::describe_symbol_kind(ctx.arena.symbols[symbol.value()].variant));
                    db::add_error(db, ctx.doc_id, segment.name.range, std::move(message));
                    return new_symbol(ctx, segment.name, db::Error {});
                }
            }
            else {
                auto message = std::format(
                    "{} does not contain '{}'",
                    environment_name(db, ctx, env_id),
                    db.string_pool.get(segment.name.id));
                db::add_error(db, ctx.doc_id, segment.name.range, std::move(message));
                return new_symbol(ctx, segment.name, db::Error {});
            }
        }
    }

    auto find_starting_point(Context& ctx, db::Environment_id env_id, db::Name name)
        -> std::optional<db::Environment_id>
    {
        for (;;) {
            auto& env = ctx.arena.environments[env_id];
            if (env.map.contains(name.id)) {
                return env_id;
            }
            if (not env.parent_id.has_value()) {
                return std::nullopt;
            }
            env_id = env.parent_id.value();
        }
    }
} // namespace

auto ki::res::resolve_path(
    db::Database&      db,
    Context&           ctx,
    Block_state&       state,
    db::Environment_id env_id,
    ast::Path const&   path) -> db::Symbol_id
{
    (void)state; // Block state will be needed for template argument resolution.

    auto const visitor = utl::Overload {
        [&](std::monostate) -> db::Symbol_id {
            cpputil::always_assert(not path.segments.empty());
            db::Name front = path.segments.front().name;

            if (auto start_env_id = find_starting_point(ctx, env_id, front)) {
                return lookup(db, ctx, start_env_id.value(), path.segments);
            }

            auto message = std::format("Undeclared identifier: '{}'", db.string_pool.get(front.id));
            db::add_error(db, ctx.doc_id, front.range, std::move(message));
            return new_symbol(ctx, front, db::Error {});
        },
        [&](ast::Path_root_global) -> db::Symbol_id {
            return lookup(db, ctx, ctx.root_env_id, path.segments);
        },
        [&](ast::Type_id) -> db::Symbol_id { cpputil::todo(); },
    };

    return std::visit(visitor, path.root);
}
