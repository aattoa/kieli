#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <iosfwd>

namespace kieli::lsp {

    struct Server {
        Database           db;
        std::optional<int> exit_code;
        bool               is_initialized {};
    };

    // If `message` describes a request, returns a reply that should be sent to the client.
    [[nodiscard]] auto handle_client_message(Server& server, std::string_view message)
        -> std::optional<std::string>;

    // Run a language server with the given I/O streams.
    [[nodiscard]] auto run_server(bool debug, std::istream& in, std::ostream& out) -> int;

} // namespace kieli::lsp
