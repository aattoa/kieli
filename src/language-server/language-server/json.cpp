#include <libutl/utilities.hpp>
#include <language-server/json.hpp>

ki::lsp::Bad_client_json::Bad_client_json(std::string message) : message(std::move(message)) {}

auto ki::lsp::Bad_client_json::what() const noexcept -> char const*
{
    return message.c_str();
}

auto ki::lsp::error_response(Error_code const code, Json::String message, Json id) -> Json
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

auto ki::lsp::success_response(Json result, Json id) -> Json
{
    return Json { Json::Object {
        { "jsonrpc", Json { "2.0" } },
        { "result", std::move(result) },
        { "id", std::move(id) },
    } };
}

auto ki::lsp::path_from_json(Json::String const& uri) -> std::filesystem::path
{
    if (uri.starts_with("file://")) {
        return uri.substr("file://"sv.size());
    }
    throw Bad_client_json { std::format("URI with unsupported scheme: '{}'", uri) };
}

auto ki::lsp::path_to_json(std::filesystem::path const& path) -> Json
{
    return Json { std::format("file://{}", path.c_str()) };
}

auto ki::lsp::position_from_json(Json::Object object) -> Position
{
    return Position {
        .line   = as_unsigned(at(object, "line")),
        .column = as_unsigned(at(object, "character")),
    };
}

auto ki::lsp::position_to_json(Position const position) -> Json
{
    return Json { Json::Object {
        { "line", Json { cpputil::num::safe_cast<Json::Number>(position.line) } },
        { "character", Json { cpputil::num::safe_cast<Json::Number>(position.column) } },
    } };
}

auto ki::lsp::range_from_json(Json::Object object) -> Range
{
    return Range {
        position_from_json(as<Json::Object>(at(object, "start"))),
        position_from_json(as<Json::Object>(at(object, "end"))),
    };
}

auto ki::lsp::range_to_json(Range const range) -> Json
{
    return Json { Json::Object {
        { "start", position_to_json(range.start) },
        { "end", position_to_json(range.stop) },
    } };
}

auto ki::lsp::document_id_from_json(db::Database const& db, Json::Object object) -> db::Document_id
{
    auto path = path_from_json(as<Json::String>(at(object, "uri")));
    if (auto const it = db.paths.find(path); it != db.paths.end()) {
        return it->second;
    }
    auto message = std::format("Referenced an unopened document: '{}'", path.c_str());
    throw Bad_client_json(std::move(message));
}

auto ki::lsp::document_id_to_json(db::Database const& db, db::Document_id const id) -> Json
{
    return Json { Json::Object { { "uri", path_to_json(document_path(db, id)) } } };
}

auto ki::lsp::location_from_json(db::Database const& db, Json::Object object) -> Location
{
    Range range = range_from_json(as<Json::Object>(at(object, "range")));
    return Location { .doc_id = document_id_from_json(db, std::move(object)), .range = range };
}

auto ki::lsp::location_to_json(db::Database const& db, Location const location) -> Json
{
    return Json { Json::Object {
        { "uri", path_to_json(document_path(db, location.doc_id)) },
        { "range", range_to_json(location.range) },
    } };
}

auto ki::lsp::position_params_from_json(db::Database const& db, Json::Object object)
    -> Position_params
{
    return Position_params {
        .doc_id   = document_id_from_json(db, as<Json::Object>(at(object, "textDocument"))),
        .position = position_from_json(as<Json::Object>(at(object, "position"))),
    };
}

auto ki::lsp::severity_to_json(Severity severity) -> Json
{
    switch (severity) {
    case Severity::Error:       return Json { Json::Number { 1 } };
    case Severity::Warning:     return Json { Json::Number { 2 } };
    case Severity::Information: return Json { Json::Number { 3 } };
    case Severity::Hint:        return Json { Json::Number { 4 } };
    default:                    cpputil::unreachable();
    }
}

auto ki::lsp::diagnostic_to_json(db::Database const& db, Diagnostic const& diagnostic) -> Json
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

auto ki::lsp::type_hint_to_json(db::Database const& db, db::Type_hint hint) -> Json
{
    auto const& hir   = db.documents[hint.doc_id].arena.hir;
    std::string label = hir::to_string(hir, db.string_pool, hir.types[hint.type]);
    label.insert(0, ": "sv);
    return Json { Json::Object {
        { "position", position_to_json(hint.position) },
        { "label", Json { std::move(label) } },
        { "kind", Json { 1 } }, // Type hint
    } };
}

auto ki::lsp::document_item_from_json(Json::Object object) -> Document_item
{
    return Document_item {
        .path     = path_from_json(as<Json::String>(at(object, "uri"))),
        .text     = as<Json::String>(at(object, "text")),
        .language = as<Json::String>(at(object, "languageId")),
        .version  = as_unsigned(at(object, "version")),
    };
}

auto ki::lsp::format_options_from_json(Json::Object object) -> fmt::Options
{
    return fmt::Options {
        .tab_size   = as_unsigned(at(object, "tabSize")),
        .use_spaces = as<Json::Boolean>(at(object, "insertSpaces")),
    };
}

auto ki::lsp::markdown_content_to_json(std::string markdown) -> Json
{
    return Json { Json::Object {
        { "contents",
          Json { Json::Object {
              { "kind", Json { "markdown" } },
              { "value", Json { std::move(markdown) } },
          } } },
    } };
}

auto ki::lsp::semantic_tokens_to_json(std::span<Semantic_token const> const tokens) -> Json
{
    Json::Array array;
    array.reserve(tokens.size() * 5); // Each token is represented by five integers.

    static constexpr auto to_num = [](auto n) { return cpputil::num::safe_cast<Json::Number>(n); };

    Position prev;
    for (Semantic_token const& token : tokens) {
        assert(token.length != 0);
        assert(prev.line <= token.position.line);
        assert(prev.line != token.position.line or prev.column <= token.position.column);
        if (token.position.line != prev.line) {
            prev.column = 0;
        }
        array.emplace_back(to_num(token.position.line - prev.line));
        array.emplace_back(to_num(token.position.column - prev.column));
        array.emplace_back(to_num(token.length));
        array.emplace_back(to_num(std::to_underlying(token.type)));
        array.emplace_back(0); // Token modifiers bitmask
        prev = token.position;
    }

    return Json { std::move(array) };
}

auto ki::lsp::as_unsigned(Json json) -> std::uint32_t
{
    if (auto const number = as<Json::Number>(std::move(json)); number >= 0) {
        return cpputil::num::safe_cast<std::uint32_t>(number);
    }
    throw Bad_client_json(std::format("Unexpected negative integer"));
}

auto ki::lsp::at(Json::Object& object, char const* const key) -> Json
{
    cpputil::always_assert(key != nullptr);
    if (auto const it = object.find(key); it != object.end()) {
        return std::move(it->second);
    }
    throw Bad_client_json(std::format("Key not present: '{}'", key));
}

auto ki::lsp::maybe_at(Json::Object& object, char const* const key) -> std::optional<Json>
{
    cpputil::always_assert(key != nullptr);
    auto const it = object.find(key);
    return it != object.end() ? std::optional(std::move(it->second)) : std::nullopt;
}
