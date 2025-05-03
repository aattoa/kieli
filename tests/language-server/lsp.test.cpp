#include <libutl/utilities.hpp>
#include <cppunittest/unittest.hpp>
#include <cpputil/json.hpp>
#include <cpputil/json/decode.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/format.hpp>
#include <language-server/json.hpp>
#include <language-server/rpc.hpp>
#include <language-server/server.hpp>
#include <sstream>

using namespace ki;

UNITTEST("lifecycle")
{
    std::stringstream input;
    std::stringstream output;

    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","id":0,"method":"initialize"})");
    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","id":1,"method":"shutdown"})");
    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","method":"exit"})");

    REQUIRE_EQUAL(0, lsp::run_server(/*debug=*/false, input, output));
}

UNITTEST("premature exit")
{
    std::stringstream input;
    std::stringstream output;

    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","id":0,"method":"initialize"})");
    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","method":"exit"})");

    REQUIRE_EQUAL(1, lsp::run_server(/*debug=*/false, input, output));
}

UNITTEST("document synchronization")
{
    std::stringstream input;
    std::stringstream output;

    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","id":0,"method":"initialize"})");

    // Open a fake document with a syntax error.
    lsp::rpc::write_message(
        input,
        R"({
            "jsonrpc": "2.0",
            "method": "textDocument/didOpen",
            "params": {
                "textDocument": {
                    "uri": "file://test-uri",
                    "text": "fn _hello(): { 0.0 }",
                    "languageId": "kieli",
                    "version": 0
                }
            }
        })"sv);

    // Fix the syntax error.
    lsp::rpc::write_message(
        input,
        R"#({
            "jsonrpc": "2.0",
            "method": "textDocument/didChange",
            "params": {
                "textDocument": {
                    "uri": "file://test-uri",
                    "version": 1
                },
                "contentChanges": [{
                    "range": {
                        "start": { "line": 0, "character": 12 },
                        "end": { "line": 0, "character": 12 }
                    },
                    "text": " typeof(0.0)"
                }]
            }
        })#"sv);

    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","id":1,"method":"shutdown"})");
    lsp::rpc::write_message(input, R"({"jsonrpc":"2.0","method":"exit"})");

    // The server should exit normally.
    REQUIRE_EQUAL(0, lsp::run_server(/*debug=*/false, input, output));

    auto read_server_message = [&output] { return lsp::rpc::read_message(output).value(); };

    {
        // The server should reply to the initialize request.
        auto reply = cpputil::json::decode<lsp::Json_config>(read_server_message())
                         .value()
                         .as_object()
                         .at("result")
                         .as_object();
        REQUIRE(reply.contains("capabilities"));
        REQUIRE(reply.contains("serverInfo"));
    }

    {
        // The server should inform the client of the syntax error.
        auto const expectation = R"({
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
                "uri": "file://test-uri",
                "diagnostics": [{
                    "range": {
                        "start": { "line": 0, "character": 13 },
                        "end": { "line": 0, "character": 14 }
                    },
                    "severity": 1,
                    "message": "Expected a type, but found an opening brace"
                }]
            }
        })"sv;
        REQUIRE_EQUAL(
            cpputil::json::decode<lsp::Json_config>(expectation).value(),
            cpputil::json::decode<lsp::Json_config>(read_server_message()).value());
    }

    {
        // After the edit, the server should clear the diagnostics.
        auto const expectation = R"({
            "jsonrpc": "2.0",
            "method": "textDocument/publishDiagnostics",
            "params": {
                "uri": "file://test-uri",
                "diagnostics": []
            }
        })"sv;
        REQUIRE_EQUAL(
            cpputil::json::decode<lsp::Json_config>(expectation).value(),
            cpputil::json::decode<lsp::Json_config>(read_server_message()).value());
    }

    {
        // Finally, the server should reply to the shutdown request.
        auto const expectation = R"({"jsonrpc":"2.0","result":null,"id":1})"sv;
        REQUIRE_EQUAL(
            cpputil::json::decode<lsp::Json_config>(expectation).value(),
            cpputil::json::decode<lsp::Json_config>(read_server_message()).value());
    }

    REQUIRE(not lsp::rpc::read_message(output).has_value());
}
