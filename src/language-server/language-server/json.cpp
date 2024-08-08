#include <libutl/utilities.hpp>
#include <language-server/json.hpp>

namespace {
    auto document_id_from_path(kieli::Database const& db, std::filesystem::path const& path)
        -> kieli::Document_id
    {
        if (auto const document_id = kieli::find_document(db, path)) {
            return document_id.value();
        }
        throw kieli::lsp::Bad_client_json(
            std::format("Referenced an unopened document: '{}'", path.c_str()));
    }
} // namespace

kieli::lsp::Bad_client_json::Bad_client_json(std::string message) noexcept
    : message(std::move(message))
{}

auto kieli::lsp::Bad_client_json::what() const noexcept -> char const*
{
    return message.c_str();
}

auto kieli::lsp::path_from_json(Json::String const& uri) -> std::filesystem::path
{
    if (uri.starts_with("file://")) {
        return uri.substr("file://"sv.size());
    }
    throw Bad_client_json { std::format("URI with unsupported scheme: '{}'", uri) };
}

auto kieli::lsp::path_to_json(std::filesystem::path const& path) -> Json
{
    return Json { std::format("file://{}", path.c_str()) };
}

auto kieli::lsp::position_from_json(Json::Object const& object) -> Position
{
    return Position {
        .line   = at<Json::Number>(object, "line"),
        .column = at<Json::Number>(object, "character"),
    };
}

auto kieli::lsp::position_to_json(Position const position) -> Json
{
    return Json { Json::Object {
        { "line", Json { static_cast<Json::Number>(position.line) } },
        { "character", Json { static_cast<Json::Number>(position.column) } },
    } };
}

auto kieli::lsp::range_from_json(Json::Object const& object) -> Range
{
    return Range {
        position_from_json(at<Json::Object>(object, "start")),
        position_from_json(at<Json::Object>(object, "end")),
    };
}

auto kieli::lsp::range_to_json(Range const range) -> Json
{
    return Json { Json::Object {
        { "start", position_to_json(range.start) },
        { "end", position_to_json(range.stop) },
    } };
}

auto kieli::lsp::document_id_from_json(Database const& db, Json::Object const& object)
    -> Document_id
{
    return document_id_from_path(db, path_from_json(at<Json::String>(object, "uri")));
}

auto kieli::lsp::document_id_to_json(Database const& db, Document_id const document_id) -> Json
{
    return Json { Json::Object { { "uri", path_to_json(db.paths[document_id]) } } };
}

auto kieli::lsp::location_from_json(Database const& db, Json::Object const& object) -> Location
{
    return Location {
        .document_id = document_id_from_json(db, object),
        .range       = range_from_json(at<Json::Object>(object, "range")),
    };
}

auto kieli::lsp::location_to_json(Database const& db, Location const location) -> Json
{
    return Json { Json::Object {
        { "uri", path_to_json(db.paths[location.document_id]) },
        { "range", range_to_json(location.range) },
    } };
}

auto kieli::lsp::character_location_from_json(Database const& db, Json::Object const& object)
    -> Character_location
{
    return Character_location {
        .document_id = document_id_from_json(db, at<Json::Object>(object, "textDocument")),
        .position    = position_from_json(at<Json::Object>(object, "position")),
    };
}

auto kieli::lsp::character_location_to_json(Database const& db, Character_location const location)
    -> Json
{
    return Json { Json::Object {
        { "textDocument", document_id_to_json(db, location.document_id) },
        { "position", position_to_json(location.position) },
    } };
}

auto kieli::lsp::document_item_from_json(Json::Object const& object) -> Document_item
{
    return Document_item {
        .path     = path_from_json(at<Json::String>(object, "uri")),
        .text     = at<Json::String>(object, "text"),
        .language = at<Json::String>(object, "languageId"),
        .version  = at<Json::Number>(object, "version"),
    };
}

auto kieli::lsp::format_options_from_json(Json::Object const& object) -> Format_options
{
    return Format_options {
        .tab_size      = at<Json::Number>(object, "tabSize"),
        .insert_spaces = at<Json::Boolean>(object, "insertSpaces"),
    };
}

auto kieli::lsp::markdown_content_to_json(std::string markdown) -> Json
{
    return Json { Json::Object {
        { "contents",
          Json { Json::Object {
              { "kind", Json { "markdown" } },
              { "value", Json { std::move(markdown) } },
          } } },
    } };
}
