#ifndef KIELI_LANGUAGE_SERVER_JSON
#define KIELI_LANGUAGE_SERVER_JSON

#include <libutl/utilities.hpp>
#include <cpputil/json.hpp>
#include <libcompiler/db.hpp>
#include <libformat/format.hpp>

namespace ki::lsp {

    struct Json_config {
        using Object  = std::unordered_map<std::string, cpputil::json::Basic_value<Json_config>>;
        using Array   = std::vector<cpputil::json::Basic_value<Json_config>>;
        using String  = std::string;
        using Number  = std::int32_t;
        using Boolean = bool;
    };

    // The JSON type used for JSON-RPC communication.
    using Json = cpputil::json::Basic_value<Json_config>;

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#errorCodes
    enum struct Error_code : Json::Number {
        Server_not_initialized = -32002,
        Invalid_request        = -32600,
        Method_not_found       = -32601,
        Invalid_params         = -32602,
        Parse_error            = -32700,
        Request_failed         = -32803,
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
        explicit Bad_client_json(std::string message);
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    auto error_response(Error_code code, Json::String message, Json id) -> Json;
    auto success_response(Json result, Json id) -> Json;

    auto path_from_json(Json::String const& uri) -> std::filesystem::path;
    auto path_to_json(std::filesystem::path const& path) -> Json;

    auto position_from_json(Json::Object object) -> Position;
    auto position_to_json(Position position) -> Json;

    auto range_from_json(Json::Object object) -> Range;
    auto range_to_json(Range range) -> Json;

    auto document_id_from_json(db::Database const& db, Json::Object object) -> db::Document_id;
    auto document_id_to_json(db::Database const& db, db::Document_id document_id) -> Json;

    auto location_from_json(db::Database const& db, Json::Object object) -> Location;
    auto location_to_json(db::Database const& db, Location location) -> Json;

    auto severity_to_json(Severity severity) -> Json;
    auto markdown_content_to_json(std::string markdown) -> Json;
    auto semantic_tokens_to_json(std::span<Semantic_token const> tokens) -> Json;
    auto diagnostic_to_json(db::Database const& db, Diagnostic const& diagnostic) -> Json;

    auto document_item_from_json(Json::Object object) -> Document_item;
    auto format_options_from_json(Json::Object object) -> fmt::Options;
    auto position_params_from_json(db::Database const& db, Json::Object object) -> Position_params;

    // Throws `Bad_client_json` if `json` is not `T`.
    template <typename T>
    auto as(Json json) -> T
    {
        if (T* const ptr = std::get_if<T>(&json.variant)) {
            return std::move(*ptr);
        }
        throw Bad_client_json("Value has unexpected type");
    }

    // Throws `Bad_client_json` if `json` is not a non-negative integer.
    auto as_unsigned(Json json) -> std::uint32_t;

    // If `object` contains `key`, moves out the value. Otherwise throws `Bad_client_json`.
    auto at(Json::Object& object, char const* key) -> Json;

    // If `object` contains `key`, moves out the value. Otherwise returns nullopt.
    auto maybe_at(Json::Object& object, char const* key) -> std::optional<Json>;

} // namespace ki::lsp

#endif // KIELI_LANGUAGE_SERVER_JSON
