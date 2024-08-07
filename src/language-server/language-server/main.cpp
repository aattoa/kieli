#include <libutl/utilities.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/decode.hpp>
#include <cpputil/json/format.hpp>
#include <language-server/lsp.hpp>
#include <language-server/rpc.hpp>
#include <libcompiler/compiler.hpp>

#include <iostream>

namespace {
    auto run(std::istream& in, std::ostream& out) -> void
    {
        kieli::Database db;
        while (auto content = kieli::lsp::rpc_read_message(in)) {
            std::println(stderr, "received message: {}", content.value());
            if (auto json = cpputil::json::decode<kieli::lsp::Json_config>(content.value())) {
                if (auto result = kieli::lsp::handle_message(db, json.value().as_object())) {
                    std::string const reply = cpputil::json::encode(result.value());
                    std::println(stderr, "replying with: {}", reply);
                    kieli::lsp::rpc_write_message(out, reply);
                }
            }
            else {
                std::println(stderr, "Failed to parse json content: {}", json.error());
            }
        }
    }
} // namespace

auto main() -> int
{
    try {
        run(std::cin, std::cout);
        return EXIT_SUCCESS;
    }
    catch (std::exception const& exception) {
        std::println(stderr, "Caught exception: {}", exception.what());
        return EXIT_FAILURE;
    }
}
