#include <libutl/common/utilities.hpp>
#include <libutl/common/pooled_string.hpp>

utl::Pooled_string::Pooled_string(Relative_string const relative, std::string const* const pool)
    : m_relative { relative }
    , m_pool { pool }
{}

auto utl::Pooled_string::view() const noexcept -> std::string_view
{
    return m_relative.view_in(*m_pool);
}

auto utl::Pooled_string::size() const noexcept -> std::size_t
{
    return m_relative.length;
}

auto utl::Pooled_string::operator==(Pooled_string const& other) const -> bool
{
    return m_pool == other.m_pool && m_relative.offset == other.m_relative.offset
        && m_relative.length == other.m_relative.length;
}

auto utl::operator==(Pooled_string const& left, std::string_view const right) -> bool
{
    return left.view() == right;
}

auto utl::operator==(std::string_view const left, Pooled_string const& right) -> bool
{
    return left == right.view();
}

utl::String_pool::String_pool(std::size_t const initial_capacity)
    : m_string { std::make_unique<std::string>() }
{
    m_string->reserve(std::max(initial_capacity, sizeof(std::string) + 1));
}

utl::String_pool::String_pool() : String_pool { 2048 } {}

auto utl::String_pool::make(std::string_view const string) -> Pooled_string
{
    // TODO: examine searcher performance
    auto const it = std::search(
        m_string->begin(), m_string->end(), std::default_searcher { string.begin(), string.end() });
    if (it == m_string->end()) {
        return make_guaranteed_new_string(string);
    }
    return Pooled_string {
        Relative_string {
            .offset = static_cast<std::size_t>(std::distance(m_string->begin(), it)),
            .length = string.size(),
        },
        m_string.get(),
    };
}

auto utl::String_pool::make_guaranteed_new_string(std::string_view const string) -> Pooled_string
{
    Relative_string const relative { .offset = m_string->size(), .length = string.size() };
    m_string->append(string);
    return Pooled_string { relative, m_string.get() };
}
