#include <libutl/utilities.hpp>
#include <cpputil/json/decode.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/format.hpp>
#include <language-server/json.hpp>
#include <language-server/lsp.hpp>
#include <language-server/rpc.hpp>
#include <libcompiler/compiler.hpp>

#include <iostream>

namespace {
    auto run(bool const debug, std::istream& in, std::ostream& out) -> void
    {
        kieli::Database db;
        while (auto content = kieli::lsp::rpc_read_message(in)) {
            if (debug) {
                std::println(stderr, "[debug] received message: {}", content.value());
            }
            if (auto json = cpputil::json::decode<kieli::lsp::Json_config>(content.value())) {
                try {
                    if (auto result = kieli::lsp::handle_message(db, json.value().as_object())) {
                        std::string const reply = cpputil::json::encode(result.value());
                        if (debug) {
                            std::println(stderr, "[debug] replying with: {}", reply);
                        }
                        kieli::lsp::rpc_write_message(out, reply);
                    }
                }
                catch (kieli::lsp::Bad_client_json const& bad_json) {
                    // TODO: respond properly
                    std::println(stderr, "Bad client json: {}", bad_json.message);
                }
            }
            else {
                // TODO: respond properly
                std::println(stderr, "Failed to parse json content: {}", json.error());
            }
        }
    }
} // namespace

auto main(int const argc, char const* const* const argv) -> int
{
    try {
        bool const debug = std::find(argv, argv + argc, "--debug"sv) != (argv + argc);
        run(debug, std::cin, std::cout);
        return EXIT_SUCCESS;
    }
    catch (std::exception const& exception) {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
