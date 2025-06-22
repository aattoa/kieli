#ifndef KIELI_LANGUAGE_SERVER_SERVER
#define KIELI_LANGUAGE_SERVER_SERVER

#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>

namespace ki::lsp {

    // Run a language server with the given I/O streams.
    auto run_server(db::Configuration config, std::istream& in, std::ostream& out) -> int;

    // Default server database configuration.
    auto default_server_config() -> db::Configuration;

    // Get symbol documentation formatted as markdown.
    auto symbol_documentation(
        db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> std::string;

} // namespace ki::lsp

#endif // KIELI_LANGUAGE_SERVER_SERVER
