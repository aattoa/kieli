#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli::query {

    // Get the `Source_id` corresponding to `path`.
    [[nodiscard]] auto source(Database& db, std::filesystem::path path)
        -> std::expected<Source_id, Read_failure>;

    // Get the concrete syntax tree corresponding to `source`.
    [[nodiscard]] auto cst(Database& db, Source_id source) -> std::optional<CST>;

    // Get the abstract syntax tree corresponding to `cst`.
    [[nodiscard]] auto ast(Database& db, CST const& cst) -> std::optional<AST>;

    // Get hover information for `position` formatted as markdown.
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover
    [[nodiscard]] auto hover(Database& db, Location location) -> std::optional<std::string>;

    // Get the definition location of the symbol at `position`.
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition
    [[nodiscard]] auto definition(Database& db, Location location) -> std::optional<Location>;

} // namespace kieli::query
