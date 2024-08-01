#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

auto kieli::Compilation_failure::what() const noexcept -> char const*
{
    return "kieli::Compilation_failure";
}

auto kieli::emit_diagnostic(
    cppdiag::Severity const    severity,
    Database&                  db,
    Source_id const            source,
    Range const                range,
    std::string                message,
    std::optional<std::string> help_note) -> void
{
    db.diagnostics.push_back(cppdiag::Diagnostic {
        .text_sections = utl::to_vector({
            text_section(db.sources[source], range, std::move(help_note)),
        }),
        .message       = std::move(message),
        .help_note     = std::move(help_note),
        .severity      = severity,
    });
}

auto kieli::fatal_error(
    Database&                  db,
    Source_id const            source,
    Range const                error_range,
    std::string                message,
    std::optional<std::string> help_note) -> void
{
    emit_diagnostic(
        cppdiag::Severity::error,
        db,
        source,
        error_range,
        std::move(message),
        std::move(help_note));
    throw Compilation_failure {};
}

auto kieli::text_section(
    Source const&                          source,
    Range const                            range,
    std::optional<std::string>             note,
    std::optional<cppdiag::Severity> const note_severity) -> cppdiag::Text_section
{
    return cppdiag::Text_section {
        .source_string  = source.content,
        .source_name    = source.path.string(),
        .start_position = { range.start.line, range.start.column },
        .stop_position  = { range.stop.line, range.stop.column },
        .note           = std::move(note),
        .note_severity  = note_severity,
    };
}

auto kieli::format_diagnostics(
    std::span<cppdiag::Diagnostic const> diagnostics, cppdiag::Colors const colors) -> std::string
{
    std::string output;
    for (cppdiag::Diagnostic const& diagnostic : diagnostics) {
        cppdiag::format_diagnostic(output, diagnostic, colors);
        output.push_back('\n');
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

auto kieli::built_in_type::integer_name(Integer const integer) noexcept -> std::string_view
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
