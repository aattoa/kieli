#include <libutl/utilities.hpp>
#include <language-server/server.hpp>

using namespace ki;

namespace {
    struct Visitor {
        db::Database const& db;
        hir::Arena const&   hir;

        auto display(utl::String_id string_id)
        {
            return db.string_pool.get(string_id);
        }

        auto display(auto const& x)
        {
            return hir::to_string(hir, db.string_pool, x);
        }

        auto operator()(db::Error)
        {
            return "# Error"s;
        }

        auto operator()(hir::Function_id id)
        {
            auto const& info     = hir.functions[id];
            std::string markdown = std::format("# Function `{}`", display(info.name.id));
            if (info.signature.has_value()) {
                std::format_to(
                    std::back_inserter(markdown),
                    "\n---\nType: `{}`",
                    display(info.signature.value().function_type));
            }
            return markdown;
        }

        auto operator()(hir::Enumeration_id id)
        {
            auto const& info = hir.enumerations[id];
            return std::format("# Enum `{}`", display(info.name.id));
        }

        auto operator()(hir::Concept_id id)
        {
            auto const& info = hir.concepts[id];
            return std::format("# Concept `{}`", display(info.name.id));
        }

        auto operator()(hir::Alias_id id)
        {
            auto const& info     = hir.aliases[id];
            std::string markdown = std::format("# Type alias `{}`", display(info.name.id));
            if (info.hir.has_value()) {
                std::format_to(
                    std::back_inserter(markdown),
                    " = `{}`",
                    display(hir.aliases[id].hir.value().type));
            }
            return markdown;
        }

        auto operator()(hir::Module_id id)
        {
            return std::format("# Module `{}`", display(hir.modules[id].name.id));
        }

        auto operator()(hir::Local_variable_id id)
        {
            auto const& local = hir.local_variables[id];
            return std::format(
                "# Local variable `{}` `{}`\n---\nType: `{}`",
                display(local.mut_id),
                display(local.name.id),
                display(local.type_id));
        }

        auto operator()(hir::Local_mutability_id id)
        {
            auto const& local = hir.local_mutabilities[id];
            return std::format("# Local mutability `{}`", display(local.name.id));
        }

        auto operator()(hir::Local_type_id id)
        {
            auto const& local = hir.local_types[id];
            return std::format(
                "# Local type `{}` = `{}`", display(local.name.id), display(local.type_id));
        }
    };
} // namespace

auto ki::lsp::symbol_documentation(
    db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> std::string
{
    auto const& arena = db.documents[doc_id].arena;
    return std::visit(Visitor { .db = db, .hir = arena.hir }, arena.symbols[symbol_id].variant);
}
