#include <libutl/utilities.hpp>
#include <language-server/json.hpp>

namespace {
    auto document_id_from_path(kieli::Database const& db, std::filesystem::path const& path)
        -> kieli::Document_id
    {
        if (auto const document_id = kieli::find_existing_document_id(db, path)) {
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

auto kieli::lsp::error_response(Error_code const code, Json::String message, Json id) -> Json
{
    auto error = Json { Json::Object {
        { "code", Json { std::to_underlying(code) } },
        { "message", Json { std::move(message) } },
    } };
    return Json { Json::Object {
        { "jsonrpc", Json { "2.0" } },
        { "error", std::move(error) },
        { "id", std::move(id) },
    } };
}

auto kieli::lsp::success_response(Json result, Json id) -> Json
{
    return Json { Json::Object {
        { "jsonrpc", Json { "2.0" } },
        { "result", std::move(result) },
        { "id", std::move(id) },
    } };
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
        .line   = as_unsigned(at(object, "line")),
        .column = as_unsigned(at(object, "character")),
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
        position_from_json(as<Json::Object>(at(object, "start"))),
        position_from_json(as<Json::Object>(at(object, "end"))),
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
    return document_id_from_path(db, path_from_json(as<Json::String>(at(object, "uri"))));
}

auto kieli::lsp::document_id_to_json(Database const& db, Document_id const document_id) -> Json
{
    return Json { Json::Object { { "uri", path_to_json(db.paths[document_id]) } } };
}

auto kieli::lsp::location_from_json(Database const& db, Json::Object const& object) -> Location
{
    return Location {
        .document_id = document_id_from_json(db, object),
        .range       = range_from_json(as<Json::Object>(at(object, "range"))),
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
        .document_id = document_id_from_json(db, as<Json::Object>(at(object, "textDocument"))),
        .position    = position_from_json(as<Json::Object>(at(object, "position"))),
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

auto kieli::lsp::severity_to_json(Severity severity) -> Json
{
    switch (severity) {
    case Severity::error:       return Json { Json::Number { 1 } };
    case Severity::warning:     return Json { Json::Number { 2 } };
    case Severity::information: return Json { Json::Number { 3 } };
    case Severity::hint:        return Json { Json::Number { 4 } };
    default:                    cpputil::unreachable();
    }
}

auto kieli::lsp::diagnostic_to_json(Database const& db, Diagnostic const& diagnostic) -> Json
{
    auto const info_to_json = [&](Diagnostic_related const& info) {
        return Json { Json::Object {
            { "location", location_to_json(db, info.location) },
            { "message", Json { info.message } },
        } };
    };

    auto related = std::ranges::to<Json::Array>(
        std::views::transform(diagnostic.related_info, info_to_json));

    return Json { Json::Object {
        { "range", range_to_json(diagnostic.range) },
        { "severity", severity_to_json(diagnostic.severity) },
        { "message", Json { diagnostic.message } },
        { "relatedInformation", Json { std::move(related) } },
    } };
}

auto kieli::lsp::document_item_from_json(Json::Object const& object) -> Document_item
{
    return Document_item {
        .path     = path_from_json(as<Json::String>(at(object, "uri"))),
        .text     = as<Json::String>(at(object, "text")),
        .language = as<Json::String>(at(object, "languageId")),
        .version  = as_unsigned(at(object, "version")),
    };
}

auto kieli::lsp::format_options_from_json(Json::Object const& object) -> Format_options
{
    return Format_options {
        .tab_size   = as_unsigned(at(object, "tabSize")),
        .use_spaces = as<Json::Boolean>(at(object, "insertSpaces")),
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

auto kieli::lsp::as_unsigned(Json const& json) -> std::uint32_t
{
    if (auto const number = as<Json::Number>(json); number >= 0) {
        return static_cast<std::uint32_t>(number);
    }
    throw Bad_client_json(std::format("Unexpected negative integer"));
}

auto kieli::lsp::at(Json::Object const& object, char const* const key) -> Json const&
{
    cpputil::always_assert(key != nullptr);
    if (auto const it = object.find(key); it != object.end()) {
        return it->second;
    }
    throw Bad_client_json(std::format("Key not present: '{}'", key));
}

auto kieli::lsp::maybe_at(Json::Object const& object, char const* const key) -> std::optional<Json>
{
    cpputil::always_assert(key != nullptr);
    auto const it = object.find(key);
    return it != object.end() ? std::optional(it->second) : std::nullopt;
}
