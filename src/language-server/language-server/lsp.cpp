#include <libutl/utilities.hpp>
#include <language-server/lsp.hpp>
#include <libquery/query.hpp>
#include <libformat/format.hpp>
#include <cpputil/json/encode.hpp>

using kieli::Database;
using kieli::lsp::Json;
using kieli::query::Result;

namespace {
    auto uri_path(std::string_view uri) -> Result<std::filesystem::path>
    {
        if (uri.starts_with("file://")) {
            uri.remove_prefix("file://"sv.size());
            return uri;
        }
        return std::unexpected(std::format("URI with unsupported scheme: '{}'", uri));
    }

    auto document_source_id(Database& db, Json::Object const& document) -> Result<kieli::Source_id>
    {
        auto const query = [&](std::filesystem::path const& path) {
            return kieli::query::source(db, path); //
        };
        return uri_path(document.at("uri").as_string()).and_then(query);
    }

    auto position_from_json(Json::Object const& object) -> kieli::Position
    {
        return kieli::Position {
            .line   = object.at("line").as_number(),
            .column = object.at("character").as_number(),
        };
    }

    auto position_to_json(kieli::Position const position) -> Json
    {
        return Json { Json::Object {
            { "line", Json { static_cast<Json::Number>(position.line) } },
            { "character", Json { static_cast<Json::Number>(position.column) } },
        } };
    }

    auto position_params_from_json(Database& db, Json::Object const& params)
        -> Result<kieli::Location>
    {
        auto const position = position_from_json(params.at("position").as_object());
        auto       id       = document_source_id(db, params.at("textDocument").as_object());

        return std::move(id).transform([&](kieli::Source_id const source) {
            return kieli::Location { source, kieli::Range::for_position(position) };
        });
    }

    auto range_to_json(kieli::Range const range) -> Json
    {
        return Json { Json::Object {
            { "start", position_to_json(range.start) },
            { "end", position_to_json(range.stop) },
        } };
    }

    auto location_to_json(Database& db, kieli::Location const location) -> Json
    {
        auto const& path = db.sources[location.source].path;
        return Json { Json::Object {
            { "uri", Json { std::format("file://{}", path.c_str()) } },
            { "range", range_to_json(location.range) },
        } };
    }

    auto markdown_content_to_json(std::string markdown) -> Json
    {
        return Json { Json::Object {
            { "contents",
              Json { Json::Object {
                  { "kind", Json { "markdown" } },
                  { "value", Json { std::move(markdown) } },
              } } },
        } };
    }

    auto maybe_markdown_content(std::optional<std::string> markdown) -> Json
    {
        return std::move(markdown).transform(markdown_content_to_json).value_or(Json {});
    }

    auto document_format_edit(std::string new_text) -> Json
    {
        return Json { Json::Array {
            Json { Json::Object {
                // TODO: send proper whole-document range
                { "range", range_to_json(kieli::Range { {}, { 999999999 } }) },
                { "newText", Json { std::move(new_text) } },
            } },
        } };
    }

    auto handle_initialize(Database&, Json const& /*params*/) -> Result<Json>
    {
        return Json { Json::Object {
            { "capabilities",
              Json { Json::Object {
                  { "hoverProvider", Json { true } },
                  { "definitionProvider", Json { true } },
                  { "documentFormattingProvider", Json { true } },
              } } },
        } };
    }

    auto handle_hover(Database& db, Json::Object const& params) -> Result<Json>
    {
        return position_params_from_json(db, params).and_then([&](kieli::Location const cursor) {
            return kieli::query::hover(db, cursor).transform(maybe_markdown_content);
        });
    }

    auto handle_goto_definition(Database& db, Json::Object const& params) -> Result<Json>
    {
        return position_params_from_json(db, params)
            .and_then(
                [&](kieli::Location const cursor) { return kieli::query::definition(db, cursor); })
            .transform(
                [&](kieli::Location const location) { return location_to_json(db, location); });
    }

    auto handle_formatting(Database& db, Json::Object const& params) -> Result<Json>
    {
        return document_source_id(db, params.at("textDocument").as_object())
            .and_then([&](kieli::Source_id const source) {
                return kieli::query::cst(db, source); //
            })
            .transform([&](kieli::CST const& cst) {
                return kieli::format_module(*cst.module, kieli::Format_configuration {});
            })
            .transform(document_format_edit);
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

    auto handle_notification(Database&, std::string_view const method, Json const& params) -> void
    {
        (void)method;
        (void)params;
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

    std::println(stderr, "method = {}, params = {}", method, cpputil::json::encode(params));

    // Absence of message id means the client expects no reply.
    if (!message.contains("id")) {
        handle_notification(db, method, params);
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
