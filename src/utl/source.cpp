#include "utl/utilities.hpp"
#include "source.hpp"


utl::Source::Source(std::string&& name)
    : m_filename { std::move(name) }
{
    if (std::ifstream file { m_filename }) {
        m_contents.reserve(sizeof m_contents);
        m_contents.assign(std::istreambuf_iterator<char> { file }, {});
    }
    else {
        throw Exception { std::format("The file '{}' could not be opened", m_filename) };
    }
}

utl::Source::Source(Mock_tag const mock_tag, std::string&& contents)
    : m_filename { std::format("[{}]", mock_tag.filename) }
    , m_contents { std::move(contents) }
{
    m_contents.reserve(sizeof contents); // Disable SSO
}

auto utl::Source::name() const noexcept -> std::string_view {
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
        return Source_view { string, start_position, stop_position };
    }
    else if (string.empty()) {
        return other + *this;
    }
    else {
        utl::always_assert(&string.front() <= &other.string.back());

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
    return std::format_to(context.out(), "{}:{}", value.line, value.column);
}