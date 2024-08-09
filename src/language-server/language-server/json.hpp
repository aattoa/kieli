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
        using Number  = std::int32_t;
        using Boolean = bool;
    };

    // The JSON type used for JSON-RPC communication.
    using Json = Json_config::Value;

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#errorCodes
    enum class Error_code : Json::Number {
        server_not_initialized = -32002,
        invalid_request        = -32600,
        method_not_found       = -32601,
        invalid_params         = -32602,
        parse_error            = -32700,
        request_failed         = -32803,
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#formattingOptions
    struct Format_options {
        std::size_t tab_size {};
        bool        insert_spaces {};
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentItem
    struct Document_item {
        std::filesystem::path path;
        std::string           text;
        std::string           language;
        std::size_t           version {};
    };

    // Thrown when the JSON sent by the client is syntactically correct but invalid in some way.
    struct Bad_client_json : std::exception {
        std::string message;
        Bad_client_json(std::string message) noexcept;
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    auto error_response(Error_code code, Json::String message, Json id) -> Json;
    auto success_response(Json result, Json id) -> Json;

    auto path_from_json(Json::String const& uri) -> std::filesystem::path;
    auto path_to_json(std::filesystem::path const& path) -> Json;

    auto position_from_json(Json::Object const& object) -> Position;
    auto position_to_json(Position position) -> Json;

    auto range_from_json(Json::Object const& object) -> Range;
    auto range_to_json(Range range) -> Json;

    auto document_id_from_json(Database const& db, Json::Object const& object) -> Document_id;
    auto document_id_to_json(Database const& db, Document_id document_id) -> Json;

    auto location_from_json(Database const& db, Json::Object const& object) -> Location;
    auto location_to_json(Database const& db, Location location) -> Json;

    auto character_location_from_json(Database const& db, Json::Object const& object)
        -> Character_location;
    auto character_location_to_json(Database const& db, Character_location location) -> Json;

    auto document_item_from_json(Json::Object const& object) -> Document_item;
    auto format_options_from_json(Json::Object const& object) -> Format_options;
    auto markdown_content_to_json(std::string markdown) -> Json;

    // Throws `Bad_client_json` if `json` is not `T`.
    template <class T>
    auto as(Json const& json) -> T const&
    {
        if (std::holds_alternative<T>(json.variant)) {
            return std::get<T>(json.variant);
        }
        throw Bad_client_json("Value has unexpected type");
    }

    // Throws `Bad_client_json` if `json` is not a non-negative integer.
    auto as_unsigned(Json const& json) -> std::uint32_t;

    // Throws `Bad_client_json` if `object` does not contain `key`.
    auto at(Json::Object const& object, char const* key) -> Json const&;

    // If `object` contains `key`, returns the value. Otherwise, returns nullopt.
    auto maybe_at(Json::Object const& object, char const* key) -> std::optional<Json>;

} // namespace kieli::lsp
