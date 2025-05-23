#include <libutl/utilities.hpp>
#include <language-server/json.hpp>

ki::lsp::Bad_json::Bad_json(std::string message) : message(std::move(message)) {}

auto ki::lsp::Bad_json::what() const noexcept -> char const*
{
    return message.c_str();
}

auto ki::lsp::error_response(Error_code code, Json::String message, Json id) -> Json
{
    Json::Object error;
    error.try_emplace("code", std::to_underlying(code));
    error.try_emplace("message", std::move(message));

    Json::Object object;
    object.try_emplace("jsonrpc", "2.0");
    object.try_emplace("error", std::move(error));
    object.try_emplace("id", std::move(id));
    return Json { std::move(object) };
}

auto ki::lsp::success_response(Json result, Json id) -> Json
{
    Json::Object object;
    object.try_emplace("jsonrpc", "2.0");
    object.try_emplace("result", std::move(result));
    object.try_emplace("id", std::move(id));
    return Json { std::move(object) };
}

auto ki::lsp::path_to_uri(std::filesystem::path const& path) -> std::string
{
    return std::format("file://{}", path.c_str());
}

auto ki::lsp::path_from_uri(std::string_view uri) -> std::filesystem::path
{
    if (uri.starts_with("file://")) {
        return uri.substr("file://"sv.size());
    }
    throw Bad_json(std::format("URI with unsupported scheme: '{}'", uri));
}

auto ki::lsp::position_from_json(Json json) -> Position
{
    auto object = as<Json::Object>(std::move(json));
    return Position {
        .line   = as_unsigned(at(object, "line")),
        .column = as_unsigned(at(object, "character")),
    };
}

auto ki::lsp::position_to_json(Position position) -> Json
{
    Json::Object result;
    result.try_emplace("line", cpputil::num::safe_cast<Json::Number>(position.line));
    result.try_emplace("character", cpputil::num::safe_cast<Json::Number>(position.column));
    return Json { std::move(result) };
}

auto ki::lsp::range_from_json(Json json) -> Range
{
    auto object = as<Json::Object>(std::move(json));
    auto start  = position_from_json(at(object, "start"));
    auto end    = position_from_json(at(object, "end"));
    return Range(start, end);
}

auto ki::lsp::range_to_json(Range range) -> Json
{
    Json::Object result;
    result.try_emplace("start", position_to_json(range.start));
    result.try_emplace("end", position_to_json(range.stop));
    return Json { std::move(result) };
}

auto ki::lsp::document_identifier_from_json(db::Database const& db, Json json) -> db::Document_id
{
    auto object = as<Json::Object>(std::move(json));
    auto path   = path_from_uri(as<Json::String>(at(object, "uri")));
    if (auto const it = db.paths.find(path); it != db.paths.end()) {
        return it->second;
    }
    throw Bad_json(std::format("Referenced an unopened document: '{}'", path.c_str()));
}

auto ki::lsp::document_identifier_to_json(db::Database const& db, db::Document_id doc_id) -> Json
{
    Json::Object object;
    object.try_emplace("uri", path_to_uri(document_path(db, doc_id)));
    return Json { std::move(object) };
}

auto ki::lsp::location_from_json(db::Database const& db, Json json) -> Location
{
    auto object = as<Json::Object>(std::move(json));
    auto range  = range_from_json(at(object, "range"));
    return Location {
        .doc_id = document_identifier_from_json(db, Json { std::move(object) }),
        .range  = range,
    };
}

auto ki::lsp::location_to_json(db::Database const& db, Location location) -> Json
{
    Json::Object object;
    object.try_emplace("uri", path_to_uri(document_path(db, location.doc_id)));
    object.try_emplace("range", range_to_json(location.range));
    return Json { std::move(object) };
}

auto ki::lsp::position_params_from_json(db::Database const& db, Json json) -> Position_params
{
    auto object = as<Json::Object>(std::move(json));
    return Position_params {
        .doc_id   = document_identifier_from_json(db, at(object, "textDocument")),
        .position = position_from_json(at(object, "position")),
    };
}

auto ki::lsp::range_params_from_json(db::Database const& db, Json json) -> Range_params
{
    auto object = as<Json::Object>(std::move(json));
    return Range_params {
        .doc_id = document_identifier_from_json(db, at(object, "textDocument")),
        .range  = range_from_json(at(object, "range")),
    };
}

auto ki::lsp::rename_params_from_json(db::Database const& db, Json json) -> Rename_params
{
    auto object = as<Json::Object>(std::move(json));
    return Rename_params {
        .doc_id   = document_identifier_from_json(db, at(object, "textDocument")),
        .position = position_from_json(at(object, "position")),
        .new_text = as<Json::String>(at(object, "newName")),
    };
}

auto ki::lsp::document_identifier_params_from_json(db::Database const& db, Json json)
    -> db::Document_id
{
    auto object = as<Json::Object>(std::move(json));
    return document_identifier_from_json(db, at(object, "textDocument"));
}

auto ki::lsp::severity_to_json(Severity severity) -> Json
{
    switch (severity) {
    case Severity::Error:       return Json { Json::Number { 1 } };
    case Severity::Warning:     return Json { Json::Number { 2 } };
    case Severity::Information: return Json { Json::Number { 3 } };
    case Severity::Hint:        return Json { Json::Number { 4 } };
    }
    cpputil::unreachable();
}

auto ki::lsp::diagnostic_to_json(db::Database const& db, Diagnostic const& diagnostic) -> Json
{
    auto info_to_json = [&](Diagnostic_related const& info) {
        Json::Object object;
        object.try_emplace("location", location_to_json(db, info.location));
        object.try_emplace("message", info.message);
        return Json { std::move(object) };
    };

    auto tag_to_json = [](Diagnostic_tag tag) {
        switch (tag) {
        case Diagnostic_tag::Unnecessary: return Json { 1 };
        case Diagnostic_tag::Deprecated:  return Json { 2 };
        case Diagnostic_tag::None:
        };
        cpputil::unreachable();
    };

    Json::Object object;
    object.try_emplace("range", range_to_json(diagnostic.range));
    object.try_emplace("severity", severity_to_json(diagnostic.severity));
    object.try_emplace("message", diagnostic.message);

    if (not diagnostic.related_info.empty()) {
        auto info = diagnostic.related_info             //
                  | std::views::transform(info_to_json) //
                  | std::ranges::to<Json::Array>();
        object.try_emplace("relatedInformation", std::move(info));
    }

    if (diagnostic.tag != Diagnostic_tag::None) {
        object.try_emplace("tags", utl::to_vector({ tag_to_json(diagnostic.tag) }));
    }

    return Json { std::move(object) };
}

auto ki::lsp::diagnostic_params_to_json(db::Database const& db, db::Document_id doc_id) -> Json
{
    auto diagnostics = db.documents[doc_id].info.diagnostics
                     | std::views::transform(
                           [&](Diagnostic const& diag) { return diagnostic_to_json(db, diag); })
                     | std::ranges::to<Json::Array>();

    Json::Object params;
    params.try_emplace("uri", path_to_uri(db::document_path(db, doc_id)));
    params.try_emplace("diagnostics", std::move(diagnostics));
    return Json { std::move(params) };
}

auto ki::lsp::hint_to_json(db::Database const& db, db::Document_id doc_id, db::Inlay_hint hint)
    -> Json
{
    auto const& hir = db.documents[doc_id].arena.hir;

    auto const visitor = utl::Overload {
        [&](hir::Type_id type_id) -> Json {
            std::string label = ": ";
            hir::format(hir, db.string_pool, type_id, label);

            Json::Object object;
            object.try_emplace("position", position_to_json(hint.position));
            object.try_emplace("label", std::move(label));
            object.try_emplace("kind", 1); // Type hint
            return Json { std::move(object) };
        },
        [&](hir::Pattern_id patt_id) -> Json {
            std::string label = hir::to_string(hir, db.string_pool, hir.patterns[patt_id]);
            label.append(" =");

            Json::Object object;
            object.try_emplace("position", position_to_json(hint.position));
            object.try_emplace("label", std::move(label));
            object.try_emplace("kind", 2); // Parameter hint
            object.try_emplace("paddingRight", true);
            return Json { std::move(object) };
        },
    };
    return std::visit(visitor, hint.variant);
}

auto ki::lsp::action_to_json(db::Database const& db, db::Document_id doc_id, db::Action action)
    -> Json
{
    auto const visitor = utl::Overload {
        [&](db::Action_silence_unused action) {
            auto const& symbol = db.documents[doc_id].arena.symbols[action.symbol_id];
            auto const  name   = db.string_pool.get(symbol.name.id);

            assert(symbol.use_count == 0);

            auto range = lsp::Range(symbol.name.range.start, symbol.name.range.start);
            auto edit  = make_text_edit(range, "_");
            auto uri   = path_to_uri(db::document_path(db, doc_id));

            Json::Object changes;
            changes.try_emplace(std::move(uri), utl::to_vector({ std::move(edit) }));

            Json::Object workspace_edit;
            workspace_edit.try_emplace("changes", std::move(changes));

            Json::Object object;
            object.try_emplace("title", std::format("Rename '{0}' to '_{0}'", name));
            object.try_emplace("edit", std::move(workspace_edit));
            return Json { std::move(object) };
        },
    };
    return std::visit(visitor, action.variant);
}

auto ki::lsp::reference_to_json(Reference reference) -> Json
{
    Json::Object object;
    object.try_emplace("range", range_to_json(reference.range));
    object.try_emplace("kind", reference_kind_to_json(reference.kind));
    return Json { std::move(object) };
}

auto ki::lsp::reference_kind_to_json(Reference_kind kind) -> Json
{
    switch (kind) {
    case Reference_kind::Text:  return Json { Json::Number { 1 } };
    case Reference_kind::Read:  return Json { Json::Number { 2 } };
    case Reference_kind::Write: return Json { Json::Number { 3 } };
    }
    cpputil::unreachable();
}

auto ki::lsp::make_text_edit(Range range, std::string new_text) -> Json
{
    Json::Object edit;
    edit.try_emplace("range", range_to_json(range));
    edit.try_emplace("newText", std::move(new_text));
    return Json { std::move(edit) };
}

auto ki::lsp::document_item_from_json(Json json) -> Document_item
{
    auto object = as<Json::Object>(std::move(json));
    return Document_item {
        .path     = path_from_uri(as<Json::String>(at(object, "uri"))),
        .text     = as<Json::String>(at(object, "text")),
        .language = as<Json::String>(at(object, "languageId")),
        .version  = as_unsigned(at(object, "version")),
    };
}

auto ki::lsp::format_options_from_json(Json json) -> fmt::Options
{
    auto object = as<Json::Object>(std::move(json));
    return fmt::Options {
        .tab_size   = as_unsigned(at(object, "tabSize")),
        .use_spaces = as<Json::Boolean>(at(object, "insertSpaces")),
    };
}

auto ki::lsp::formatting_params_from_json(db::Database const& db, Json json) -> Formatting_params
{
    auto object = as<Json::Object>(std::move(json));
    return Formatting_params {
        .doc_id  = document_identifier_from_json(db, at(object, "textDocument")),
        .options = format_options_from_json(at(object, "options")),
    };
}

auto ki::lsp::markdown_content_to_json(std::string markdown) -> Json
{
    Json::Object contents;
    contents.try_emplace("kind", "markdown");
    contents.try_emplace("value", std::move(markdown));

    Json::Object object;
    object.try_emplace("contents", std::move(contents));
    return Json { std::move(object) };
}

auto ki::lsp::semantic_tokens_to_json(std::span<Semantic_token const> tokens) -> Json
{
    static constexpr auto to_num = [](auto n) { return cpputil::num::safe_cast<Json::Number>(n); };

    Json::Array array;
    array.reserve(tokens.size() * 5); // Each token is represented by five integers.

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

auto ki::lsp::make_notification(Json::String method, Json params) -> Json
{
    Json::Object notification;
    notification.try_emplace("jsonrpc", "2.0");
    notification.try_emplace("method", std::move(method));
    notification.try_emplace("params", std::move(params));
    return Json { std::move(notification) };
}

auto ki::lsp::as_unsigned(Json json) -> std::uint32_t
{
    if (auto number = as<Json::Number>(std::move(json)); number >= 0) {
        return cpputil::num::safe_cast<std::uint32_t>(number);
    }
    throw Bad_json(std::format("Unexpected negative integer"));
}

auto ki::lsp::at(Json::Object& object, char const* key) -> Json
{
    cpputil::always_assert(key != nullptr);
    if (auto const it = object.find(key); it != object.end()) {
        return std::move(it->second);
    }
    throw Bad_json(std::format("Key not present: '{}'", key));
}

auto ki::lsp::maybe_at(Json::Object& object, char const* key) -> std::optional<Json>
{
    cpputil::always_assert(key != nullptr);
    auto const it = object.find(key);
    return it != object.end() ? std::optional(std::move(it->second)) : std::nullopt;
}
