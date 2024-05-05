#include <libutl/common/utilities.hpp>
#include <libutl/common/source.hpp>
#include <cpputil/io.hpp>

namespace {
    auto find_position(std::string_view const string, utl::Source_position const position)
        -> std::string_view::iterator
    {
        cpputil::always_assert(position.is_valid());
        auto it = string.begin();
        for (std::size_t i = 1; i != position.line; ++i) {
            it = std::find(it, string.end(), '\n');
            cpputil::always_assert(it != string.end());
            ++it;
        }
        cpputil::always_assert(std::cmp_less(position.column - 1, std::distance(it, string.end())));
        return it + position.column - 1;
    }

    auto advance_position_up_to(
        utl::Source_position             start,
        utl::Source_position const       stop,
        std::string_view::iterator       it,
        std::string_view::iterator const end) -> std::string_view::iterator
    {
        while (start != stop) {
            cpputil::always_assert(it != end);
            start.advance_with(*it++);
        }
        return it;
    }
} // namespace

utl::Source::Source(std::filesystem::path&& path, std::string&& content)
    : m_file_path { std::move(path) }
    , m_file_content { std::move(content) }
{
    disable_short_string_optimization(m_file_content);
}

auto utl::Source::read(std::filesystem::path path) -> std::expected<Source, Read_error>
{
    if (auto file = cpputil::io::File::open_read(path.c_str())) {
        if (auto content = cpputil::io::read(file.get())) {
            disable_short_string_optimization(content.value());
            return Source { std::move(path), std::move(content.value()) };
        }
        return std::unexpected { Read_error::failed_to_read };
    }
    if (std::filesystem::exists(path)) {
        return std::unexpected { Read_error::failed_to_open };
    }
    return std::unexpected { Read_error::does_not_exist };
}

auto utl::Source::path() const noexcept -> std::filesystem::path const&
{
    return m_file_path;
}

auto utl::Source::string() const noexcept -> std::string_view
{
    return m_file_content;
}

auto utl::Source_position::advance_with(char const c) -> void
{
    assert(is_valid());
    if (c == '\n') {
        ++line;
        column = 1;
    }
    else {
        ++column;
    }
}

auto utl::Source_position::is_valid() const noexcept -> bool
{
    return line != 0 && column != 0;
}

auto utl::Source_range::in(std::string_view const string) const -> std::string_view
{
    cpputil::always_assert(start <= stop);
    auto const first = find_position(string, start);
    auto const last  = advance_position_up_to(start, stop, first, string.end());
    return std::string_view { first, last + 1 };
}

auto utl::Source_range::up_to(Source_range const other) const -> Source_range
{
    cpputil::always_assert(start <= other.stop);
    return Source_range { start, other.stop };
};

auto utl::Source_range::dummy() noexcept -> Source_range
{
    return Source_range { Source_position {}, Source_position {} };
}
