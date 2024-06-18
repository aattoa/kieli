#pragma once

#include <libutl/utilities.hpp>
#include <cpputil/json.hpp>

namespace kieli::lsp {

    using Json = cpputil::json::Value;

    [[nodiscard]] auto handle_message(Json::Object const& message) -> std::optional<std::string>;

} // namespace kieli::lsp
