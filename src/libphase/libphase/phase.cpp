#include <libutl/common/utilities.hpp>
#include <libphase/phase.hpp>

namespace {
    constexpr std::string_view source_string = R"(
        namespace std {
            class Copy { fn copy(&self): Self }
            class Drop { fn drop(&self): () }
            fn id[X](x: X) = x
        }
    )";
}

auto kieli::predefinitions_source(Compilation_info& compilation_info) -> utl::Wrapper<utl::Source>
{
    return compilation_info.get()->source_arena.wrap("[predefined]", std::string(source_string));
}

auto kieli::mock_compilation_info(utl::diagnostics::Level const level) -> Compilation_info
{
    return std::make_shared<kieli::Shared_compilation_info>(kieli::Shared_compilation_info {
        .diagnostics  = utl::diagnostics::Builder { utl::diagnostics::Builder::Configuration {
             .note_level    = level,
             .warning_level = level,
        } },
        .source_arena = utl::Source::Arena::with_page_size(1),
    });
}

auto kieli::test_info_and_source(std::string&& source_string)
    -> utl::Pair<Compilation_info, utl::Source::Wrapper>
{
    kieli::Compilation_info test_info = mock_compilation_info(utl::diagnostics::Level::suppress);
    utl::wrapper auto const test_source
        = test_info.get()->source_arena.wrap("[test]", std::move(source_string));
    return { std::move(test_info), test_source };
}

auto kieli::Name_dynamic::as_upper() const noexcept -> Name_upper
{
    utl::always_assert(is_upper.get());
    return { identifier, source_view };
}

auto kieli::Name_dynamic::as_lower() const noexcept -> Name_lower
{
    utl::always_assert(!is_upper.get());
    return { identifier, source_view };
}

auto kieli::Name_upper::as_dynamic() const noexcept -> Name_dynamic
{
    return { identifier, source_view, true };
}

auto kieli::Name_lower::as_dynamic() const noexcept -> Name_dynamic
{
    return { identifier, source_view, false };
}

auto kieli::Name_dynamic::operator==(Name_dynamic const& other) const noexcept -> bool
{
    return identifier == other.identifier;
}

auto kieli::Name_lower::operator==(Name_lower const& other) const noexcept -> bool
{
    return identifier == other.identifier;
}

auto kieli::Name_upper::operator==(Name_upper const& other) const noexcept -> bool
{
    return identifier == other.identifier;
}

auto kieli::built_in_type::integer_string(Integer const integer) noexcept -> std::string_view
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
        utl::unreachable();
    }
}
