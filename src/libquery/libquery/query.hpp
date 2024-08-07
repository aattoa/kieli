#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <libcompiler/tree_fwd.hpp>

namespace kieli::query {

    template <class T>
    using Result = std::expected<T, std::string>;

    // Get the `Document_id` corresponding to `path`.
    [[nodiscard]] auto document_id(Database& db, std::filesystem::path const& path)
        -> Result<Document_id>;

    // Get the concrete syntax tree corresponding to `source`.
    [[nodiscard]] auto cst(Database& db, Document_id source) -> Result<CST>;

    // Get the abstract syntax tree corresponding to `cst`.
    [[nodiscard]] auto ast(Database& db, CST const& cst) -> Result<AST>;

    // Get hover information for `position` formatted as markdown.
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover
    [[nodiscard]] auto hover(Database& db, Location location) -> Result<std::optional<std::string>>;

    // Get the definition location of the symbol at `position`.
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition
    [[nodiscard]] auto definition(Database& db, Location location) -> Result<Location>;

} // namespace kieli::query
