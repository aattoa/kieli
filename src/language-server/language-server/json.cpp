#include <libutl/utilities.hpp>
#include <language-server/json.hpp>
#include <language-server/server.hpp>

using namespace ki;

namespace {
    auto integer_to_json(std::integral auto integer) -> lsp::Json
    {
        return { cpputil::num::safe_cast<lsp::Json::Number>(integer) };
    };

    auto ascii_to_lower(char c) noexcept -> char
    {
        return ('A' <= c and c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    auto ascii_to_upper(char c) noexcept -> char
    {
        return ('a' <= c and c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
    }

    auto fuzzy_prefix_match(std::string_view prefix, std::string_view string) -> bool
    {
        return std::ranges::all_of(prefix, [&](char c) {
            return string.contains(ascii_to_lower(c)) or string.contains(ascii_to_upper(c));
        });
    }

    auto tuple_completions(
        db::Database const&        db,
        db::Arena const&           arena,
        std::string_view           prefix,
        std::span<hir::Type const> types) -> lsp::Json::Array
    {
        lsp::Json::Array items;
        items.reserve(types.size());

        (void)prefix; // TODO: filter?

        for (auto const [index, type] : std::views::enumerate(types)) {
            lsp::Json::Object item;

            item.try_emplace("label", std::to_string(index));
            item.try_emplace("kind", 5); // kind=field
            item.try_emplace("detail", hir::to_string(arena.hir, db.string_pool, type));

            items.emplace_back(std::move(item));
        }

        return items;
    }

    auto completion_items(
        db::Database const&  db,
        db::Document_id      doc_id,
        std::string_view     prefix,
        db::Field_completion completion) -> lsp::Json::Array
    {
        auto const& arena   = db.documents[doc_id].arena;
        auto const& variant = arena.hir.types[completion.type_id];

        if (auto const* type = std::get_if<hir::type::Structure>(&variant)) {
            auto const& structure   = arena.hir.structures[type->id].hir.value();
            auto const& constructor = arena.hir.constructors[structure.constructor_id];

            if (auto const* body = std::get_if<hir::Struct_constructor>(&constructor.body)) {
                lsp::Json::Array items;

                for (auto const [name_id, field_id] : body->fields) {
                    if (fuzzy_prefix_match(prefix, db.string_pool.get(name_id))) {
                        items.push_back(lsp::completion_item_to_json(
                            db, doc_id, arena.hir.fields[field_id].symbol_id));
                    }
                }

                return items;
            }
            else if (auto const* body = std::get_if<hir::Tuple_constructor>(&constructor.body)) {
                return tuple_completions(db, arena, prefix, body->types);
            }
        }
        else if (auto const* tuple = std::get_if<hir::type::Tuple>(&variant)) {
            return tuple_completions(db, arena, prefix, tuple->types);
        }

        return {};
    }

    auto completion_items(
        db::Database const&        db,
        db::Document_id            doc_id,
        std::string_view           prefix,
        db::Environment_completion completion) -> lsp::Json::Array
    {
        lsp::Json::Array items;

        for (;;) {
            auto const& env = db.documents[doc_id].arena.environments[completion.env_id];

            for (auto const [name_id, symbol_id] : env.map) {
                if (fuzzy_prefix_match(prefix, db.string_pool.get(name_id))) {
                    items.push_back(lsp::completion_item_to_json(db, doc_id, symbol_id));
                }
            }

            if (completion.mode == db::Completion_mode::Top and env.parent_id.has_value()) {
                completion.env_id = env.parent_id.value();
            }
            else {
                break;
            }
        }

        return items;
    }
} // namespace

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
    auto const scheme = "file://"sv;
    if (uri.starts_with(scheme)) {
        return uri.substr(scheme.size());
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
    result.try_emplace("line", integer_to_json(position.line));
    result.try_emplace("character", integer_to_json(position.column));
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
    case Severity::Error:       return Json { 1 };
    case Severity::Warning:     return Json { 2 };
    case Severity::Information: return Json { 3 };
    case Severity::Hint:        return Json { 4 };
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
        case Diagnostic_tag::None:        ;
        };
        cpputil::unreachable();
    };

    Json::Object object;
    object.try_emplace("range", range_to_json(diagnostic.range));
    object.try_emplace("severity", severity_to_json(diagnostic.severity));
    object.try_emplace("message", diagnostic.message);

    if (not diagnostic.related_info.empty()) {
        auto info = std::ranges::to<Json::Array>(
            std::views::transform(diagnostic.related_info, info_to_json));
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
            hir::format_to(label, hir, db.string_pool, type_id);

            Json::Object object;
            object.try_emplace("position", position_to_json(hint.position));
            object.try_emplace("label", std::move(label));
            object.try_emplace("kind", 1); // Type hint
            return Json { std::move(object) };
        },
        [&](hir::Pattern_id patt_id) -> Json {
            std::string label = hir::to_string(hir, db.string_pool, patt_id);
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
    auto const& arena = db.documents[doc_id].arena;

    auto const visitor = utl::Overload {
        [&](db::Action_silence_unused silence) {
            auto const& symbol = arena.symbols[silence.symbol_id];
            auto const  name   = db.string_pool.get(symbol.name.id);

            assert(symbol.use_count == 0);

            auto range = to_range_0(symbol.name.range.start);
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
        [&](db::Action_fill_in_struct_init const& fill) {
            Json::String text;

            for (hir::Field_id field_id : fill.field_ids) {
                auto name = db.string_pool.get(arena.hir.fields[field_id].name.id);
                text.append(", ").append(name).append(" = _");
            }

            if (not fill.final_field_end.has_value()) {
                text.erase(0, 2); // Erase the first ", "
            }

            auto pos = Position {
                .line   = action.range.stop.line,
                .column = action.range.stop.column - 1,
            };
            auto range = to_range_0(fill.final_field_end.value_or(pos));
            auto edit  = make_text_edit(range, std::move(text));
            auto uri   = path_to_uri(db::document_path(db, doc_id));

            Json::Object changes;
            changes.try_emplace(std::move(uri), utl::to_vector({ std::move(edit) }));

            Json::Object workspace_edit;
            workspace_edit.try_emplace("changes", std::move(changes));

            Json::Object object;
            object.try_emplace("title", "Fill in missing struct fields");
            object.try_emplace("edit", std::move(workspace_edit));
            return Json { std::move(object) };
        },
    };
    return std::visit(visitor, action.variant);
}

auto ki::lsp::signature_help_to_json(
    db::Database const& db, db::Document_id doc_id, db::Signature_info const& info) -> Json
{
    auto const& hir = db.documents[doc_id].arena.hir;
    auto const& sig = hir.functions[info.function_id].signature.value();

    // The signature label has to be formatted here because the parameter label offsets
    // are needed in order for the client to be able to highlight the active parameter.

    Json::String label = std::format("fn {}(", db.string_pool.get(sig.name.id));
    Json::Array  parameters;

    for (auto const& [pattern, type, default_argument] : sig.parameters) {
        if (label.back() != '(') {
            label.append(", ");
        }

        auto const start = cpputil::num::safe_cast<Json::Number>(label.size());

        hir::format_to(label, hir, db.string_pool, pattern);
        label.append(": ");
        hir::format_to(label, hir, db.string_pool, type);

        if (default_argument.has_value()) {
            label.append(" = ");
            hir::format_to(label, hir, db.string_pool, default_argument.value());
        }

        auto const end = cpputil::num::safe_cast<Json::Number>(label.size());

        Json::Object parameter;
        parameter.try_emplace("label", utl::to_vector({ Json { start }, Json { end } }));
        parameters.emplace_back(std::move(parameter));
    }

    label.append("): ");
    hir::format_to(label, hir, db.string_pool, sig.return_type);

    Json::Object object;
    object.try_emplace("label", std::move(label));
    object.try_emplace("parameters", std::move(parameters));

    Json::Object help;
    help.try_emplace("signatures", utl::to_vector({ Json { std::move(object) } }));
    help.try_emplace("activeParameter", cpputil::num::safe_cast<Json::Number>(info.active_param));
    return Json { std::move(help) };
}

auto ki::lsp::completion_list_to_json(
    db::Database const& db, db::Document_id doc_id, db::Completion_info const& info) -> Json
{
    cpputil::always_assert(not is_multiline(info.range));

    auto const visitor = [&](auto completion) {
        return completion_items(db, doc_id, info.prefix, std::move(completion));
    };

    Json::Object defaults;
    defaults.try_emplace("editRange", range_to_json(info.range));

    Json::Object list;
    list.try_emplace("items", std::visit(visitor, info.variant));
    list.try_emplace("itemDefaults", std::move(defaults));
    list.try_emplace("isIncomplete", false);
    return Json { std::move(list) };
}

auto ki::lsp::completion_item_to_json(
    db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> Json
{
    auto const& arena  = db.documents[doc_id].arena;
    auto const& symbol = arena.symbols[symbol_id];

    Json::Object item;

    item.try_emplace("label", Json::String(db.string_pool.get(symbol.name.id)));
    item.try_emplace("kind", completion_item_kind_to_json(symbol.variant));

    std::string markdown = symbol_documentation(db, doc_id, symbol_id);
    item.try_emplace("documentation", markdown_content_to_json(std::move(markdown)));

    if (auto const type = db::symbol_type(arena, symbol_id)) {
        auto detail = hir::to_string(arena.hir, db.string_pool, type.value());
        item.try_emplace("detail", std::move(detail));
    }

    return Json { std::move(item) };
}

auto ki::lsp::symbol_to_json(
    db::Database const& db, db::Document_id doc_id, db::Symbol_id symbol_id) -> Json
{
    auto const& arena  = db.documents[doc_id].arena;
    auto const& symbol = arena.symbols[symbol_id];

    Json::Object object;
    object.try_emplace("name", Json::String(db.string_pool.get(symbol.name.id)));
    object.try_emplace("kind", symbol_kind_to_json(symbol.variant));
    object.try_emplace("range", range_to_json(symbol.name.range)); // TODO: full range
    object.try_emplace("selectionRange", range_to_json(symbol.name.range));

    auto const children_visitor = utl::Overload {
        [&](hir::Structure_id id) {
            return constructor_fields(
                db, doc_id, arena.hir.structures[id].hir.value().constructor_id);
        },
        [&](hir::Enumeration_id enum_id) {
            return arena.hir.enumerations[enum_id].hir.value().constructor_ids
                 | std::views::transform(std::bind_front(symbol_to_json, std::cref(db), doc_id))
                 | std::ranges::to<Json::Array>();
        },
        [&](hir::Constructor_id ctor_id) {
            return constructor_fields(db, doc_id, ctor_id); //
        },
        [&](hir::Module_id id) {
            return environment_symbols(db, doc_id, arena.hir.modules[id].mod_env_id);
        },
        [](auto) { return Json::Array(); },
    };

    auto const detail_visitor = utl::Overload {
        [&](hir::Function_id id) {
            auto const& signature = arena.hir.functions[id].signature.value();
            return hir::to_string(arena.hir, db.string_pool, signature.function_type);
        },
        [&](hir::Field_id id) {
            return hir::to_string(arena.hir, db.string_pool, arena.hir.fields[id].type);
        },
        [](auto) { return std::string(); },
    };

    if (auto children = std::visit(children_visitor, symbol.variant); not children.empty()) {
        object.try_emplace("children", std::move(children));
    }
    if (auto detail = std::visit(detail_visitor, symbol.variant); not detail.empty()) {
        object.try_emplace("detail", std::move(detail));
    }

    return Json { std::move(object) };
}

auto ki::lsp::symbol_kind_to_json(db::Symbol_variant variant) -> Json
{
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#symbolKind
    auto const visitor = utl::Overload {
        [](db::Error) { return 0; },
        [](hir::Function_id) { return 12; },
        [](hir::Structure_id) { return 23; },
        [](hir::Enumeration_id) { return 10; },
        [](hir::Constructor_id) { return 9; },
        [](hir::Field_id) { return 8; },
        [](hir::Concept_id) { return 11; },
        [](hir::Alias_id) { return 14; },
        [](hir::Module_id) { return 2; },
        [](hir::Local_variable_id) { return 13; },
        [](hir::Local_mutability_id) { return 13; },
        [](hir::Local_type_id) { return 14; },
    };
    return Json { std::visit(visitor, variant) };
}

auto ki::lsp::completion_item_kind_to_json(db::Symbol_variant variant) -> Json
{
    // https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#completionItemKind
    auto const visitor = utl::Overload {
        [](db::Error) { return 0; },
        [](hir::Function_id) { return 3; },
        [](hir::Structure_id) { return 22; },
        [](hir::Enumeration_id) { return 13; },
        [](hir::Constructor_id) { return 4; },
        [](hir::Field_id) { return 5; },
        [](hir::Concept_id) { return 8; },
        [](hir::Alias_id) { return 21; },
        [](hir::Module_id) { return 9; },
        [](hir::Local_variable_id) { return 6; },
        [](hir::Local_mutability_id) { return 6; },
        [](hir::Local_type_id) { return 6; },
    };
    return Json { std::visit(visitor, variant) };
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
    case Reference_kind::Text:  return Json { 1 };
    case Reference_kind::Read:  return Json { 2 };
    case Reference_kind::Write: return Json { 3 };
    }
    cpputil::unreachable();
}

auto ki::lsp::environment_symbols(
    db::Database const& db, db::Document_id doc_id, db::Environment_id env_id) -> Json::Array
{
    return std::views::values(db.documents[doc_id].arena.environments[env_id].map)
         | std::views::transform(std::bind_front(symbol_to_json, std::cref(db), doc_id))
         | std::ranges::to<Json::Array>();
}

auto ki::lsp::constructor_fields(
    db::Database const& db, db::Document_id doc_id, hir::Constructor_id ctor_id) -> Json::Array
{
    auto const& arena = db.documents[doc_id].arena;
    auto const& ctor  = arena.hir.constructors[ctor_id];

    if (auto const* body = std::get_if<hir::Struct_constructor>(&ctor.body)) {
        auto field_to_json = [&](hir::Field_id field_id) {
            return symbol_to_json(db, doc_id, arena.hir.fields[field_id].symbol_id);
        };
        return std::views::values(body->fields)     //
             | std::views::transform(field_to_json) //
             | std::ranges::to<Json::Array>();
    }

    return {};
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
    Json::Object object;
    object.try_emplace("kind", "markdown");
    object.try_emplace("value", std::move(markdown));
    return Json { std::move(object) };
}

auto ki::lsp::semantic_tokens_to_json(std::span<Semantic_token const> tokens) -> Json
{
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
        array.emplace_back(integer_to_json(token.position.line - prev.line));
        array.emplace_back(integer_to_json(token.position.column - prev.column));
        array.emplace_back(integer_to_json(token.length));
        array.emplace_back(integer_to_json(std::to_underlying(token.type)));
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

auto ki::lsp::at(Json::Object& object, std::string_view key) -> Json
{
    if (auto const it = object.find(key); it != object.end()) {
        return std::move(it->second);
    }
    throw Bad_json(std::format("Key not present: '{}'", key));
}

auto ki::lsp::maybe_at(Json::Object& object, std::string_view key) -> std::optional<Json>
{
    auto const it = object.find(key);
    return it != object.end() ? std::optional(std::move(it->second)) : std::nullopt;
}
