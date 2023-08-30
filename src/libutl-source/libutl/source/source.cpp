#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>

utl::Source::Source(std::filesystem::path&& file_path, std::string&& file_content)
    : m_file_path { std::move(file_path) }
    , m_file_content { std::move(file_content) }
{
    disable_short_string_optimization(m_file_content);
}

auto utl::Source::read(std::filesystem::path&& path) -> Source
{
    if (std::ifstream file { path }) {
        return Source {
            std::move(path),
            std::string { std::istreambuf_iterator<char> { file }, {} },
        };
    }
    else if (std::filesystem::exists(path)) {
        throw exception("Failed to open file '{}'", path.string());
    }
    else {
        throw exception("File '{}' does not exist", path.string());
    }
}

auto utl::Source::path() const noexcept -> std::filesystem::path const&
{
    return m_file_path;
}

auto utl::Source::string() const noexcept -> std::string_view
{
    return m_file_content;
}

auto utl::Source_position::advance_with(char const c) noexcept -> void
{
    if (c == '\n') {
        ++line, column = 1;
    }
    else {
        ++column;
    }
}

utl::Source_view::Source_view(
    Wrapper<Source> const  source,
    std::string_view const string,
    Source_position const  start,
    Source_position const  stop) noexcept
    : source { source }
    , string { string }
    , start_position { start }
    , stop_position { stop }
{
    always_assert(start_position <= stop_position);
}

auto utl::Source_view::dummy() -> Source_view
{
    static auto               dummy_source_arena = Source::Arena::with_page_size(1);
    static wrapper auto const dummy_source
        = dummy_source_arena.wrap("[dummy]", "dummy file content");
    return Source_view { dummy_source, dummy_source->string(), {}, {} };
}

auto utl::Source_view::combine_with(Source_view const& other) const noexcept -> Source_view
{
    always_assert(source.is(other.source));

    if (other.string.empty()) {
        return *this;
    }
    if (string.empty()) {
        return other;
    }

    always_assert(std::invoke(std::less_equal {}, &string.front(), &other.string.back()));
    return Source_view {
        source,
        std::string_view { string.data(), other.string.data() + other.string.size() },
        start_position,
        other.stop_position,
    };
}
