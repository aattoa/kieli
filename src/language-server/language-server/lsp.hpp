#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <cpputil/json.hpp>

namespace kieli::lsp {

    struct Json_config {
        using Value   = cpputil::json::Basic_value<Json_config>;
        using Object  = std::unordered_map<std::string, Value>;
        using Array   = std::vector<Value>;
        using String  = std::string;
        using Number  = std::uint32_t;
        using Boolean = bool;
    };

    using Json = Json_config::Value;

    // Handle language client requests and notifications.
    // If `message` describes a request, returns a reply that should be sent to the client.
    [[nodiscard]] auto handle_message(Database& db, Json::Object const& message)
        -> std::optional<Json>;

} // namespace kieli::lsp
