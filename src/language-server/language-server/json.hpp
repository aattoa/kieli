#ifndef KIELI_LANGUAGE_SERVER_JSON
#define KIELI_LANGUAGE_SERVER_JSON

#include <libutl/utilities.hpp>
#include <cpputil/json.hpp>
#include <libcompiler/db.hpp>
#include <libformat/format.hpp>

namespace ki::lsp {

    struct Json_config {
        using Object = std::unordered_map<
            std::string,
            cpputil::json::Basic_value<Json_config>,
            utl::Transparent_hash<std::string_view>,
            std::equal_to<>>;
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

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocumentPositionParams
    struct Position_params {
        db::Document_id doc_id;
        Position        position;
    };

    // Common structure that works for InlayHintParams and CodeActionParams.
    struct Range_params {
        db::Document_id doc_id;
        Range           range;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#renameParams
    struct Rename_params {
        db::Document_id doc_id;
        Position        position;
        std::string     new_text;
    };

    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentFormattingParams
    struct Formatting_params {
        db::Document_id doc_id;
        fmt::Options    options;
    };

    // Thrown when the JSON sent by the client is syntactically correct but invalid in some way.
    struct Bad_json : std::exception {
        std::string message;
        explicit Bad_json(std::string message);
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    auto error_response(Error_code code, Json::String message, Json id) -> Json;
    auto success_response(Json result, Json id) -> Json;

    auto path_from_uri(std::string_view uri) -> std::filesystem::path;
    auto path_to_uri(std::filesystem::path const& path) -> std::string;

    auto position_from_json(Json json) -> Position;
    auto position_to_json(Position position) -> Json;

    auto range_from_json(Json json) -> Range;
    auto range_to_json(Range range) -> Json;

    auto document_identifier_from_json(db::Database const& db, Json json) -> db::Document_id;
    auto document_identifier_to_json(db::Database const& db, db::Document_id doc_id) -> Json;

    auto location_from_json(db::Database const& db, Json json) -> Location;
    auto location_to_json(db::Database const& db, Location location) -> Json;

    auto log_level_from_json(Json json) -> db::Log_level;
    auto semantic_token_mode_from_json(Json json) -> db::Semantic_token_mode;
    auto inlay_hint_mode_from_json(Json json) -> db::Inlay_hint_mode;
    auto database_config_from_json(Json json) -> db::Configuration;
    auto document_item_from_json(Json json) -> Document_item;
    auto format_options_from_json(Json json) -> fmt::Options;
    auto formatting_params_from_json(db::Database const& db, Json json) -> Formatting_params;
    auto position_params_from_json(db::Database const& db, Json json) -> Position_params;
    auto range_params_from_json(db::Database const& db, Json json) -> Range_params;
    auto rename_params_from_json(db::Database const& db, Json json) -> Rename_params;
    auto document_identifier_params_from_json(db::Database const& db, Json json) -> db::Document_id;

    auto severity_to_json(Severity severity) -> Json;
    auto markdown_content_to_json(std::string markdown) -> Json;
    auto semantic_tokens_to_json(std::span<Semantic_token const> tokens) -> Json;
    auto diagnostic_to_json(db::Database const& db, Diagnostic const& diagnostic) -> Json;
    auto diagnostic_params_to_json(db::Database const& db, db::Document_id doc_id) -> Json;
    auto hint_to_json(db::Database const& db, db::Document_id doc_id, db::Inlay_hint hint) -> Json;
    auto action_to_json(db::Database const& db, db::Document_id doc_id, db::Action action) -> Json;

    auto signature_help_to_json(
        db::Database const& db, db::Document_id doc_id, db::Signature_info const& info) -> Json;

    auto completion_list_to_json(
        db::Database const& db, db::Document_id doc_id, db::Completion_info const& info) -> Json;

    auto symbol_to_json(db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id)
        -> Json;

    auto completion_item_to_json(
        db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> Json;

    auto symbol_kind_to_json(db::Symbol_variant variant) -> Json;
    auto completion_item_kind_to_json(db::Symbol_variant variant) -> Json;

    auto reference_to_json(Reference reference) -> Json;
    auto reference_kind_to_json(Reference_kind kind) -> Json;

    auto environment_symbols(
        db::Database const& db, db::Document_id doc_id, db::Environment_id env_id) -> Json::Array;

    auto constructor_fields(
        db::Database const& db, db::Document_id doc_id, hir::Constructor_id ctor_id) -> Json::Array;

    auto make_text_edit(Range range, Json::String new_text) -> Json;

    auto make_notification(Json::String method, Json params) -> Json;

    // Throws `Bad_json` if `json` is not `T`.
    template <typename T>
    auto as(Json json) -> T
    {
        if (T* ptr = std::get_if<T>(&json.variant)) {
            return std::move(*ptr);
        }
        throw Bad_json("Value has unexpected type");
    }

    // Throws `Bad_json` if `json` is not a non-negative integer.
    auto as_unsigned(Json json) -> std::uint32_t;

    // If `object` contains `key`, moves out the value. Otherwise throws `Bad_json`.
    auto at(Json::Object& object, std::string_view key) -> Json;

    // If `object` contains `key`, moves out the value. Otherwise returns nullopt.
    auto maybe_at(Json::Object& object, std::string_view key) -> std::optional<Json>;

    template <typename T>
    auto maybe_at(Json::Object& object, std::string_view key) -> std::optional<T>
    {
        return maybe_at(object, key).transform([key](Json json) {
            if (T* ptr = std::get_if<T>(&json.variant)) {
                return std::move(*ptr);
            }
            throw Bad_json(std::format("Key '{}' has unexpected type", key));
        });
    }

} // namespace ki::lsp

#endif // KIELI_LANGUAGE_SERVER_JSON
