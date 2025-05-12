#include <libutl/utilities.hpp>
#include <cpputil/json/decode.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/format.hpp>
#include <language-server/json.hpp>
#include <language-server/rpc.hpp>
#include <language-server/server.hpp>
#include <libformat/format.hpp>
#include <libparse/parse.hpp>
#include <libresolve/resolve.hpp>

using namespace ki;
using namespace ki::lsp;

namespace {
    struct Server {
        db::Database       db;
        std::optional<int> exit_code;
        std::istream&      input;
        std::ostream&      output;
        bool               is_initialized {};
        bool               debug {};
    };

    template <typename T>
    using Result = std::expected<T, std::string>;

    void publish_diagnostics(Server const& server, db::Document_id doc_id)
    {
        auto message = cpputil::json::encode(make_notification(
            Json::String("textDocument/publishDiagnostics"),
            diagnostic_params_to_json(server.db, doc_id)));
        if (server.debug) {
            std::println(std::cerr, "[debug] <-- {}", message);
        }
        rpc::write_message(server.output, message);
    }

    void analyze_document(Server& server, db::Document_id doc_id)
    {
        server.db.documents[doc_id].info = {};

        auto ctx        = res::context(doc_id);
        auto symbol_ids = res::collect_document(server.db, ctx);

        for (db::Symbol_id symbol_id : symbol_ids) {
            res::resolve_symbol(server.db, ctx, symbol_id);
        }
        for (db::Symbol_id symbol_id : symbol_ids) {
            res::warn_if_unused(server.db, ctx, symbol_id);
        }

        server.db.documents[doc_id].arena = std::move(ctx.arena);
    }

    auto find_reference(std::span<db::Symbol_reference const> references, Position position)
        -> std::optional<db::Symbol_reference>
    {
        // TODO: binary search
        auto const it = std::ranges::find_if(references, [=](db::Symbol_reference sym) {
            return range_contains(sym.reference.range, position);
        });
        return it != references.end() ? std::optional(*it) : std::nullopt;
    }

    auto symbol_references(
        std::span<db::Symbol_reference const> references, db::Symbol_id symbol_id)
    {
        return std::views::filter(references, [=](auto ref) { return ref.symbol_id == symbol_id; })
             | std::views::transform([](db::Symbol_reference ref) { return ref.reference; });
    }

    auto handle_formatting(Server& server, Json params) -> Result<Json>
    {
        auto const [doc_id, options] = formatting_params_from_json(server.db, std::move(params));

        auto& document = server.db.documents[doc_id];
        document.info.diagnostics.clear();
        document.info.semantic_tokens.clear();
        auto const cst = par::parse(server.db, doc_id);

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
            Json::Object edit;
            edit.try_emplace("range", range_to_json(document.arena.cst.ranges[definition.range]));
            edit.try_emplace("newText", std::move(new_text));
            edits.emplace_back(std::move(edit));
        }
        return Json { std::move(edits) };
    }

    auto handle_inlay_hints(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, range] = range_params_from_json(server.db, std::move(params));

        auto hints = server.db.documents[doc_id].info.inlay_hints
                   | std::views::filter(
                         [=](db::Inlay_hint hint) { return range_contains(range, hint.position); })
                   | std::views::transform(
                         [&](db::Inlay_hint hint) { return hint_to_json(server.db, doc_id, hint); })
                   | std::ranges::to<Json::Array>();

        return Json { std::move(hints) };
    }

    auto handle_semantic_tokens(Server const& server, Json params) -> Json
    {
        auto doc_id = document_identifier_params_from_json(server.db, std::move(params));
        auto data   = semantic_tokens_to_json(server.db.documents[doc_id].info.semantic_tokens);

        Json::Object result;
        result.try_emplace("data", std::move(data));
        return Json { std::move(result) };
    }

    auto handle_highlight(Server const& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& references        = server.db.documents[doc_id].info.references;

        if (auto ref = find_reference(references, position)) {
            auto highlights = symbol_references(references, ref.value().symbol_id)
                            | std::views::transform(reference_to_json)
                            | std::ranges::to<Json::Array>();
            return Json { std::move(highlights) };
        }

        return Json {};
    }

    auto handle_references(Server const& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& references        = server.db.documents[doc_id].info.references;

        auto const make_location = [&](Reference reference) {
            return location_to_json(server.db, { .doc_id = doc_id, .range = reference.range });
        };

        if (auto ref = find_reference(references, position)) {
            auto locations = symbol_references(references, ref.value().symbol_id)
                           | std::views::transform(make_location) //
                           | std::ranges::to<Json::Array>();
            return Json { std::move(locations) };
        }

        return Json {};
    }

    auto handle_definition(Server const& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& references        = server.db.documents[doc_id].info.references;

        if (auto ref = find_reference(references, position)) {
            auto locations = symbol_references(references, ref.value().symbol_id);

            auto it = std::ranges::find(locations, Reference_kind::Write, &Reference::kind);
            if (it != locations.end()) {
                return location_to_json(server.db, { .doc_id = doc_id, .range = (*it).range });
            }
        }

        return Json {};
    }

    auto handle_hover(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        return find_reference(server.db.documents[doc_id].info.references, position)
            .transform([&](db::Symbol_reference ref) {
                return markdown_content_to_json(
                    symbol_documentation(server.db, doc_id, ref.symbol_id));
            })
            .value_or(Json {});
    }

    auto handle_action(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, range] = range_params_from_json(server.db, std::move(params));

        auto actions = server.db.documents[doc_id].info.actions
                     | std::views::filter([&](db::Action const& action) {
                           return range_contains(action.range, range.start)
                               or range_contains(action.range, range.stop);
                       })
                     | std::views::transform([&](db::Action const& action) {
                           return action_to_json(server.db, doc_id, action);
                       })
                     | std::ranges::to<Json::Array>();

        return Json { std::move(actions) };
    }

    auto handle_prepare_rename(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& references        = server.db.documents[doc_id].info.references;
        if (auto ref = find_reference(references, position)) {
            Json::Object object;
            object.try_emplace("range", range_to_json(ref.value().reference.range));
            return Json { std::move(object) };
        }
        return Json {};
    }

    auto handle_rename(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, position, text] = rename_params_from_json(server.db, std::move(params));
        auto const& references              = server.db.documents[doc_id].info.references;

        auto make_edit = [&](Reference ref) { return make_text_edit(ref.range, text); };

        if (auto ref = find_reference(references, position)) {
            auto uri   = path_to_uri(db::document_path(server.db, doc_id));
            auto edits = symbol_references(references, ref.value().symbol_id)
                       | std::views::transform(make_edit) //
                       | std::ranges::to<Json::Array>();

            Json::Object changes;
            changes.try_emplace(std::move(uri), std::move(edits));

            Json::Object workspace_edit;
            workspace_edit.try_emplace("changes", std::move(changes));

            return Json { std::move(workspace_edit) };
        }

        return Json {};
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

        Json capabilities { Json::Object {
            { "textDocumentSync",
              Json { Json::Object {
                  { "openClose", Json { true } },
                  { "change", Json { incremental_sync } },
              } } },
            { "semanticTokensProvider",
              Json { Json::Object {
                  { "legend", std::move(semantic_tokens_legend) },
                  { "full", Json { true } },
              } } },
            { "renameProvider",
              Json { Json::Object {
                  { "prepareProvider", Json { true } },
              } } },
            { "inlayHintProvider", Json { Json::Object {} } },
            { "codeActionProvider", Json { Json::Object {} } },
            { "hoverProvider", Json { true } },
            { "definitionProvider", Json { true } },
            { "referencesProvider", Json { true } },
            { "documentHighlightProvider", Json { true } },
            { "documentFormattingProvider", Json { true } },
        } };

        Json info { Json::Object { { "name", Json { "kieli-language-server" } } } };

        return Json { Json::Object {
            { "capabilities", std::move(capabilities) },
            { "serverInfo", std::move(info) },
        } };
    }

    auto handle_shutdown(Server& server) -> Json
    {
        if (not std::exchange(server.is_initialized, false)) {
            std::println(std::cerr, "Received shutdown request while uninitialized");
        }
        server.db = db::Database {}; // Reset the compilation database.
        return Json {};
    }

    auto handle_request(Server& server, std::string_view const method, Json params) -> Result<Json>
    {
        if (method == "textDocument/semanticTokens/full") {
            return handle_semantic_tokens(server, std::move(params));
        }
        if (method == "textDocument/documentHighlight") {
            return handle_highlight(server, std::move(params));
        }
        if (method == "textDocument/inlayHint") {
            return handle_inlay_hints(server, std::move(params));
        }
        if (method == "textDocument/definition") {
            return handle_definition(server, std::move(params));
        }
        if (method == "textDocument/references") {
            return handle_references(server, std::move(params));
        }
        if (method == "textDocument/hover") {
            return handle_hover(server, std::move(params));
        }
        if (method == "textDocument/codeAction") {
            return handle_action(server, std::move(params));
        }
        if (method == "textDocument/prepareRename") {
            return handle_prepare_rename(server, std::move(params));
        }
        if (method == "textDocument/rename") {
            return handle_rename(server, std::move(params));
        }
        if (method == "textDocument/formatting") {
            return handle_formatting(server, std::move(params));
        }
        if (method == "shutdown") {
            return handle_shutdown(server);
        }
        return std::unexpected(std::format("Unsupported request method: {}", method));
    }

    auto handle_open(Server& server, Json params) -> Result<void>
    {
        auto object   = as<Json::Object>(std::move(params));
        auto document = document_item_from_json(at(object, "textDocument"));
        if (document.language == "kieli") {
            auto doc_id = db::client_open_document(
                server.db, std::move(document.path), std::move(document.text));
            analyze_document(server, doc_id);
            publish_diagnostics(server, doc_id);
            return {};
        }
        return std::unexpected(std::format("Unsupported language: '{}'", document.language));
    }

    auto handle_close(Server& server, Json params) -> Result<void>
    {
        auto doc_id = document_identifier_params_from_json(server.db, std::move(params));
        db::client_close_document(server.db, doc_id);
        return {};
    }

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentContentChangeEvent
    void apply_content_change(std::string& text, Json object)
    {
        auto change   = as<Json::Object>(std::move(object));
        auto new_text = as<Json::String>(at(change, "text"));
        if (auto field = maybe_at(change, "range")) {
            auto range = range_from_json(std::move(field).value());
            db::edit_text(text, range, new_text);
        }
        else {
            text = std::move(new_text);
        }
    }

    auto handle_change(Server& server, Json params) -> Result<void>
    {
        auto object = as<Json::Object>(std::move(params));
        auto doc_id = document_identifier_from_json(server.db, at(object, "textDocument"));
        for (Json change : as<Json::Array>(at(object, "contentChanges"))) {
            apply_content_change(server.db.documents[doc_id].text, std::move(change));
        }
        analyze_document(server, doc_id);
        publish_diagnostics(server, doc_id);
        return {};
    }

    auto handle_notification(Server& server, std::string_view method, Json params) -> Result<void>
    {
        if (method == "textDocument/didChange") {
            return handle_change(server, std::move(params));
        }
        if (method == "textDocument/didOpen") {
            return handle_open(server, std::move(params));
        }
        if (method == "textDocument/didClose") {
            return handle_close(server, std::move(params));
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
                std::println(std::cerr, "Received duplicate initialize request");
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
                std::println(std::cerr, "Error while handling notification: {}", result.error());
            }
        }
        else {
            std::println(std::cerr, "Server is uninitialized, dropping notification: {}", method);
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
            catch (Bad_json const& bad_json) {
                return invalid_params_error_response(
                    bad_json.message, std::move(id).value_or(Json {}));
            }
        }
        catch (Bad_json const& bad_json) {
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

    auto handle_client_message(Server& server, std::string_view message)
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
    Server server {
        .db             = db::Database {},
        .exit_code      = std::nullopt,
        .input          = in,
        .output         = out,
        .is_initialized = false,
        .debug          = debug,
    };

    if (debug) {
        std::println(std::cerr, "[debug] Started server.");
    }

    while (not server.exit_code.has_value()) {
        if (auto const message = rpc::read_message(server.input)) {
            if (debug) {
                std::println(std::cerr, "[debug] --> {}", message.value());
            }
            if (auto const reply = handle_client_message(server, message.value())) {
                if (server.debug) {
                    std::println(std::cerr, "[debug] <-- {}", reply.value());
                }
                rpc::write_message(server.output, reply.value());
            }
        }
        else {
            std::println(std::cerr, "Unable to read message, exiting.");
            return EXIT_FAILURE;
        }
    }

    if (debug) {
        std::println(std::cerr, "[debug] Exiting normally.");
    }

    return server.exit_code.value();
}
