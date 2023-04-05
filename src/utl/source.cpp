#include "utl/utilities.hpp"
#include "source.hpp"


utl::Source::Source(Filename&& filename)
    : m_filename { std::move(filename.string) }
{
    disable_short_string_optimization(m_filename);
    disable_short_string_optimization(m_contents);

    if (std::ifstream file { m_filename })
        m_contents.assign(std::istreambuf_iterator<char> { file }, {});
    else
        throw exception("Failed to open file '{}'", m_filename);
}

utl::Source::Source(Filename&& filename, std::string&& file_content)
    : m_filename { std::move(filename.string) }
    , m_contents { std::move(file_content) }
{
    disable_short_string_optimization(m_filename);
    disable_short_string_optimization(m_contents);
}

auto utl::Source::filename() const noexcept -> std::string_view {
    return m_filename;
}
auto utl::Source::string() const noexcept -> std::string_view {
    return m_contents;
}

auto utl::Source_position::increment_with(char const c) noexcept -> void {
    if (c == '\n') {
        ++line;
        column = 1;
    }
    else {
        ++column;
    }
}

auto utl::Source_view::operator+(Source_view const& other) const noexcept -> Source_view {
    if (other.string.empty()) {
        return *this;
    }
    else if (string.empty()) {
        return other;
    }
    else {
        always_assert(&string.front() <= &other.string.back());

        return Source_view {
            std::string_view {
                string.data(),
                other.string.data() + other.string.size()
            },
            start_position,
            other.stop_position
        };
    }
}


static_assert(std::is_trivially_copyable_v<utl::Source_view>);
static_assert(utl::Source_position { 4, 5 } < utl::Source_position { 9, 2 });
static_assert(utl::Source_position { 5, 2 } < utl::Source_position { 5, 3 });
static_assert(utl::Source_position { 3, 2 } > utl::Source_position { 2, 3 });


DEFINE_FORMATTER_FOR(utl::Source_position) {
    return fmt::format_to(context.out(), "{}:{}", value.line, value.column);
}