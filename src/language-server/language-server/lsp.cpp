#include <libutl/utilities.hpp>
#include <cpputil/json/decode.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/format.hpp>
#include <language-server/json.hpp>
#include <language-server/server.hpp>

// TODO: remove
#include <libcompiler/cst/cst.hpp>
#include <libparse/parse.hpp>
#include <libformat/format.hpp>
#include <libresolve/resolve.hpp>

using kieli::Database;
using namespace kieli::lsp;

namespace {
    template <typename T>
    using Result = std::expected<T, std::string>;

    void placeholder_analyze_document(Database& db, kieli::Document_id id)
    {
        db.documents.at(id).diagnostics.clear();
        auto ctx = libresolve::context(db);
        auto env = libresolve::collect_document(ctx, id);
        libresolve::resolve_environment(ctx, env);
    }

    auto maybe_markdown_content(std::optional<std::string> markdown) -> Json
    {
        return std::move(markdown).transform(markdown_content_to_json).value_or(Json {});
    }

    auto handle_hover(Database& db, Json::Object const& params) -> Result<Json>
    {
        auto const cursor = character_location_from_json(db, params);
        return maybe_markdown_content(std::format(
            "Hello, world!\n\nFile: `{}`\nPosition: {}",
            db.paths[cursor.document_id].c_str(),
            cursor.position));
    }

    auto handle_formatting(Database& db, Json::Object const& params) -> Result<Json>
    {
        auto const id = document_id_from_json(db, as<Json::Object>(at(params, "textDocument")));
        auto const options = format_options_from_json(params.at("options").as_object());

        kieli::Document& document = db.documents.at(id);
        document.diagnostics.clear();
        auto const cst = kieli::parse(db, id);

        Json::Array edits;
        for (auto const& definition : cst.get().definitions) {
            std::string new_text;
            kieli::format(cst.get().arena, options, definition, new_text);
            if (new_text == kieli::text_range(document.text, definition.range)) {
                // Avoid sending redundant edits when nothing changed.
                // text_range takes linear time, but it's fine for now.
                continue;
            }
            edits.emplace_back(Json::Object {
                { "range", range_to_json(definition.range) },
                { "newText", Json { std::move(new_text) } },
            });
        }
        return Json { std::move(edits) };
    }

    auto handle_pull_diagnostics(Database& db, Json::Object const& params) -> Result<Json>
    {
        auto const id = document_id_from_json(db, as<Json::Object>(at(params, "textDocument")));

        auto items = db.documents.at(id).diagnostics
                   | std::views::transform(std::bind_front(diagnostic_to_json, std::cref(db)))
                   | std::ranges::to<Json::Array>();

        return Json { Json::Object {
            { "kind", Json { "full" } },
            { "items", Json { std::move(items) } },
        } };
    }

    auto encode_semantic_tokens(std::span<kieli::Semantic_token const> tokens) -> Json
    {
        Json::Array array;
        array.reserve(tokens.size() * 5); // Each token is represented by five integers.
        for (auto const& token : tokens) {
            array.emplace_back(utl::safe_cast<Json::Number>(token.delta_line));
            array.emplace_back(utl::safe_cast<Json::Number>(token.delta_column));
            array.emplace_back(utl::safe_cast<Json::Number>(token.length));
            array.emplace_back(utl::safe_cast<Json::Number>(std::to_underlying(token.type)));
            array.emplace_back(0); // token modifiers bitmask
        }
        return Json { std::move(array) };
    }

    auto handle_semantic_tokens(Database const& db, Json::Object const& params) -> Json
    {
        auto id   = document_id_from_json(db, as<Json::Object>(at(params, "textDocument")));
        auto data = encode_semantic_tokens(db.documents.at(id).semantic_tokens);
        return Json { Json::Object { { "data", std::move(data) } } };
    }

    auto handle_initialize() -> Json
    {
        // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentSyncKind
        static constexpr Json::Number incremental_sync = 2;

        // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#semanticTokensLegend
        Json semantic_tokens_legend { Json::Object {
            { "tokenTypes",
              Json { Json::Array {
                  Json { "comment" },
                  Json { "enumMember" },
                  Json { "enum" },
                  Json { "function" },
                  Json { "interface" },
                  Json { "keyword" },
                  Json { "method" },
                  Json { "namespace" },
                  Json { "number" },
                  Json { "operator" },
                  Json { "parameter" },
                  Json { "property" },
                  Json { "string" },
                  Json { "struct" },
                  Json { "type" },
                  Json { "typeParameter" },
                  Json { "variable" },
              } } },
            { "tokenModifiers", Json { Json::Array {} } },
        } };

        // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#initializeResult
        return Json { Json::Object {
            { "capabilities",
              Json { Json::Object {
                  { "textDocumentSync",
                    Json { Json::Object {
                        { "openClose", Json { true } },
                        { "change", Json { incremental_sync } },
                    } } },
                  { "diagnosticProvider",
                    Json { Json::Object {
                        { "interFileDependencies", Json { true } },
                        { "workspaceDiagnostics", Json { false } },
                    } } },
                  { "semanticTokensProvider",
                    Json { Json::Object {
                        { "legend", std::move(semantic_tokens_legend) },
                        { "full", Json { true } },
                    } } },
                  { "hoverProvider", Json { true } },
                  { "documentFormattingProvider", Json { true } },
              } } },
        } };
    }

    auto handle_shutdown(Server& server) -> Json
    {
        if (not std::exchange(server.is_initialized, false)) {
            std::println(stderr, "Received shutdown request while uninitialized");
        }
        server.db = Database {}; // Reset the compilation database.
        return Json {};
    }

    auto handle_request(Server& server, std::string_view const method, Json const& params)
        -> Result<Json>
    {
        if (method == "textDocument/semanticTokens/full") {
            return handle_semantic_tokens(server.db, params.as_object());
        }
        if (method == "textDocument/hover") {
            return handle_hover(server.db, params.as_object());
        }
        if (method == "textDocument/formatting") {
            return handle_formatting(server.db, params.as_object());
        }
        if (method == "textDocument/diagnostic") {
            return handle_pull_diagnostics(server.db, params.as_object());
        }
        if (method == "shutdown") {
            return handle_shutdown(server);
        }
        return std::unexpected(std::format("Unsupported request method: {}", method));
    }

    auto handle_open(Database& db, Json::Object const& params) -> Result<void>
    {
        auto document = document_item_from_json(as<Json::Object>(at(params, "textDocument")));
        if (document.language == "kieli") {
            auto const id = kieli::client_open_document(
                db, std::move(document.path), std::move(document.text));
            placeholder_analyze_document(db, id);
            return {};
        }
        return std::unexpected(std::format("Unsupported language: '{}'", document.language));
    }

    auto handle_close(Database& db, Json::Object const& params) -> Result<void>
    {
        auto const id = document_id_from_json(db, as<Json::Object>(at(params, "textDocument")));
        kieli::client_close_document(db, id);
        return {};
    }

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentContentChangeEvent
    void apply_content_change(std::string& text, Json::Object const& change)
    {
        std::string const& new_text = change.at("text").as_string();
        if (auto const it = change.find("range"); it != change.end()) {
            kieli::edit_text(text, range_from_json(it->second.as_object()), new_text);
        }
        else {
            text = new_text;
        }
    }

    auto handle_change(Database& db, Json::Object const& params) -> Result<void>
    {
        auto const   id   = document_id_from_json(db, as<Json::Object>(at(params, "textDocument")));
        std::string& text = db.documents.at(id).text;
        for (Json const& change : params.at("contentChanges").as_array()) {
            apply_content_change(text, change.as_object());
        }
        placeholder_analyze_document(db, id);
        return {};
    }

    auto handle_notification(Server& server, std::string_view const method, Json const& params)
        -> Result<void>
    {
        if (method == "textDocument/didChange") {
            return handle_change(server.db, params.as_object());
        }
        if (method == "textDocument/didOpen") {
            return handle_open(server.db, params.as_object());
        }
        if (method == "textDocument/didClose") {
            return handle_close(server.db, params.as_object());
        }
        if (method == "initialized") {
            return {};
        }
        if (method.starts_with("$/")) {
            return {};
        }
        return std::unexpected(std::format("Unsupported notification method: {}", method));
    }

    auto dispatch_handle_request(
        Server& server, std::string_view const method, Json const& params, Json const& id) -> Json
    {
        if (method == "initialize") {
            if (std::exchange(server.is_initialized, true)) {
                std::println(stderr, "Received duplicate initialize request");
            }
            return success_response(handle_initialize(), id);
        }
        else if (not server.is_initialized) {
            return error_response(Error_code::server_not_initialized, "Server not initialized", id);
        }
        else if (auto result = handle_request(server, method, params)) {
            return success_response(std::move(result).value(), id);
        }
        else {
            return error_response(Error_code::request_failed, std::move(result).error(), id);
        }
    }

    void dispatch_handle_notification(
        Server& server, std::string_view const method, Json const& params)
    {
        if (method == "exit") {
            // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#exit
            server.exit_code = server.is_initialized ? 1 : 0;
        }
        else if (not server.is_initialized) {
            std::println(stderr, "Server is uninitialized, dropping notification");
        }
        else {
            auto const result = handle_notification(server, method, params);
            if (not result.has_value()) {
                std::println(stderr, "Error while handling notification: {}", result.error());
            }
        }
    }

    auto message_params(Json::Object const& message) -> Json const&
    {
        if (auto const it = message.find("params"); it != message.end()) {
            return it->second;
        }
        static Json const null;
        return null;
    }

    auto parse_error_response(cpputil::json::Parse_error const error) -> Json
    {
        std::string message = std::format("Failed to parse JSON: {}", error);
        return error_response(Error_code::parse_error, std::move(message), Json {});
    }

    auto invalid_request_error_response(std::string_view const description, Json id) -> Json
    {
        std::string message = std::format("Invalid request object: {}", description);
        return error_response(Error_code::invalid_request, std::move(message), std::move(id));
    }

    auto invalid_params_error_response(std::string_view const description, Json id) -> Json
    {
        std::string message = std::format("Invalid method parameters: {}", description);
        return error_response(Error_code::invalid_params, std::move(message), std::move(id));
    }

    auto dispatch_handle_message_object(Server& server, Json const& message) -> std::optional<Json>
    {
        std::optional<Json> id;
        try {
            auto const& object = as<Json::Object>(message);
            id                 = maybe_at(object, "id");
            auto const& method = as<Json::String>(at(object, "method"));
            auto const& params = message_params(object);

            // If there is an id, the message is a request and the client expects a reply.
            // Otherwise, the message is a notification and the client does not expect a reply.

            try {
                if (id.has_value()) {
                    return dispatch_handle_request(server, method, params, id.value());
                }
                else {
                    dispatch_handle_notification(server, method, params);
                    return std::nullopt;
                }
            }
            catch (Bad_client_json const& bad_json) {
                return invalid_params_error_response(
                    bad_json.message, std::move(id).value_or(Json {}));
            }
        }
        catch (Bad_client_json const& bad_json) {
            return invalid_request_error_response(
                bad_json.message, std::move(id).value_or(Json {}));
        }
    }

    // https://www.jsonrpc.org/specification#batch
    auto dispatch_handle_message_batch(Server& server, Json::Array const& messages)
        -> std::optional<Json>
    {
        if (messages.empty()) {
            return invalid_request_error_response("Empty batch message", Json {});
        }
        Json::Array replies;
        for (Json const& message : messages) {
            if (auto reply = dispatch_handle_message_object(server, message)) {
                replies.push_back(std::move(reply).value());
            }
        }
        if (replies.empty()) {
            return std::nullopt; // The batch contained notifications only, do not reply.
        }
        return Json { std::move(replies) };
    }

    auto dispatch_handle_message(Server& server, Json const& message) -> std::optional<Json>
    {
        if (message.is_array()) {
            return dispatch_handle_message_batch(server, message.as_array());
        }
        else {
            return dispatch_handle_message_object(server, message);
        }
    }
} // namespace

auto kieli::lsp::handle_client_message(Server& server, std::string_view const message)
    -> std::optional<std::string>
{
    std::optional<Json> reply;

    if (auto json = cpputil::json::decode<Json_config>(message)) {
        reply = dispatch_handle_message(server, json.value());
    }
    else {
        reply = parse_error_response(json.error());
    }

    return reply.transform(cpputil::json::encode<Json_config>);
}
