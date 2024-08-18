#include <libutl/utilities.hpp>
#include <language-server/rpc.hpp>
#include <language-server/server.hpp>

auto kieli::lsp::run_server(bool const debug, std::istream& in, std::ostream& out) -> int
{
    Server server;

    if (debug) {
        std::println(stderr, "[debug] Started server.");
    }

    while (!server.exit_code.has_value()) {
        if (auto const message = kieli::lsp::rpc_read_message(in)) {
            if (debug) {
                std::println(stderr, "[debug] --> {}", message.value());
            }
            if (auto const reply = kieli::lsp::handle_client_message(server, message.value())) {
                if (debug) {
                    std::println(stderr, "[debug] <-- {}", reply.value());
                }
                kieli::lsp::rpc_write_message(out, reply.value());
            }
        }
        else {
            std::println(stderr, "Unable to read message, exiting.");
            return EXIT_FAILURE;
        }
    }

    if (debug) {
        std::println(stderr, "[debug] Exiting normally.");
    }

    return server.exit_code.value();
}
