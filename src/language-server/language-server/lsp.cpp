#include <libutl/utilities.hpp>
#include <language-server/lsp.hpp>
#include <libquery/query.hpp>
#include <cpputil/json/format.hpp>

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

    auto decode_position(Json::Object const& object) -> kieli::Position
    {
        return kieli::Position {
            .line   = static_cast<std::uint32_t>(object.at("line").as_number()),
            .column = static_cast<std::uint32_t>(object.at("character").as_number()),
        };
    }

    auto encode_position(kieli::Position const position) -> Json
    {
        return Json { Json::Object {
            { "line", Json { static_cast<Json::Number>(position.line) } },
            { "character", Json { static_cast<Json::Number>(position.column) } },
        } };
    }

    auto decode_position_params(Database& db, Json::Object const& params) -> Result<kieli::Location>
    {
        auto const& uri      = params.at("textDocument").as_object().at("uri").as_string();
        auto const& position = decode_position(params.at("position").as_object());

        return uri_path(uri)
            .and_then([&](std::filesystem::path const& path) {
                return kieli::query::source(db, path); //
            })
            .transform([&](kieli::Source_id const source) {
                return kieli::Location { source, kieli::Range::for_position(position) };
            });
    }

    auto encode_range(kieli::Range const range) -> Json
    {
        return Json { Json::Object {
            { "start", encode_position(range.start) },
            { "end", encode_position(range.stop) },
        } };
    }

    auto encode_location(Database& db, kieli::Location const location) -> Json
    {
        auto const& path = db.sources[location.source].path;
        return Json { Json::Object {
            { "uri", Json { std::format("file://{}", path.c_str()) } },
            { "range", encode_range(location.range) },
        } };
    }

    auto encode_markdown_content(std::string markdown) -> Json
    {
        return Json { Json::Object {
            { "contents",
              Json { Json::Object {
                  { "kind", Json { "markdown" } },
                  { "value", Json { std::move(markdown) } },
              } } },
        } };
    }

    auto handle_initialize(Database&, Json const& /*params*/) -> Result<Json>
    {
        return Json { Json::Object {
            { "capabilities",
              Json { Json::Object {
                  { "hoverProvider", Json { true } },
                  { "definitionProvider", Json { true } },
              } } },
        } };
    }

    auto handle_hover(Database& db, Json::Object const& params) -> Result<Json>
    {
        return decode_position_params(db, params)
            .and_then([&](kieli::Location const cursor) { return kieli::query::hover(db, cursor); })
            .transform(encode_markdown_content);
    }

    auto handle_goto_definition(Database& db, Json::Object const& params) -> Result<Json>
    {
        return decode_position_params(db, params)
            .and_then(
                [&](kieli::Location const cursor) { return kieli::query::definition(db, cursor); })
            .transform(
                [&](kieli::Location const location) { return encode_location(db, location); });
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
            { "code", Json { 0. } },
            { "message", Json { std::move(message) } },
        } };
    }
} // namespace

auto kieli::lsp::handle_message(Database& db, Json::Object const& message) -> std::optional<Json>
{
    auto const& method = message.at("method").as_string();
    auto const& params = message.contains("params") ? message.at("params") : Json {};

    std::println(stderr, "handle_message: method = {}, params = {}", method, params);

    // Absence of message id means the client expects no reply.
    if (!message.contains("id")) {
        handle_notification(db, method, params);
        return std::nullopt;
    }

    Json::Object reply {
        { "jsonrpc", Json { "2.0" } },
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
