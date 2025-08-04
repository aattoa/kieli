#include <libutl/utilities.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::res;

namespace {
    void set_completion(
        db::Database&       db,
        db::Document_id     doc_id,
        db::Environment_id  env_id,
        db::Name            name,
        db::Completion_mode mode)
    {
        db::Environment_completion completion { .env_id = env_id, .mode = mode };
        db::add_completion(db, doc_id, name, completion);
    }

    auto environment_name(db::Database& db, Context& ctx, db::Environment_id env_id) -> std::string
    {
        if (auto name_id = ctx.arena.environments[env_id].name_id) {
            return std::format("Module '{}'", db.string_pool.get(name_id.value()));
        }
        return "The root module";
    }

    auto type_associated_environment(db::Database& db, Context& ctx, hir::Type_id type_id)
        -> std::optional<db::Environment_id>
    {
        auto const& variant = ctx.arena.hir.types[type_id];
        if (auto const* enumeration = std::get_if<hir::type::Enumeration>(&variant)) {
            return resolve_enumeration(db, ctx, enumeration->id).associated_env_id;
        }
        if (auto const* structure = std::get_if<hir::type::Structure>(&variant)) {
            return resolve_structure(db, ctx, structure->id).associated_env_id;
        }
        return std::nullopt;
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
        if (auto const* struct_id = std::get_if<hir::Structure_id>(&symbol.variant)) {
            return resolve_structure(db, ctx, *struct_id).associated_env_id;
        }
        if (auto const* alias_id = std::get_if<hir::Alias_id>(&symbol.variant)) {
            auto const& alias = resolve_alias(db, ctx, *alias_id);
            return type_associated_environment(db, ctx, alias.type.id);
        }
        return std::nullopt;
    }

    auto apply_segment(
        db::Database& db, Context& ctx, db::Environment_id env_id, ast::Path_segment const& segment)
        -> std::optional<db::Symbol_id>
    {
        auto& map = ctx.arena.environments[env_id].map;
        if (auto const it = map.find(segment.name.id); it != map.end()) {
            if (segment.template_arguments.has_value()) {
                std::string message = "Template argument resolution has not been implemented";
                ctx.add_diagnostic(lsp::error(segment.name.range, std::move(message)));
            }
            ++ctx.arena.symbols[it->second].use_count;
            db::add_reference(db, ctx.doc_id, lsp::read(segment.name.range), it->second);
            return it->second;
        }
        return std::nullopt;
    }

    auto lookup(
        db::Database&                      db,
        Context&                           ctx,
        db::Environment_id                 site_env_id,
        db::Environment_id                 lookup_env_id,
        db::Completion_mode                mode,
        std::span<ast::Path_segment const> segments) -> db::Symbol_id
    {
        for (;;) {
            cpputil::always_assert(not segments.empty());

            auto const& segment = segments.front();
            segments            = segments.subspan(1);

            auto complete_env_id = mode == db::Completion_mode::Path ? lookup_env_id : site_env_id;
            set_completion(db, ctx.doc_id, complete_env_id, segment.name, mode);
            mode = db::Completion_mode::Path;

            if (auto symbol = apply_segment(db, ctx, lookup_env_id, segment)) {
                if (segments.empty()) {
                    return symbol.value();
                }
                else if (auto next_env_id = symbol_environment(db, ctx, symbol.value())) {
                    lookup_env_id = next_env_id.value();
                }
                else {
                    auto message = std::format(
                        "Expected a module, but '{}' is {}",
                        db.string_pool.get(segment.name.id),
                        db::describe_symbol_kind(ctx.arena.symbols[symbol.value()].variant));
                    ctx.add_diagnostic(lsp::error(segment.name.range, std::move(message)));
                    return new_symbol(ctx, segment.name, db::Error {});
                }
            }
            else {
                auto message = std::format(
                    "{} does not contain '{}'",
                    environment_name(db, ctx, lookup_env_id),
                    db.string_pool.get(segment.name.id));
                ctx.add_diagnostic(lsp::error(segment.name.range, std::move(message)));
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
    auto const visitor = utl::Overload {
        [&](std::monostate) {
            cpputil::always_assert(not path.segments.empty());
            db::Name front = path.segments.front().name;

            if (auto start_env_id = find_starting_point(ctx, env_id, front)) {
                return lookup(
                    db, ctx, env_id, start_env_id.value(), db::Completion_mode::Top, path.segments);
            }
            set_completion(db, ctx.doc_id, env_id, front, db::Completion_mode::Top);

            auto message = std::format("Undeclared identifier: '{}'", db.string_pool.get(front.id));
            ctx.add_diagnostic(lsp::error(front.range, std::move(message)));
            return new_symbol(ctx, front, db::Error {});
        },
        [&](ast::Path_root_global) {
            return lookup(
                db, ctx, env_id, ctx.root_env_id, db::Completion_mode::Path, path.segments);
        },
        [&](ast::Type_id type_id) {
            auto type = resolve_type(db, ctx, state, env_id, ctx.arena.ast.types[type_id]);

            if (auto associated_env_id = type_associated_environment(db, ctx, type.id)) {
                return lookup(
                    db,
                    ctx,
                    env_id,
                    associated_env_id.value(),
                    db::Completion_mode::Path,
                    path.segments);
            }

            auto message = std::format(
                "'{}' has no associated environment",
                hir::to_string(ctx.arena.hir, db.string_pool, type));
            ctx.add_diagnostic(lsp::error(type.range, std::move(message)));

            db::Name name { .id = db.string_pool.make("(ERROR)"sv), .range = type.range };
            return new_symbol(ctx, name, db::Error {});
        },
    };

    return std::visit(visitor, path.root);
}
