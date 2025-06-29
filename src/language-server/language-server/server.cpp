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
    };

    template <typename T>
    using Result = std::expected<T, std::string>;

    template <typename... Args>
    void debug_log(Server const& server, std::format_string<Args...> fmt, Args&&... args)
    {
        if (server.db.config.log_level == db::Log_level::Debug) {
            std::print(std::cerr, "[debug] ");
            std::println(std::cerr, fmt, std::forward<Args>(args)...);
        }
    }

    void publish_diagnostics(Server const& server, db::Document_id doc_id)
    {
        auto message = cpputil::json::encode(make_notification(
            Json::String("textDocument/publishDiagnostics"),
            diagnostic_params_to_json(server.db, doc_id)));
        debug_log(server, "<-- {}", message);
        rpc::write_message(server.output, message);
    }

    void analyze_document(Server& server, db::Document_id doc_id)
    {
        auto ctx = res::context(doc_id);

        server.db.documents[doc_id].info = {
            .diagnostics     = {},
            .semantic_tokens = {},
            .inlay_hints     = {},
            .references      = {},
            .actions         = {},
            .root_env_id     = ctx.root_env_id,
            .signature_info  = std::nullopt,
            .completion_info = std::nullopt,
        };

        auto symbol_ids = res::collect_document(server.db, ctx);

        for (db::Symbol_id symbol_id : symbol_ids) {
            res::resolve_symbol(server.db, ctx, symbol_id);
        }
        for (db::Symbol_id symbol_id : symbol_ids) {
            res::warn_if_unused(server.db, ctx, symbol_id);
        }

        server.db.documents[doc_id].arena = std::move(ctx.arena);
    }

    void update_edit_position(Server& server, db::Document_id doc_id, lsp::Position position)
    {
        auto& doc = server.db.documents[doc_id];
        if (doc.edit_position != position) {
            doc.edit_position = position;
            analyze_document(server, doc_id);
        }
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
        return references
             | std::views::filter([symbol_id](auto ref) { return ref.symbol_id == symbol_id; })
             | std::views::transform([](auto ref) { return ref.reference; });
    }

    auto handle_formatting(Server& server, Json params) -> Result<Json>
    {
        auto const [doc_id, options] = formatting_params_from_json(server.db, std::move(params));

        auto& doc = server.db.documents[doc_id];
        doc.info.diagnostics.clear();
        doc.info.semantic_tokens.clear();
        auto const cst = par::parse(server.db, doc_id);

        Json::Array edits;
        for (auto const& definition : cst.definitions) {
            std::string new_text;
            fmt::format(server.db.string_pool, doc.arena.cst, options, definition, new_text);
            auto text = db::text_range(doc.text, doc.arena.cst.ranges[definition.range]);
            if (new_text == text) {
                // Avoid sending redundant edits when nothing changed.
                // text_range takes linear time, but it's fine for now.
                continue;
            }
            Json::Object edit;
            edit.try_emplace("range", range_to_json(doc.arena.cst.ranges[definition.range]));
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

        return find_reference(references, position)
            .transform([&](db::Symbol_reference ref) {
                auto highlights = symbol_references(references, ref.symbol_id)
                                | std::views::transform(reference_to_json)
                                | std::ranges::to<Json::Array>();
                return Json { std::move(highlights) };
            })
            .value_or(Json {});
    }

    auto handle_completion(Server& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        update_edit_position(server, doc_id, position);

        return (server.db.documents[doc_id].info.completion_info)
            .transform(std::bind_front(completion_list_to_json, std::cref(server.db), doc_id))
            .value_or(Json {});
    }

    auto handle_references(Server const& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& references        = server.db.documents[doc_id].info.references;

        auto const make_location = [&](Reference ref) {
            return location_to_json(server.db, { .doc_id = doc_id, .range = ref.range });
        };

        return find_reference(references, position)
            .transform([&](db::Symbol_reference ref) {
                auto locations = symbol_references(references, ref.symbol_id)
                               | std::views::transform(make_location)
                               | std::ranges::to<Json::Array>();
                return Json { std::move(locations) };
            })
            .value_or(Json {});
    }

    auto handle_signature_help(Server& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        update_edit_position(server, doc_id, position);

        return (server.db.documents[doc_id].info.signature_info)
            .transform(std::bind_front(signature_help_to_json, std::cref(server.db), doc_id))
            .value_or(Json {});
    }

    auto handle_definition(Server const& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& doc               = server.db.documents[doc_id];

        return find_reference(doc.info.references, position)
            .transform([&](db::Symbol_reference ref) {
                lsp::Range range = doc.arena.symbols[ref.symbol_id].name.range;
                return location_to_json(server.db, { .doc_id = doc_id, .range = range });
            })
            .value_or(Json {});
    }

    auto handle_type_definition(Server const& server, Json params) -> Json
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& doc               = server.db.documents[doc_id];

        return find_reference(doc.info.references, position)
            .and_then([&](db::Symbol_reference ref) {
                return symbol_type(doc.arena, ref.symbol_id); //
            })
            .and_then([&](hir::Type_id type_id) {
                return type_definition(doc.arena, type_id); //
            })
            .transform([&](lsp::Range range) {
                return location_to_json(server.db, { .doc_id = doc_id, .range = range });
            })
            .value_or(Json {});
    }

    auto handle_hover(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        return find_reference(server.db.documents[doc_id].info.references, position)
            .transform([&](db::Symbol_reference ref) {
                std::string markdown = symbol_documentation(server.db, doc_id, ref.symbol_id);

                Json::Object object;
                object.try_emplace("contents", markdown_content_to_json(std::move(markdown)));
                return Json { std::move(object) };
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

    auto handle_symbols(Server const& server, Json params) -> Result<Json>
    {
        auto const doc_id = document_identifier_params_from_json(server.db, std::move(params));
        auto const env_id = server.db.documents[doc_id].info.root_env_id.value();
        return Json { environment_symbols(server.db, doc_id, env_id) };
    }

    auto handle_prepare_rename(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, position] = position_params_from_json(server.db, std::move(params));
        auto const& references        = server.db.documents[doc_id].info.references;

        return find_reference(references, position)
            .transform([&](db::Symbol_reference ref) -> Json {
                Json::Object object;
                object.try_emplace("range", range_to_json(ref.reference.range));
                return Json { std::move(object) };
            })
            .value_or(Json {});
    }

    auto handle_rename(Server const& server, Json params) -> Result<Json>
    {
        auto const [doc_id, position, text] = rename_params_from_json(server.db, std::move(params));
        auto const& references              = server.db.documents[doc_id].info.references;

        auto make_edit = [&](Reference ref) { return make_text_edit(ref.range, text); };

        return find_reference(references, position)
            .transform([&](db::Symbol_reference ref) {
                auto uri   = path_to_uri(db::document_path(server.db, doc_id));
                auto edits = symbol_references(references, ref.symbol_id)
                           | std::views::transform(make_edit) //
                           | std::ranges::to<Json::Array>();

                Json::Object changes;
                changes.try_emplace(std::move(uri), std::move(edits));

                Json::Object workspace_edit;
                workspace_edit.try_emplace("changes", std::move(changes));

                return Json { std::move(workspace_edit) };
            })
            .value_or(Json {});
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
            { "signatureHelpProvider",
              Json { Json::Object {
                  { "triggerCharacters", Json { Json::Array { Json { "(" }, Json { "," } } } },
                  { "retriggerCharacters", Json { Json::Array { Json { " " } } } },
              } } },
            {
                "completionProvider",
                Json { Json::Object {
                    { "triggerCharacters", Json { Json::Array { Json { "." }, Json { ":" } } } },
                } },
            },
            { "inlayHintProvider", Json { Json::Object {} } },
            { "codeActionProvider", Json { Json::Object {} } },
            { "hoverProvider", Json { true } },
            { "definitionProvider", Json { true } },
            { "typeDefinitionProvider", Json { true } },
            { "referencesProvider", Json { true } },
            { "documentSymbolProvider", Json { true } },
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
        if (method == "textDocument/completion") {
            return handle_completion(server, std::move(params));
        }
        if (method == "textDocument/inlayHint") {
            return handle_inlay_hints(server, std::move(params));
        }
        if (method == "textDocument/definition") {
            return handle_definition(server, std::move(params));
        }
        if (method == "textDocument/typeDefinition") {
            return handle_type_definition(server, std::move(params));
        }
        if (method == "textDocument/references") {
            return handle_references(server, std::move(params));
        }
        if (method == "textDocument/signatureHelp") {
            return handle_signature_help(server, std::move(params));
        }
        if (method == "textDocument/hover") {
            return handle_hover(server, std::move(params));
        }
        if (method == "textDocument/codeAction") {
            return handle_action(server, std::move(params));
        }
        if (method == "textDocument/documentSymbol") {
            return handle_symbols(server, std::move(params));
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
    void apply_content_change(db::Document& document, Json object)
    {
        document.edit_position = std::nullopt;

        auto change   = as<Json::Object>(std::move(object));
        auto new_text = as<Json::String>(at(change, "text"));
        if (auto field = maybe_at(change, "range")) {
            auto range = range_from_json(std::move(field).value());
            db::edit_text(document.text, range, new_text);

            // If the change is small, assume the user just typed some characters.
            if (not is_multiline(range) and new_text.size() < 5 and not new_text.contains('\n')) {
                document.edit_position = column_offset(range.start, new_text.size());
            }
        }
        else {
            document.text = std::move(new_text);
        }
    }

    auto handle_change(Server& server, Json params) -> Result<void>
    {
        auto object = as<Json::Object>(std::move(params));
        auto doc_id = document_identifier_from_json(server.db, at(object, "textDocument"));
        for (Json change : as<Json::Array>(at(object, "contentChanges"))) {
            apply_content_change(server.db.documents[doc_id], std::move(change));
        }
        analyze_document(server, doc_id);
        publish_diagnostics(server, doc_id);
        return {};
    }

    auto handle_change_config(Server& server, Json params) -> Result<void>
    {
        auto object      = as<Json::Object>(std::move(params));
        auto settings    = as<Json::Object>(at(object, "settings"));
        server.db.config = database_config_from_json(at(settings, "kieli"));
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
        if (method == "workspace/didChangeConfiguration") {
            return handle_change_config(server, std::move(params));
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

auto ki::lsp::run_server(db::Configuration config, std::istream& in, std::ostream& out) -> int
{
    Server server {
        .db             = db::database(std::move(config)),
        .exit_code      = std::nullopt,
        .input          = in,
        .output         = out,
        .is_initialized = false,
    };

    debug_log(server, "Starting server.");

    while (not server.exit_code.has_value()) {
        if (auto const message = rpc::read_message(server.input)) {
            debug_log(server, "--> {}", message.value());
            if (auto const reply = handle_client_message(server, message.value())) {
                debug_log(server, "<-- {}", reply.value());
                rpc::write_message(server.output, reply.value());
            }
        }
        else {
            std::println(std::cerr, "Unable to read message, exiting.");
            return EXIT_FAILURE;
        }
    }

    debug_log(server, "Stopping server.");

    return server.exit_code.value();
}

auto ki::lsp::default_server_config() -> db::Configuration
{
    return db::Configuration {
        .semantic_tokens = db::Semantic_token_mode::Full,
        .inlay_hints     = db::Inlay_hint_mode::Full,
        .references      = true,
        .code_actions    = true,
        .signature_help  = true,
        .code_completion = true,
        .diagnostics     = true,
    };
}
