#include <libutl/utilities.hpp>
#include <libcompiler/db.hpp>
#include <cpputil/io.hpp>

ki::lsp::Range::Range(Position start, Position stop) : start { start }, stop { stop }
{
    assert(start <= stop);
}

auto ki::lsp::advance(Position position, char character) noexcept -> Position
{
    if (character == '\n') {
        return Position { .line = position.line + 1, .column = 0 };
    }
    else {
        return Position { .line = position.line, .column = position.column + 1 };
    }
}

auto ki::lsp::column_offset(Position position, std::uint32_t offset) noexcept -> Position
{
    return Position { .line = position.line, .column = position.column + offset };
}

auto ki::lsp::to_range(Position position) noexcept -> Range
{
    return Range(position, column_offset(position, 1));
}

auto ki::lsp::to_range_0(Position position) noexcept -> Range
{
    return Range(position, position);
}

auto ki::lsp::range_contains(Range range, Position position) noexcept -> bool
{
    return range.start <= position and position < range.stop;
}

auto ki::lsp::range_contains_inclusive(Range range, Position position) noexcept -> bool
{
    return range.start <= position and position <= range.stop;
}

auto ki::lsp::is_multiline(Range range) noexcept -> bool
{
    return range.start.line != range.stop.line;
}

auto ki::lsp::error(Range range, std::string message) -> Diagnostic
{
    return Diagnostic {
        .message      = std::move(message),
        .range        = range,
        .severity     = Severity::Error,
        .related_info = {},
        .tag          = Diagnostic_tag::None,
    };
}

auto ki::lsp::warning(Range range, std::string message) -> Diagnostic
{
    return Diagnostic {
        .message      = std::move(message),
        .range        = range,
        .severity     = Severity::Warning,
        .related_info = {},
        .tag          = Diagnostic_tag::None,
    };
}

auto ki::lsp::severity_string(Severity severity) -> std::string_view
{
    switch (severity) {
    case Severity::Error:       return "Error";
    case Severity::Warning:     return "Warning";
    case Severity::Hint:        return "Hint";
    case Severity::Information: return "Info";
    }
    cpputil::unreachable();
}

auto ki::lsp::read(Range range) noexcept -> Reference
{
    return Reference { .range = range, .kind = Reference_kind::Read };
}

auto ki::lsp::write(Range range) noexcept -> Reference
{
    return Reference { .range = range, .kind = Reference_kind::Write };
}

auto ki::db::type_hints_enabled(Inlay_hint_mode mode) noexcept -> bool
{
    return mode == Inlay_hint_mode::Type or mode == Inlay_hint_mode::Full;
}

auto ki::db::parameter_hints_enabled(Inlay_hint_mode mode) noexcept -> bool
{
    return mode == Inlay_hint_mode::Parameter or mode == Inlay_hint_mode::Full;
}

auto ki::db::database(Configuration config) -> Database
{
    return Database {
        .documents   = {},
        .paths       = {},
        .string_pool = {},
        .config      = std::move(config),
    };
}

auto ki::db::document(std::string text, Ownership ownership) -> Document
{
    return Document {
        .info          = Document_info {},
        .text          = std::move(text),
        .arena         = Arena {},
        .ownership     = ownership,
        .edit_position = std::nullopt,
    };
}

auto ki::db::document_path(Database const& db, Document_id id) -> std::filesystem::path const&
{
    for (auto const& [path, doc_id] : db.paths) {
        if (id == doc_id) {
            return path;
        }
    }
    cpputil::unreachable();
}

auto ki::db::set_document(Database& db, std::filesystem::path path, Document document)
    -> Document_id
{
    auto id = db.documents.push(std::move(document));
    db.paths.insert_or_assign(std::move(path), id);
    return id;
}

auto ki::db::client_open_document(Database& db, std::filesystem::path path, std::string text)
    -> Document_id
{
    return set_document(db, std::move(path), document(std::move(text), Ownership::Client));
}

void ki::db::client_close_document(Database& db, Document_id const id)
{
    if (db.documents[id].ownership == Ownership::Client) {
        db.documents[id] = Document {};
    }
}

auto ki::db::test_document(Database& db, std::string text) -> Document_id
{
    return set_document(db, "[test]", document(std::move(text), Ownership::Server));
}

auto ki::db::read_file(std::filesystem::path const& path)
    -> std::expected<std::string, Read_failure>
{
    if (auto file = cpputil::io::File::open_read(path.c_str())) {
        if (auto text = cpputil::io::read(file.get())) {
            return std::move(text).value();
        }
        return std::unexpected(Read_failure::Failed_to_read);
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected(Read_failure::Failed_to_open);
    }
    return std::unexpected(Read_failure::Does_not_exist);
}

auto ki::db::read_document(Database& db, std::filesystem::path path)
    -> std::expected<Document_id, Read_failure>
{
    return read_file(path).transform([&](std::string text) {
        return set_document(db, std::move(path), document(std::move(text), Ownership::Server));
    });
}

auto ki::db::describe_read_failure(Read_failure failure) -> std::string_view
{
    switch (failure) {
    case Read_failure::Does_not_exist: return "does not exist";
    case Read_failure::Failed_to_open: return "failed to open";
    case Read_failure::Failed_to_read: return "failed to read";
    }
    cpputil::unreachable();
}

auto ki::db::describe_symbol_kind(Symbol_variant variant) -> std::string_view
{
    auto const visitor = utl::Overload {
        [](Error) { return "an error"; },
        [](hir::Function_id) { return "a function"; },
        [](hir::Structure_id) { return "a structure"; },
        [](hir::Enumeration_id) { return "an enumeration"; },
        [](hir::Constructor_id) { return "a constructor"; },
        [](hir::Field_id) { return "a field"; },
        [](hir::Concept_id) { return "a concept"; },
        [](hir::Alias_id) { return "a type alias"; },
        [](hir::Module_id) { return "a module"; },
        [](hir::Local_variable_id) { return "a local variable binding"; },
        [](hir::Local_mutability_id) { return "a local mutability binding"; },
        [](hir::Local_type_id) { return "a local type binding"; },
    };
    return std::visit(visitor, variant);
}

auto ki::db::text_range(std::string_view text, lsp::Range range) -> std::string_view
{
    cpputil::always_assert(range.start <= range.stop);

    char const* begin = text.begin();
    for (std::size_t i = 0; i != range.start.line; ++i) {
        begin = std::find(begin, text.end(), '\n');
        cpputil::always_assert(begin != text.end());
        ++begin; // Skip the line feed
    }

    cpputil::always_assert(range.start.column <= std::distance(begin, text.end()));
    begin += range.start.column;

    char const*   end = begin;
    lsp::Position pos = range.start;
    while (pos != range.stop) {
        cpputil::always_assert(end != text.end());
        pos = lsp::advance(pos, *end++);
    }

    return { begin, end };
}

void ki::db::edit_text(std::string& text, lsp::Range range, std::string_view new_text)
{
    auto const where  = text_range(text, range);
    auto const offset = static_cast<std::size_t>(where.data() - text.data());
    text.replace(offset, where.size(), new_text);
}

void ki::db::add_signature_help(
    Database&        db,
    Document_id      doc_id,
    lsp::Range       range,
    hir::Function_id function_id,
    std::size_t      parameter_index)
{
    if (db.config.signature_help) {
        auto const edit_pos = db.documents[doc_id].edit_position;
        if (edit_pos.has_value() and lsp::range_contains(range, edit_pos.value())) {
            db.documents[doc_id].info.signature_info = Signature_info {
                .function_id  = function_id,
                .active_param = cpputil::num::safe_cast<std::uint32_t>(parameter_index),
            };
        }
    }
}

void ki::db::add_completion(Database& db, Document_id doc_id, Name name, Completion_variant variant)
{
    if (db.config.code_completion) {
        auto const edit_pos = db.documents[doc_id].edit_position;
        if (edit_pos.has_value() and lsp::range_contains_inclusive(name.range, edit_pos.value())) {
            auto const prefix_length = edit_pos.value().column - name.range.start.column;
            db.documents[doc_id].info.completion_info = Completion_info {
                .prefix  = std::string(db.string_pool.get(name.id).substr(0, prefix_length)),
                .range   = lsp::Range(name.range.start, edit_pos.value()),
                .variant = std::move(variant),
            };
        }
    }
}

void ki::db::add_type_hint(
    Database& db, Document_id doc_id, lsp::Position position, hir::Type_id type_id)
{
    if (type_hints_enabled(db.config.inlay_hints)) {
        db.documents[doc_id].info.inlay_hints.emplace_back(position, type_id);
    }
}

void ki::db::add_param_hint(
    Database& db, Document_id doc_id, lsp::Position position, hir::Pattern_id param)
{
    if (parameter_hints_enabled(db.config.inlay_hints)) {
        db.documents[doc_id].info.inlay_hints.emplace_back(position, param);
    }
}

void ki::db::add_action(Database& db, Document_id doc_id, lsp::Range range, Action_variant variant)
{
    if (db.config.code_actions) {
        db.documents[doc_id].info.actions.emplace_back(std::move(variant), range);
    }
}

void ki::db::add_reference(
    Database& db, Document_id doc_id, lsp::Reference ref, Symbol_id symbol_id)
{
    if (db.config.references) {
        db.documents[doc_id].info.references.emplace_back(ref, symbol_id);
    }
}

void ki::db::add_diagnostic(Database& db, Document_id doc_id, lsp::Diagnostic diagnostic)
{
    if (db.config.diagnostics) {
        db.documents[doc_id].info.diagnostics.push_back(std::move(diagnostic));
    }
}

void ki::db::add_error(Database& db, Document_id doc_id, lsp::Range range, std::string message)
{
    add_diagnostic(db, doc_id, error(range, std::move(message)));
}

void ki::db::print_diagnostics(std::ostream& stream, Database const& db, Document_id doc_id)
{
    for (lsp::Diagnostic const& diag : db.documents[doc_id].info.diagnostics) {
        std::println(stream, "{} {}: {}", severity_string(diag.severity), diag.range, diag.message);
    }
}

auto ki::db::symbol_type(Arena const& arena, Symbol_id symbol_id) -> std::optional<hir::Type_id>
{
    auto const visitor = utl::Overload {
        [](Error) { return std::nullopt; },
        [](hir::Concept_id) { return std::nullopt; },
        [](hir::Module_id) { return std::nullopt; },
        [](hir::Local_mutability_id) { return std::nullopt; },

        [&](hir::Function_id id) {
            return arena.hir.functions[id].signature.value().function_type.id;
        },
        [&](hir::Structure_id id) { return arena.hir.structures[id].type_id; },
        [&](hir::Enumeration_id id) { return arena.hir.enumerations[id].type_id; },
        [&](hir::Constructor_id id) { return arena.hir.constructors[id].owner_type_id; },
        [&](hir::Field_id id) { return arena.hir.fields[id].type.id; },
        [&](hir::Alias_id id) { return arena.hir.aliases[id].hir.value().type.id; },
        [&](hir::Local_variable_id id) { return arena.hir.local_variables[id].type_id; },
        [&](hir::Local_type_id id) { return arena.hir.local_types[id].type_id; },
    };
    return std::visit<std::optional<hir::Type_id>>(visitor, arena.symbols[symbol_id].variant);
}

auto ki::db::type_definition(Arena const& arena, hir::Type_id type_id) -> std::optional<lsp::Range>
{
    auto const& variant = arena.hir.types[type_id];
    if (auto const* enumeration = std::get_if<hir::type::Enumeration>(&variant)) {
        return arena.hir.enumerations[enumeration->id].name.range;
    }
    if (auto const* structure = std::get_if<hir::type::Structure>(&variant)) {
        return arena.hir.structures[structure->id].name.range;
    }
    if (auto const* reference = std::get_if<hir::type::Reference>(&variant)) {
        return type_definition(arena, reference->referenced_type.id);
    }
    if (auto const* pointer = std::get_if<hir::type::Pointer>(&variant)) {
        return type_definition(arena, pointer->pointee_type.id);
    }
    if (auto const* function = std::get_if<hir::type::Function>(&variant)) {
        return type_definition(arena, function->return_type.id);
    }
    return std::nullopt;
}

auto ki::db::mutability_string(Mutability mutability) -> std::string_view
{
    return mutability == Mutability::Mut ? "mut" : "immut";
}

auto ki::db::is_uppercase(std::string_view name) -> bool
{
    auto upper = [](char const c) { return 'A' <= c and c <= 'Z'; };
    auto index = name.find_first_not_of('_');
    return index != std::string_view::npos and upper(name.at(index));
}
