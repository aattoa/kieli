#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

namespace kieli::lsp {

    struct Server {
        Database           db;
        std::optional<int> exit_code;
        bool               is_initialized {};
    };

    // If `message` describes a request, returns a reply that should be sent to the client.
    [[nodiscard]] auto handle_client_message(Server& server, std::string_view message)
        -> std::optional<std::string>;

} // namespace kieli::lsp
