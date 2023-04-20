#include "utl/utilities.hpp"
#include "source.hpp"


utl::Source::Source(std::filesystem::path&& file_path, std::string&& file_content)
    : m_file_path    { std::move(file_path) }
    , m_file_content { std::move(file_content) }
{
    disable_short_string_optimization(m_file_content);
}

auto utl::Source::read(std::filesystem::path&& path) -> Source {
    if (std::ifstream file { path })
        return Source { std::move(path), std::string { std::istreambuf_iterator<char> { file }, {} } };
    else if (std::filesystem::exists(path))
        throw exception("Failed to open file '{}'", path.string());
    else
        throw exception("File '{}' does not exist", path.string());
}

auto utl::Source::path() const noexcept -> const std::filesystem::path& {
    return m_file_path;
}
auto utl::Source::string() const noexcept -> std::string_view {
    return m_file_content;
}

auto utl::Source_position::advance_with(char const c) noexcept -> void {
    if (c == '\n')
        ++line, column = 1;
    else
        ++column;
}

auto utl::Source_view::dummy() -> Source_view {
    static Wrapper_arena<Source> dummy_source_arena { /*page_size=*/ 1 };
    static wrapper auto const dummy_source = dummy_source_arena.wrap("[dummy]", "");
    return Source_view { dummy_source, dummy_source->string(), {}, {} };
}

auto utl::Source_view::operator+(Source_view const& other) const noexcept -> Source_view {
    always_assert(std::to_address(source) == std::to_address(other.source));

    if (other.string.empty())
        return *this;
    if (string.empty())
        return other;

    always_assert(std::invoke(std::less_equal {}, &string.front(), &other.string.back()));
    return Source_view {
        source,
        std::string_view { string.data(), other.string.data() + other.string.size() },
        start_position,
        other.stop_position
    };
}

static_assert(std::is_trivially_copyable_v<utl::Source_view>);
static_assert(std::is_trivially_copyable_v<utl::Source_position>);
static_assert(utl::Source_position { 4, 5 } < utl::Source_position { 9, 2 });
static_assert(utl::Source_position { 5, 2 } < utl::Source_position { 5, 3 });
static_assert(utl::Source_position { 3, 2 } > utl::Source_position { 2, 3 });

DEFINE_FORMATTER_FOR(utl::Source_position) {
    return fmt::format_to(context.out(), "{}:{}", value.line, value.column);
}