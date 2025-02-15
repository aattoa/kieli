#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <cpputil/io.hpp>

void kieli::Position::advance_with(char const character) noexcept
{
    if (character == '\n') {
        ++line, column = 0;
    }
    else {
        ++column;
    }
}

kieli::Range::Range(Position const start, Position const stop) noexcept
    : start { start }
    , stop { stop }
{}

auto kieli::Range::for_position(Position const position) noexcept -> Range
{
    return Range(position, Position { .line = position.line, .column = position.column + 1 });
}

auto kieli::Range::dummy() noexcept -> Range
{
    return Range::for_position(Position {});
}

auto kieli::Range::contains(Position const position) const noexcept -> bool
{
    return start <= position && position < stop;
}

auto kieli::find_existing_document_id(Database const& db, std::filesystem::path const& path)
    -> std::optional<Document_id>
{
    auto const it = db.paths.find(path);
    return it != db.paths.end() ? std::optional(it->second) : std::nullopt;
}

auto kieli::document_path(const Database& db, Document_id id) -> std::filesystem::path const&
{
    for (auto const& [path, document_id] : db.paths) {
        if (id == document_id) {
            return path;
        }
    }
    cpputil::unreachable();
}

auto kieli::set_document(Database& db, std::filesystem::path path, Document document) -> Document_id
{
    auto id = db.documents.push(std::move(document));
    db.paths.insert_or_assign(std::move(path), id);
    return id;
}

auto kieli::client_open_document(Database& db, std::filesystem::path path, std::string text)
    -> Document_id
{
    Document document {
        .text      = std::move(text),
        .ownership = Document_ownership::client,
    };
    return set_document(db, std::move(path), std::move(document));
}

void kieli::client_close_document(Database& db, Document_id const id)
{
    if (db.documents[id].ownership == Document_ownership::client) {
        db.documents[id] = Document {};
    }
}

auto kieli::test_document(Database& db, std::string text) -> Document_id
{
    Document document {
        .text      = std::move(text),
        .ownership = Document_ownership::server,
    };
    return set_document(db, "[test-document]", std::move(document));
}

auto kieli::read_file(std::filesystem::path const& path) -> std::expected<std::string, Read_failure>
{
    if (auto file = cpputil::io::File::open_read(path.c_str())) {
        if (auto text = cpputil::io::read(file.get())) {
            return std::move(text).value();
        }
        return std::unexpected(Read_failure::failed_to_read);
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected(Read_failure::failed_to_open);
    }
    return std::unexpected(Read_failure::does_not_exist);
}

auto kieli::read_document(Database& db, std::filesystem::path path)
    -> std::expected<Document_id, Read_failure>
{
    return read_file(path).transform([&](std::string text) {
        Document document {
            .text      = std::move(text),
            .time      = std::filesystem::last_write_time(path),
            .ownership = Document_ownership::server,
        };
        return set_document(db, std::move(path), std::move(document));
    });
}

auto kieli::describe_read_failure(Read_failure const failure) -> std::string_view
{
    switch (failure) {
    case Read_failure::does_not_exist: return "does not exist";
    case Read_failure::failed_to_open: return "failed to open";
    case Read_failure::failed_to_read: return "failed to read";
    default:                           cpputil::unreachable();
    }
}

auto kieli::text_range(std::string_view const text, Range const range) -> std::string_view
{
    cpputil::always_assert(range.start <= range.stop);

    auto begin = text.begin();
    for (std::size_t i = 0; i != range.start.line; ++i) {
        begin = std::find(begin, text.end(), '\n');
        cpputil::always_assert(begin != text.end());
        ++begin; // Skip the line feed
    }
    begin += range.start.column;

    auto end = begin;
    auto pos = range.start;
    while (pos != range.stop) {
        cpputil::always_assert(end != text.end());
        pos.advance_with(*end++);
    }

    return { begin, end };
}

void kieli::edit_text(std::string& text, Range const range, std::string_view const new_text)
{
    auto const where  = text_range(text, range);
    auto const offset = static_cast<std::size_t>(where.data() - text.data());
    text.replace(offset, where.size(), new_text);
}

void kieli::add_diagnostic(Database& db, Document_id const id, Diagnostic diagnostic)
{
    db.documents[id].diagnostics.push_back(std::move(diagnostic));
}

void kieli::add_error(Database& db, Document_id const id, Range const range, std::string message)
{
    add_diagnostic(db, id, error(range, std::move(message)));
}

auto kieli::error(Range const range, std::string message) -> Diagnostic
{
    return Diagnostic {
        .message  = std::move(message),
        .range    = range,
        .severity = Severity::error,
    };
}

auto kieli::format_document_diagnostics(
    Database const& db, Document_id const id, cppdiag::Colors const colors) -> std::string
{
    // TODO: remove cppdiag

    auto const to_cppdiag = [&](kieli::Diagnostic const& diagnostic) {
        auto const pos = [](Position const position) {
            return cppdiag::Position { .line = position.line, .column = position.column };
        };
        return cppdiag::Diagnostic {
            .text_sections = utl::to_vector({ cppdiag::Text_section {
                .source_string  = db.documents[id].text,
                .source_name    = document_path(db, id),
                .start_position = pos(diagnostic.range.start),
                .stop_position  = pos(diagnostic.range.stop),
            } }),
            .message       = diagnostic.message,
            .severity      = static_cast<cppdiag::Severity>(diagnostic.severity),
        };
    };

    std::string output;
    for (Diagnostic const& diagnostic : db.documents[id].diagnostics) {
        cppdiag::format_diagnostic(output, to_cppdiag(diagnostic), colors);
    }
    return output;
}

auto kieli::Name::is_upper() const -> bool
{
    static constexpr auto is_upper = [](char const c) { return 'A' <= c and c <= 'Z'; };
    std::size_t const     position = identifier.view().find_first_not_of('_');
    return position != std::string_view::npos and is_upper(identifier.view().at(position));
}

auto kieli::Name::operator==(Name const& other) const noexcept -> bool
{
    return identifier == other.identifier;
}
