#include <libutl/common/utilities.hpp>
#include <libutl/source/source.hpp>
#include <cpputil/io.hpp>

namespace {
    constexpr auto is_valid_position(utl::Source_position const position) noexcept -> bool
    {
        return position.line != 0 && position.column != 0;
    }

    auto find_position(std::string_view const string, utl::Source_position const position)
        -> std::string_view::iterator
    {
        cpputil::always_assert(is_valid_position(position));
        auto it = string.begin();
        for (std::size_t i = 1; i != position.line; ++i) {
            it = std::find(it, string.end(), '\n');
            cpputil::always_assert(it != string.end());
            ++it;
        }
        cpputil::always_assert(std::cmp_less(position.column - 1, std::distance(it, string.end())));
        return it + position.column - 1;
    }
} // namespace

utl::Source::Source(std::filesystem::path&& file_path, std::string&& file_content)
    : m_file_path { std::move(file_path) }
    , m_file_content { std::move(file_content) }
{
    disable_short_string_optimization(m_file_content);
}

auto utl::Source::read(std::filesystem::path&& path) -> std::expected<Source, Read_error>
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

auto utl::Source_position::advance_with(char const c) noexcept -> void
{
    assert(is_valid_position(*this));
    if (c == '\n') {
        ++line;
        column = 1;
    }
    else {
        ++column;
    }
}

auto utl::Source_range::in(std::string_view const string) const -> std::string_view
{
    // TODO: maybe avoid traversing the string twice
    return std::string_view { find_position(string, start), find_position(string, stop) + 1 };
}

auto utl::Source_range::up_to(Source_range const other) const -> Source_range
{
    cpputil::always_assert(start <= other.stop);
    return Source_range { start, other.stop };
};
