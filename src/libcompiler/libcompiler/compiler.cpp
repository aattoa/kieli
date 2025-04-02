#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <cpputil/io.hpp>

void ki::Position::advance_with(char const character) noexcept
{
    if (character == '\n') {
        ++line, column = 0;
    }
    else {
        ++column;
    }
}

auto ki::Position::horizontal_offset(std::uint32_t offset) const noexcept -> Position
{
    return Position { .line = line, .column = column + offset };
}

ki::Range::Range(Position start, Position stop) noexcept : start { start }, stop { stop }
{
    assert(start <= stop);
}

auto ki::Range::for_position(Position position) noexcept -> Range
{
    return Range(position, position.horizontal_offset(1));
}

auto ki::Range::dummy() noexcept -> Range
{
    return Range::for_position(Position {});
}

auto ki::database(Manifest manifest) -> Database
{
    return Database {
        .documents   = {},
        .paths       = {},
        .string_pool = {},
        .manifest    = std::move(manifest),
    };
}

auto ki::document(std::string text, Document_ownership ownership) -> Document
{
    return ki::Document {
        .text            = std::move(text),
        .diagnostics     = {},
        .semantic_tokens = {},
        .time            = {},
        .ownership       = ownership,
        .revision        = {},
    };
}

auto ki::document_path(Database const& db, Document_id id) -> std::filesystem::path const&
{
    for (auto const& [path, document_id] : db.paths) {
        if (id == document_id) {
            return path;
        }
    }
    cpputil::unreachable();
}

auto ki::set_document(Database& db, std::filesystem::path path, Document document) -> Document_id
{
    auto id = db.documents.push(std::move(document));
    db.paths.insert_or_assign(std::move(path), id);
    return id;
}

auto ki::client_open_document(Database& db, std::filesystem::path path, std::string text)
    -> Document_id
{
    return set_document(db, std::move(path), document(std::move(text), Document_ownership::Client));
}

void ki::client_close_document(Database& db, Document_id const id)
{
    if (db.documents[id].ownership == Document_ownership::Client) {
        db.documents[id] = Document {};
    }
}

auto ki::test_document(Database& db, std::string text) -> Document_id
{
    return set_document(db, "[test]", document(std::move(text), Document_ownership::Server));
}

auto ki::read_file(std::filesystem::path const& path) -> std::expected<std::string, Read_failure>
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

auto ki::read_document(Database& db, std::filesystem::path path)
    -> std::expected<Document_id, Read_failure>
{
    return read_file(path).transform([&](std::string text) {
        Document document {
            .text            = std::move(text),
            .diagnostics     = {},
            .semantic_tokens = {},
            .time            = std::filesystem::last_write_time(path),
            .ownership       = Document_ownership::Server,
            .revision        = 0,
        };
        return set_document(db, std::move(path), std::move(document));
    });
}

auto ki::describe_read_failure(Read_failure failure) -> std::string_view
{
    switch (failure) {
    case Read_failure::Does_not_exist: return "does not exist";
    case Read_failure::Failed_to_open: return "failed to open";
    case Read_failure::Failed_to_read: return "failed to read";
    default:                           cpputil::unreachable();
    }
}

auto ki::text_range(std::string_view text, Range range) -> std::string_view
{
    cpputil::always_assert(range.start <= range.stop);

    char const* begin = text.begin();
    for (std::size_t i = 0; i != range.start.line; ++i) {
        begin = std::find(begin, text.end(), '\n');
        cpputil::always_assert(begin != text.end());
        ++begin; // Skip the line feed
    }
    begin += range.start.column;

    char const* end = begin;
    Position    pos = range.start;
    while (pos != range.stop) {
        cpputil::always_assert(end != text.end());
        pos.advance_with(*end++);
    }

    return { begin, end };
}

void ki::edit_text(std::string& text, Range range, std::string_view new_text)
{
    auto const where  = text_range(text, range);
    auto const offset = static_cast<std::size_t>(where.data() - text.data());
    text.replace(offset, where.size(), new_text);
}

void ki::add_diagnostic(Database& db, Document_id doc_id, Diagnostic diagnostic)
{
    db.documents[doc_id].diagnostics.push_back(std::move(diagnostic));
}

void ki::add_error(Database& db, Document_id doc_id, Range range, std::string message)
{
    add_diagnostic(db, doc_id, error(range, std::move(message)));
}

auto ki::error(Range range, std::string message) -> Diagnostic
{
    return Diagnostic {
        .message      = std::move(message),
        .range        = range,
        .severity     = Severity::Error,
        .related_info = {},
    };
}

auto ki::warning(Range range, std::string message) -> Diagnostic
{
    return Diagnostic {
        .message      = std::move(message),
        .range        = range,
        .severity     = Severity::Warning,
        .related_info = {},
    };
}

auto ki::range_contains(Range range, Position position) noexcept -> bool
{
    return range.start <= position and position < range.stop;
}

auto ki::is_multiline(Range range) noexcept -> bool
{
    return range.start.line != range.stop.line;
}

void ki::print_diagnostics(std::FILE* stream, Database const& db, Document_id doc_id)
{
    for (Diagnostic const& diag : db.documents[doc_id].diagnostics) {
        std::println(stream, "{} {}: {}", severity_string(diag.severity), diag.range, diag.message);
    }
}

auto ki::mutability_string(Mutability mutability) -> std::string_view
{
    return mutability == Mutability::Mut ? "mut" : "immut";
}

auto ki::severity_string(Severity severity) -> std::string_view
{
    switch (severity) {
    case Severity::Error:       return "Error";
    case Severity::Warning:     return "Warning";
    case Severity::Hint:        return "Hint";
    case Severity::Information: return "Info";
    default:                    cpputil::unreachable();
    }
}

auto ki::is_uppercase(std::string_view name) -> bool
{
    auto upper = [](char const c) { return 'A' <= c and c <= 'Z'; };
    auto index = name.find_first_not_of('_');
    return index != std::string_view::npos and upper(name.at(index));
}
