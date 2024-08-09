#include <libutl/utilities.hpp>
#include <language-server/rpc.hpp>
#include <language-server/server.hpp>

#include <iostream>

auto main(int const argc, char const* const* const argv) -> int
{
    bool const debug = std::find(argv, argv + argc, "--debug"sv) != (argv + argc);

    kieli::lsp::Server server;

    try {
        if (debug) {
            std::println(stderr, "[debug] Started server.");
        }

        while (!server.exit_code.has_value()) {
            if (auto const message = kieli::lsp::rpc_read_message(std::cin)) {
                if (debug) {
                    std::println(stderr, "[debug] Received message: {}", message.value());
                }
                if (auto const reply = kieli::lsp::handle_client_message(server, message.value())) {
                    if (debug) {
                        std::println(stderr, "[debug] Replying with: {}", reply.value());
                    }
                    kieli::lsp::rpc_write_message(std::cout, reply.value());
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
    catch (std::exception const& exception) {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
