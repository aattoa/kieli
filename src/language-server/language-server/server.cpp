#include <libutl/utilities.hpp>
#include <cpputil/json/decode.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/format.hpp>
#include <language-server/rpc.hpp>
#include <language-server/json.hpp>
#include <language-server/server.hpp>

// TODO: remove
#include <libparse/parse.hpp>
#include <libformat/format.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::lsp;

namespace {

    struct Server {
        db::Database       db;
        std::optional<int> exit_code;
        bool               is_initialized {};
    };

    template <typename T>
    using Result = std::expected<T, std::string>;

    void analyze_document(Server& server, db::Document_id doc_id)
    {
        server.db.documents[doc_id].info = {};

        auto ctx    = res::context();
        auto env_id = res::collect_document(server.db, ctx, doc_id);
        res::resolve_environment(server.db, ctx, env_id);

        server.db.documents[doc_id].arena.hir = std::move(ctx.hir);
    }

    auto handle_hover(Server& server, Json::Object params) -> Result<Json>
    {
        auto cursor = position_params_from_json(server.db, std::move(params));
        return markdown_content_to_json(std::format("Position: {}", cursor.position));
    }

    auto handle_formatting(Server& server, Json::Object params) -> Result<Json>
    {
        auto id = document_id_from_json(server.db, as<Json::Object>(at(params, "textDocument")));
        auto options = format_options_from_json(as<Json::Object>(at(params, "options")));

        auto& document = server.db.documents[id];

        document.info.diagnostics.clear();
        document.info.semantic_tokens.clear();
        auto const cst = par::parse(server.db, id);

        Json::Array edits;
        for (auto const& definition : cst.definitions) {
            std::string new_text;
            fmt::format(server.db.string_pool, document.arena.cst, options, definition, new_text);
            auto text = db::text_range(document.text, document.arena.cst.ranges[definition.range]);
            if (new_text == text) {
                // Avoid sending redundant edits when nothing changed.
                // text_range takes linear time, but it's fine for now.
                continue;
            }
            edits.emplace_back(Json::Object {
                { "range", range_to_json(document.arena.cst.ranges[definition.range]) },
                { "newText", Json { std::move(new_text) } },
            });
        }
        return Json { std::move(edits) };
    }

    auto handle_pull_diagnostics(Server& server, Json::Object params) -> Result<Json>
    {
        auto id = document_id_from_json(server.db, as<Json::Object>(at(params, "textDocument")));

        auto items = server.db.documents[id].info.diagnostics
                   | std::views::transform([&](lsp::Diagnostic const& diagnostic) {
                         return diagnostic_to_json(server.db, diagnostic);
                     })
                   | std::ranges::to<Json::Array>();

        return Json { Json::Object {
            { "kind", Json { "full" } },
            { "items", Json { std::move(items) } },
        } };
    }

    auto handle_inlay_hints(Server& server, Json::Object params) -> Result<Json>
    {
        auto id    = document_id_from_json(server.db, as<Json::Object>(at(params, "textDocument")));
        auto range = range_from_json(as<Json::Object>(at(params, "range")));

        auto hints = server.db.documents[id].info.type_hints
                   | std::views::filter(
                         [=](db::Type_hint hint) { return range_contains(range, hint.position); })
                   | std::views::transform(
                         [&](db::Type_hint hint) { return type_hint_to_json(server.db, hint); })
                   | std::ranges::to<Json::Array>();

        return Json { std::move(hints) };
    }

    auto handle_semantic_tokens(db::Database const& db, Json::Object params) -> Json
    {
        auto id   = document_id_from_json(db, as<Json::Object>(at(params, "textDocument")));
        auto data = semantic_tokens_to_json(db.documents[id].info.semantic_tokens);
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
                  { "inlayHintProvider",
                    Json { Json::Object {
                        { "resolveProvider", Json { false } },
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
        server.db = db::Database {}; // Reset the compilation database.
        return Json {};
    }

    auto handle_request(Server& server, std::string_view const method, Json params) -> Result<Json>
    {
        if (method == "textDocument/semanticTokens/full") {
            return handle_semantic_tokens(server.db, as<Json::Object>(std::move(params)));
        }
        if (method == "textDocument/hover") {
            return handle_hover(server, as<Json::Object>(std::move(params)));
        }
        if (method == "textDocument/formatting") {
            return handle_formatting(server, as<Json::Object>(std::move(params)));
        }
        if (method == "textDocument/diagnostic") {
            return handle_pull_diagnostics(server, as<Json::Object>(std::move(params)));
        }
        if (method == "textDocument/inlayHint") {
            return handle_inlay_hints(server, as<Json::Object>(std::move(params)));
        }
        if (method == "shutdown") {
            return handle_shutdown(server);
        }
        return std::unexpected(std::format("Unsupported request method: {}", method));
    }

    auto handle_open(Server& server, Json::Object params) -> Result<void>
    {
        auto document = document_item_from_json(as<Json::Object>(at(params, "textDocument")));
        if (document.language == "kieli") {
            auto const id = db::client_open_document(
                server.db, std::move(document.path), std::move(document.text));
            analyze_document(server, id);
            return {};
        }
        return std::unexpected(std::format("Unsupported language: '{}'", document.language));
    }

    auto handle_close(Server& server, Json::Object params) -> Result<void>
    {
        auto id = document_id_from_json(server.db, as<Json::Object>(at(params, "textDocument")));
        db::client_close_document(server.db, id);
        return {};
    }

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentContentChangeEvent
    void apply_content_change(std::string& text, Json::Object change)
    {
        auto new_text = as<Json::String>(at(change, "text"));
        if (auto field = maybe_at(change, "range")) {
            auto range = range_from_json(as<Json::Object>(std::move(field).value()));
            db::edit_text(text, range, new_text);
        }
        else {
            text = std::move(new_text);
        }
    }

    auto handle_change(Server& server, Json::Object params) -> Result<void>
    {
        auto id = document_id_from_json(server.db, as<Json::Object>(at(params, "textDocument")));
        for (Json change : as<Json::Array>(at(params, "contentChanges"))) {
            apply_content_change(server.db.documents[id].text, as<Json::Object>(std::move(change)));
        }
        analyze_document(server, id);
        return {};
    }

    auto handle_notification(Server& server, std::string_view method, Json params) -> Result<void>
    {
        if (method == "textDocument/didChange") {
            return handle_change(server, as<Json::Object>(std::move(params)));
        }
        if (method == "textDocument/didOpen") {
            return handle_open(server, as<Json::Object>(std::move(params)));
        }
        if (method == "textDocument/didClose") {
            return handle_close(server, as<Json::Object>(std::move(params)));
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
        Server& server, std::string_view const method, Json params, Json const& id) -> Json
    {
        if (method == "initialize") {
            if (std::exchange(server.is_initialized, true)) {
                std::println(stderr, "Received duplicate initialize request");
            }
            return success_response(handle_initialize(), id);
        }
        else if (not server.is_initialized) {
            return error_response(Error_code::Server_not_initialized, "Server not initialized", id);
        }
        else if (auto result = handle_request(server, method, std::move(params))) {
            return success_response(std::move(result).value(), id);
        }
        else {
            return error_response(Error_code::Request_failed, std::move(result).error(), id);
        }
    }

    void dispatch_handle_notification(Server& server, std::string_view const method, Json params)
    {
        if (method == "exit") {
            // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#exit
            server.exit_code = server.is_initialized ? 1 : 0;
        }
        else if (server.is_initialized) [[likely]] {
            auto const result = handle_notification(server, method, std::move(params));
            if (not result.has_value()) {
                std::println(stderr, "Error while handling notification: {}", result.error());
            }
        }
        else {
            std::println(stderr, "Server is uninitialized, dropping notification: {}", method);
        }
    }

    auto parse_error_response(cpputil::json::Parse_error const error) -> Json
    {
        std::string message = std::format("Failed to parse JSON: {}", error);
        return error_response(Error_code::Parse_error, std::move(message), Json {});
    }

    auto invalid_request_error_response(std::string_view const description, Json id) -> Json
    {
        std::string message = std::format("Invalid request object: {}", description);
        return error_response(Error_code::Invalid_request, std::move(message), std::move(id));
    }

    auto invalid_params_error_response(std::string_view const description, Json id) -> Json
    {
        std::string message = std::format("Invalid method parameters: {}", description);
        return error_response(Error_code::Invalid_params, std::move(message), std::move(id));
    }

    auto dispatch_handle_message_object(Server& server, Json message) -> std::optional<Json>
    {
        std::optional<Json> id;
        try {
            auto object = as<Json::Object>(std::move(message));
            id          = maybe_at(object, "id");
            auto method = as<Json::String>(at(object, "method"));
            auto params = maybe_at(object, "params").value_or(Json {});

            // If there is an id, the message is a request and the client expects a reply.
            // Otherwise, the message is a notification and the client does not expect a reply.

            try {
                if (id.has_value()) {
                    return dispatch_handle_request(server, method, std::move(params), id.value());
                }
                else {
                    dispatch_handle_notification(server, method, std::move(params));
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
    auto dispatch_handle_message_batch(Server& server, Json::Array messages) -> std::optional<Json>
    {
        if (messages.empty()) {
            return invalid_request_error_response("Empty batch message", Json {});
        }
        Json::Array replies;
        for (Json& message : messages) {
            if (auto reply = dispatch_handle_message_object(server, std::move(message))) {
                replies.push_back(std::move(reply).value());
            }
        }
        if (replies.empty()) {
            return std::nullopt; // The batch contained notifications only, do not reply.
        }
        return Json { std::move(replies) };
    }

    auto dispatch_handle_message(Server& server, Json message) -> std::optional<Json>
    {
        if (message.is_array()) {
            return dispatch_handle_message_batch(server, std::move(message).as_array());
        }
        else {
            return dispatch_handle_message_object(server, std::move(message));
        }
    }

    auto handle_client_message(Server& server, std::string_view const message)
        -> std::optional<std::string>
    {
        std::optional<Json> reply;

        if (auto json = cpputil::json::decode<Json_config>(message)) {
            reply = dispatch_handle_message(server, std::move(json).value());
        }
        else {
            reply = parse_error_response(json.error());
        }

        return reply.transform(cpputil::json::encode<Json_config>);
    }

} // namespace

auto ki::lsp::run_server(bool const debug, std::istream& in, std::ostream& out) -> int
{
    Server server;

    if (debug) {
        std::println(stderr, "[debug] Started server.");
    }

    while (not server.exit_code.has_value()) {
        if (auto const message = rpc::read_message(in)) {
            if (debug) {
                std::println(stderr, "[debug] --> {}", message.value());
            }
            if (auto const reply = handle_client_message(server, message.value())) {
                if (debug) {
                    std::println(stderr, "[debug] <-- {}", reply.value());
                }
                rpc::write_message(out, reply.value());
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
