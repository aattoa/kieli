#ifndef KIELI_LANGUAGE_SERVER_SERVER
#define KIELI_LANGUAGE_SERVER_SERVER

#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>

namespace ki::lsp {

    // Run a language server with the given I/O streams.
    [[nodiscard]] auto run_server(bool debug, std::istream& in, std::ostream& out) -> int;

    // Get symbol documentation formatted as markdown.
    [[nodiscard]] auto symbol_documentation(
        db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> std::string;

} // namespace ki::lsp

#endif // KIELI_LANGUAGE_SERVER_SERVER
