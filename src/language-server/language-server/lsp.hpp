#pragma once

#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <cpputil/json.hpp>

namespace kieli::lsp {

    using Json = cpputil::json::Value;

    // Handle language client requests and notifications.
    // If `message` describes a request, returns a reply that should be sent to the client.
    [[nodiscard]] auto handle_message(Database& db, Json::Object const& message)
        -> std::optional<Json>;

} // namespace kieli::lsp
