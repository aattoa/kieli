#include <libutl/utilities.hpp>
#include <language-server/lsp.hpp>
#include <language-server/json.hpp>
#include <libquery/query.hpp>
#include <libformat/format.hpp>
#include <cpputil/json/encode.hpp>

using kieli::Database;
using kieli::query::Result;
using namespace kieli::lsp;

namespace {
    auto format_config_from_options(Format_options const& options) -> kieli::Format_configuration
    {
        return kieli::Format_configuration {
            .tab_size   = options.tab_size,
            .use_spaces = options.insert_spaces,
        };
    };

    auto maybe_markdown_content(std::optional<std::string> markdown) -> Json
    {
        return std::move(markdown).transform(markdown_content_to_json).value_or(Json {});
    }

    auto document_format_edit(std::string new_text) -> Json
    {
        // TODO: send one edit per definition
        return Json { Json::Array {
            Json { Json::Object {
                { "range", range_to_json(kieli::Range { {}, { 999999999 } }) },
                { "newText", Json { std::move(new_text) } },
            } },
        } };
    }

    auto handle_hover(Database& db, Json::Object const& params) -> Result<Json>
    {
        auto const cursor = character_location_from_json(db, params);
        return kieli::query::hover(db, cursor).transform(maybe_markdown_content);
    }

    auto handle_goto_definition(Database& db, Json::Object const& params) -> Result<Json>
    {
        auto const cursor = character_location_from_json(db, params);
        return kieli::query::definition(db, cursor).transform([&](kieli::Location const location) {
            return location_to_json(db, location);
        });
    }

    auto handle_formatting(Database& db, Json::Object const& params) -> Result<Json>
    {
        auto const document_id
            = document_id_from_json(db, at<Json::Object>(params, "textDocument"));

        return kieli::query::cst(db, document_id)
            .transform([&](kieli::CST const& cst) {
                auto const options = format_options_from_json(params.at("options").as_object());
                return kieli::format_module(cst.get(), format_config_from_options(options));
            })
            .transform(document_format_edit);
    }

    auto handle_initialize(Database&, Json const&) -> Result<Json>
    {
        // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentSyncKind
        static constexpr Json::Number incremental_sync = 2;

        return Json { Json::Object {
            { "capabilities",
              Json { Json::Object {
                  { "hoverProvider", Json { true } },
                  { "definitionProvider", Json { true } },
                  { "documentFormattingProvider", Json { true } },
                  { "textDocumentSync",
                    Json { Json::Object {
                        { "openClose", Json { true } },
                        { "change", Json { incremental_sync } },
                    } } },
              } } },
        } };
    }

    auto handle_open(Database& db, Json::Object const& params) -> Result<void>
    {
        auto document = document_item_from_json(at<Json::Object>(params, "textDocument"));
        if (document.language == "kieli") {
            (void)kieli::add_document(
                db,
                std::move(document.path),
                std::move(document.text),
                kieli::Document_ownership::client);
            return {};
        }
        return std::unexpected(std::format("Unsupported language: '{}'", document.language));
    }

    auto handle_close(Database& db, Json::Object const& params) -> Result<void>
    {
        auto const document_id
            = document_id_from_json(db, at<Json::Object>(params, "textDocument"));

        if (db.documents.erase(document_id) == 1) {
            return {};
        }
        return std::unexpected("Attempted to close an unopened document");
    }

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentContentChangeEvent
    auto apply_content_change(std::string& text, Json::Object const& change) -> void
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
        auto const document_id
            = document_id_from_json(db, at<Json::Object>(params, "textDocument"));

        std::string& text = kieli::document(db, document_id).text;
        for (Json const& change : params.at("contentChanges").as_array()) {
            apply_content_change(text, change.as_object());
        }
        return {};
    }

    auto handle_request(Database& db, std::string_view const method, Json const& params)
        -> Result<Json>
    {
        if (method == "initialize") {
            return handle_initialize(db, params);
        }
        if (method == "textDocument/hover") {
            return handle_hover(db, params.as_object());
        }
        if (method == "textDocument/definition") {
            return handle_goto_definition(db, params.as_object());
        }
        if (method == "textDocument/formatting") {
            return handle_formatting(db, params.as_object());
        }
        return std::unexpected(std::format("Unsupported method: {}", method));
    }

    auto handle_notification(Database& db, std::string_view const method, Json const& params)
        -> Result<void>
    {
        if (method == "textDocument/didOpen") {
            return handle_open(db, params.as_object());
        }
        if (method == "textDocument/didClose") {
            return handle_close(db, params.as_object());
        }
        if (method == "textDocument/didChange") {
            return handle_change(db, params.as_object());
        }
        return std::unexpected(std::format("Unsupported method: {}", method));
    }

    auto request_error(std::string message) -> Json
    {
        return Json { Json::Object {
            { "code", Json { Json::Number {} } },
            { "message", Json { std::move(message) } },
        } };
    }
} // namespace

auto kieli::lsp::handle_message(Database& db, Json::Object const& message) -> std::optional<Json>
{
    auto const& method = message.at("method").as_string();
    auto const& params = message.contains("params") ? message.at("params") : Json {};

    // Absence of message id means the client expects no reply.
    if (!message.contains("id")) {
        if (auto const result = handle_notification(db, method, params); !result) {
            std::println(stderr, "Notification failed with error: {}", result.error());
        }
        return std::nullopt;
    }

    Json::Object reply {
        { "jsonrpc", message.at("jsonrpc") },
        { "id", message.at("id") },
    };

    if (auto result = handle_request(db, method, params)) {
        reply["result"] = std::move(result.value());
    }
    else {
        reply["error"] = request_error(std::move(result.error()));
    }

    return Json { std::move(reply) };
}
