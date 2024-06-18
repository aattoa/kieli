#include <libutl/utilities.hpp>
#include <language-server/lsp.hpp>
#include <cpputil/json/encode.hpp>
#include <cpputil/json/format.hpp>

using Json   = kieli::lsp::Json;
using Result = std::expected<Json, std::string>;

namespace {
    auto handle_notification(std::string_view const method, Json const& params) -> void
    {
        (void)method;
        (void)params;
    }

    auto handle_initialize(Json const& /*params*/) -> Result
    {
        return Json { Json::Object {
            { "capabilities",
              Json { Json::Object {
                  { "hoverProvider", Json { true } },
              } } },
        } };
    }

    auto handle_hover(Json const& /*params*/) -> Result
    {
        return Json { Json::Object {
            { "contents", Json { "hello, Kieli!" } },
        } };
    }

    auto handle_request(std::string_view const method, Json const& params) -> Result
    {
        if (method == "initialize") {
            return handle_initialize(params);
        }
        if (method == "textDocument/hover") {
            return handle_hover(params);
        }
        return std::unexpected(std::format("Unsupported method: {}", method));
    }

    auto request_error(std::string message) -> Json
    {
        return Json { Json::Object {
            { "code", Json { 0. } },
            { "message", Json { std::move(message) } },
        } };
    }
} // namespace

auto kieli::lsp::handle_message(Json::Object const& message) -> std::optional<std::string>
{
    auto const& method = message.at("method").as_string();
    auto const& params = message.contains("params") ? message.at("params") : Json {};

    std::println(stderr, "handle_message: method = {}, params = {}", method, params);

    // Absence of message id means the client expects no reply.
    if (!message.contains("id")) {
        handle_notification(method, params);
        return std::nullopt;
    }

    Json reply { Json::Object {
        { "jsonrpc", Json { "2.0" } },
        { "id", message.at("id") },
    } };

    if (auto result = handle_request(method, params)) {
        reply.as_object()["result"] = std::move(result.value());
    }
    else {
        reply.as_object()["error"] = request_error(std::move(result.error()));
    }

    return cpputil::json::encode(reply);
}
