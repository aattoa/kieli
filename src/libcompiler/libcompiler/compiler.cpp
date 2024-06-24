#include <libutl/utilities.hpp>
#include <libcompiler/compiler.hpp>

auto kieli::emit_diagnostic(
    cppdiag::Severity const    severity,
    Compile_info&              info,
    Source_id const            source,
    Range const                error_range,
    std::string                message,
    std::optional<std::string> help_note) -> void
{
    info.diagnostics.push_back(cppdiag::Diagnostic {
        .text_sections = utl::to_vector({
            text_section(info.source_vector[source], error_range, std::move(help_note)),
        }),
        .message       = std::move(message),
        .help_note     = std::move(help_note),
        .severity      = severity,
    });
}

auto kieli::fatal_error(
    Compile_info&              info,
    Source_id const            source,
    Range const                error_range,
    std::string                message,
    std::optional<std::string> help_note) -> void
{
    emit_diagnostic(
        cppdiag::Severity::error,
        info,
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
        .start_position = { range.start.line + 1, range.start.column + 1 },
        .stop_position  = { range.stop.line + 1, range.stop.column + 1 },
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

auto kieli::test_info_and_source(std::string&& source_string) -> std::pair<Compile_info, Source_id>
{
    Compile_info info;
    Source_id    source = info.source_vector.push(std::move(source_string), "[test]");
    return { std::move(info), source };
}

auto kieli::predefinitions_source(Compile_info& info) -> Source_id
{
    // TODO: check if predefinitions already exist

    static constexpr std::string_view source = R"(
        mod std {
            class Copy { fn copy(&self): Self }
            class Drop { fn drop(&self): () }
            fn id[X](x: X) = x
        }
    )";
    return info.source_vector.push(std::string(source), "[predefined]");
}

auto kieli::Compilation_failure::what() const noexcept -> char const*
{
    return "kieli::Compilation_failure";
}

auto kieli::Name_dynamic::as_upper() const noexcept -> Name_upper
{
    cpputil::always_assert(is_upper.get());
    return { identifier, range };
}

auto kieli::Name_dynamic::as_lower() const noexcept -> Name_lower
{
    cpputil::always_assert(!is_upper.get());
    return { identifier, range };
}

auto kieli::built_in_type::integer_name(Integer const integer) noexcept -> std::string_view
{
    switch (integer) {
    case Integer::i8:
        return "I8";
    case Integer::i16:
        return "I16";
    case Integer::i32:
        return "I32";
    case Integer::i64:
        return "I64";
    case Integer::u8:
        return "U8";
    case Integer::u16:
        return "U16";
    case Integer::u32:
        return "U32";
    case Integer::u64:
        return "U64";
    default:
        cpputil::unreachable();
    }
}
