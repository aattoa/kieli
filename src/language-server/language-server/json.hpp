#pragma once

#include <language-server/lsp.hpp>

namespace kieli::lsp {

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
    // The communication loop will catch this and respond with an error.
    struct Bad_client_json : std::exception {
        std::string message;
        Bad_client_json(std::string message) noexcept;
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

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

    template <class T>
    auto at(Json::Object const& object, char const* const key) -> T const&
    {
        try {
            cpputil::always_assert(key != nullptr);
            return std::get<T>(object.at(key).variant);
        }
        catch (std::out_of_range const&) {
            throw Bad_client_json(std::format("Key not present: '{}'", key));
        }
        catch (std::bad_variant_access const&) {
            throw Bad_client_json(std::format("Key with unexpected type: '{}'", key));
        }
    }

} // namespace kieli::lsp
