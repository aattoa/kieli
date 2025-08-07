#include <libutl/utilities.hpp>
#include <language-server/server.hpp>

using namespace ki;

namespace {
    struct Visitor {
        db::Database const& db;
        db::Arena const&    arena;

        void format_segment(db::Environment_id env_id, std::string& output)
        {
            auto const& env = arena.environments[env_id];
            if (env.name_id.has_value()) {
                format_segment(env.parent_id.value(), output);
                output.append(display(env.name_id.value()));
                output.append("::");
            }
        }

        auto display_env(db::Environment_id env_id) -> std::string
        {
            auto const& env = arena.environments[env_id];
            if (env.name_id.has_value()) {
                std::string path;
                format_segment(env.parent_id.value(), path);
                return std::format("in `{}{}`", path, display(env.name_id.value()));
            }
            return "at the module root";
        }

        auto display_info(std::string_view kind, auto const& info) -> std::string
        {
            return std::format(
                "# {} `{}`\n---\nDefined {}.",
                kind,
                display(info.name.id),
                display_env(info.env_id));
        }

        auto display(auto const& x) -> std::string
        {
            return hir::to_string(arena.hir, db.string_pool, x);
        }

        auto display(utl::String_id string_id) -> std::string_view
        {
            return db.string_pool.get(string_id);
        }

        auto operator()(db::Error)
        {
            return "# Error"s;
        }

        auto operator()(hir::Function_id id)
        {
            auto const& info     = arena.hir.functions[id];
            std::string markdown = display_info("Function", info);
            if (info.signature.has_value()) {
                std::format_to(
                    std::back_inserter(markdown),
                    "\nType: `{}`",
                    display(info.signature.value().function_type_id));
            }
            return markdown;
        }

        auto operator()(hir::Structure_id id)
        {
            return display_info("Structure", arena.hir.structures[id]);
        }

        auto operator()(hir::Enumeration_id id)
        {
            return display_info("Enumeration", arena.hir.enumerations[id]);
        }

        auto operator()(hir::Constructor_id id)
        {
            auto const& info = arena.hir.constructors[id];
            return std::format(
                "# Constructor `{}::{}`\n---\nDiscriminant: {}",
                display(info.owner_type_id),
                display(info.name.id),
                info.discriminant);
        }

        auto operator()(hir::Field_id id)
        {
            auto const& info = arena.hir.fields[id];
            return std::format(
                "# Field `{}`\n---\nType: `{}`", display(info.name.id), display(info.type_id));
        }

        auto operator()(hir::Concept_id id)
        {
            return display_info("Concept", arena.hir.concepts[id]);
        }

        auto operator()(hir::Alias_id id)
        {
            auto const& info     = arena.hir.aliases[id];
            std::string markdown = display_info("Type alias", info);
            if (info.hir.has_value()) {
                std::format_to(
                    std::back_inserter(markdown),
                    "\nAlias for `{}`",
                    display(info.hir.value().type_id));
            }
            return markdown;
        }

        auto operator()(hir::Module_id id)
        {
            return display_info("Module", arena.hir.modules[id]);
        }

        auto operator()(hir::Local_variable_id id)
        {
            auto const& local = arena.hir.local_variables[id];
            return std::format(
                "# Local variable `{}` `{}`\n---\nType: `{}`",
                display(local.mut_id),
                display(local.name.id),
                display(local.type_id));
        }

        auto operator()(hir::Local_mutability_id id)
        {
            auto const& local = arena.hir.local_mutabilities[id];
            return std::format("# Local mutability `{}`", display(local.name.id));
        }

        auto operator()(hir::Local_type_id id)
        {
            auto const& local = arena.hir.local_types[id];
            return std::format(
                "# Local type `{}` = `{}`", display(local.name.id), display(local.type_id));
        }
    };
} // namespace

auto ki::lsp::symbol_documentation(
    db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> std::string
{
    auto const& arena = db.documents[doc_id].arena;
    return std::visit(Visitor { .db = db, .arena = arena }, arena.symbols[symbol_id].variant);
}
