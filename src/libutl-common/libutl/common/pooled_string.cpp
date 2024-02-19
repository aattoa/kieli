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
    return m_pool == other.m_pool //
        && m_relative.offset == other.m_relative.offset
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

utl::String_pool::String_pool()
{
    m_string = std::make_unique<std::string>();
    disable_short_string_optimization(*m_string);
}

auto utl::String_pool::with_initial_capacity(std::size_t const capacity) -> String_pool
{
    String_pool pool;
    pool.m_string->reserve(std::max(pool.m_string->capacity(), capacity));
    return pool;
}

auto utl::String_pool::make(std::string_view const string) -> Pooled_string
{
    auto const it = std::search(m_string->begin(), m_string->end(), string.begin(), string.end());
    if (it == m_string->end()) {
        return make_guaranteed_new_string(string);
    }
    return Pooled_string {
        Relative_string {
            .offset = safe_cast<std::size_t>(std::distance(m_string->begin(), it)),
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
