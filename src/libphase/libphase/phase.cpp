#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>

namespace {
    constexpr std::string_view predefinitions_source_string = R"(
        namespace std {
            class Copy { fn copy(&self): Self }
            class Drop { fn drop(&self): () }
            fn id[X](x: X) = x
        }
    )";
}

auto kieli::predefinitions_source(Compile_info& compile_info) -> utl::Source::Wrapper
{
    return compile_info.source_arena.wrap(
        "[predefined]", std::string(predefinitions_source_string));
}

auto kieli::test_info_and_source(std::string&& source_string)
    -> std::pair<Compile_info, utl::Source::Wrapper>
{
    Compile_info test_info {
        .source_arena = utl::Source::Arena::with_page_size(1),
    };
    utl::Source::Wrapper const test_source
        = test_info.source_arena.wrap("[test]", std::move(source_string));
    return { std::move(test_info), test_source };
}

auto kieli::text_section(
    utl::Source::Wrapper const                   section_source,
    utl::Source_range const                      section_range,
    std::optional<cppdiag::Message_string> const section_note,
    std::optional<cppdiag::Severity> const       severity) -> cppdiag::Text_section
{
    return cppdiag::Text_section {
        .source_string  = section_source->string(),
        .source_name    = section_source->path().c_str(),
        .start_position = { section_range.start.line, section_range.start.column },
        .stop_position  = { section_range.stop.line, section_range.stop.column },
        .note           = section_note,
        .note_severity  = severity,
    };
}

auto kieli::Compilation_failure::what() const noexcept -> char const*
{
    return "kieli::Compilation_failure";
}

auto kieli::Diagnostics::format_all(cppdiag::Colors const colors) const -> std::string
{
    std::string output;
    for (cppdiag::Diagnostic const& diagnostic : vector) {
        format_diagnostic(diagnostic, message_buffer, output, colors);
        output.push_back('\n');
    }
    return output;
}

auto kieli::Name_dynamic::as_upper() const noexcept -> Name_upper
{
    cpputil::always_assert(is_upper.get());
    return { identifier, source_range };
}

auto kieli::Name_dynamic::as_lower() const noexcept -> Name_lower
{
    cpputil::always_assert(!is_upper.get());
    return { identifier, source_range };
}

auto kieli::built_in_type::integer_name(Integer const integer) noexcept -> std::string_view
{
    // clang-format off
    switch (integer) {
    case Integer::i8:  return "I8";
    case Integer::i16: return "I16";
    case Integer::i32: return "I32";
    case Integer::i64: return "I64";
    case Integer::u8:  return "U8";
    case Integer::u16: return "U16";
    case Integer::u32: return "U32";
    case Integer::u64: return "U64";
    default:
        cpputil::unreachable();
    }
    // clang-format on
}
