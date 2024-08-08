#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>
#include <cpputil/io.hpp>

auto kieli::Compilation_failure::what() const noexcept -> char const*
{
    return "kieli::Compilation_failure";
}

auto kieli::Position::advance_with(char const character) noexcept -> void
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

auto kieli::document(Database& db, Document_id const id) -> Document&
{
    return db.documents.at(id);
}

auto kieli::add_document(
    Database&                db,
    std::filesystem::path    path,
    std::string              text,
    Document_ownership const ownership) -> Document_id
{
    cpputil::always_assert(find_document(db, path) == std::nullopt);
    Document_id const document_id = db.paths.push(std::move(path));
    db.documents.insert({ document_id, { .text = std::move(text), .ownership = ownership } });
    return document_id;
}

auto kieli::find_document(Database const& db, std::filesystem::path const& path)
    -> std::optional<Document_id>
{
    auto const it    = std::ranges::find(db.paths.underlying, path);
    auto const index = static_cast<std::size_t>(it - db.paths.underlying.begin());
    return it != db.paths.underlying.end() ? std::optional(Document_id(index)) : std::nullopt;
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
        return add_document(db, std::move(path), std::move(text)); //
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

auto kieli::text_range(std::string_view const string, Range const range) -> std::string_view
{
    cpputil::always_assert(range.start <= range.stop);

    auto begin = string.begin();
    for (std::size_t i = 0; i != range.start.line; ++i) {
        begin = std::find(begin, string.end(), '\n');
        cpputil::always_assert(begin != string.end());
        ++begin; // Skip the line feed
    }
    begin += range.start.column;

    auto end = begin;
    auto pos = range.start;
    while (pos != range.stop) {
        cpputil::always_assert(end != string.end());
        pos.advance_with(*end++);
    }

    return { begin, end };
}

auto kieli::edit_text(std::string& text, Range const range, std::string_view const new_text) -> void
{
    auto const where  = text_range(text, range);
    auto const offset = static_cast<std::size_t>(where.data() - text.data());
    text.replace(offset, where.size(), new_text);
}

auto kieli::fatal_error(Database& db, Document_id const document_id, Diagnostic diagnostic) -> void
{
    document(db, document_id).diagnostics.push_back(std::move(diagnostic));
    throw Compilation_failure {};
}

auto kieli::fatal_error(
    Database& db, Document_id const document_id, Range const range, std::string message) -> void
{
    Diagnostic diagnostic {
        .message  = std::move(message),
        .range    = range,
        .severity = Severity::error,
    };
    fatal_error(db, document_id, std::move(diagnostic));
}

auto kieli::format_diagnostics(
    std::filesystem::path const& path,
    Document const&              document,
    cppdiag::Colors const        colors) -> std::string
{
    // TODO: remove cppdiag

    auto const to_cppdiag = [&](kieli::Diagnostic const& diagnostic) {
        auto const pos = [](Position const position) {
            return cppdiag::Position { .line = position.line, .column = position.column };
        };
        return cppdiag::Diagnostic {
            .text_sections = utl::to_vector({ cppdiag::Text_section {
                .source_string  = document.text,
                .source_name    = path.string(),
                .start_position = pos(diagnostic.range.start),
                .stop_position  = pos(diagnostic.range.stop),
            } }),
            .message       = diagnostic.message,
            .help_note     = diagnostic.help_note,
            .severity      = static_cast<cppdiag::Severity>(diagnostic.severity),
        };
    };

    std::string output;
    for (Diagnostic const& diagnostic : document.diagnostics) {
        cppdiag::format_diagnostic(output, to_cppdiag(diagnostic), colors);
    }
    return output;
}

auto kieli::Name::is_upper() const -> bool
{
    static constexpr auto is_upper = [](char const c) { return 'A' <= c && c <= 'Z'; };
    std::size_t const     position = identifier.view().find_first_not_of('_');
    return position == std::string_view::npos ? false : is_upper(identifier.view().at(position));
}

auto kieli::Name::operator==(Name const& other) const noexcept -> bool
{
    return identifier == other.identifier;
}

auto kieli::type::integer_name(Integer const integer) noexcept -> std::string_view
{
    switch (integer) {
    case Integer::i8:  return "I8";
    case Integer::i16: return "I16";
    case Integer::i32: return "I32";
    case Integer::i64: return "I64";
    case Integer::u8:  return "U8";
    case Integer::u16: return "U16";
    case Integer::u32: return "U32";
    case Integer::u64: return "U64";
    default:           cpputil::unreachable();
    }
}
