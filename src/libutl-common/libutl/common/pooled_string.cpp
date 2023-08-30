#include <libutl/common/utilities.hpp>
#include <libutl/common/pooled_string.hpp>

utl::Pooled_string::Pooled_string(
    utl::Usize const index, utl::Usize const size, std::string const* const pool)
    : m_index { index }
    , m_size { size }
    , m_pool { pool }
{}

auto utl::Pooled_string::view() const noexcept -> std::string_view
{
    return { m_pool->c_str() + m_index, m_size };
}

auto utl::Pooled_string::size() const noexcept -> Usize
{
    return m_size;
}

auto utl::operator==(Pooled_string const& left, std::string_view const right) -> bool
{
    return left.view() == right;
}

auto utl::operator==(std::string_view const left, Pooled_string const& right) -> bool
{
    return left == right.view();
}

utl::String_pool::String_pool(Usize const initial_capacity)
    : m_string { std::make_unique<std::string>() }
{
    m_string->reserve(std::max(initial_capacity, sizeof(std::string) + 1));
}

utl::String_pool::String_pool() : String_pool { 2048 } {}

auto utl::String_pool::make(std::string_view const string) -> Pooled_string
{
    // The searcher overload is faster than the bare iterator overload for some reason
    // TODO: re-examine searcher performance
    auto it = std::search(
        m_string->begin(), m_string->end(), std::default_searcher { string.begin(), string.end() });
    if (it == m_string->end()) {
        return make_guaranteed_new_string(string);
    }
    else {
        return Pooled_string {
            unsigned_distance(m_string->begin(), it),
            string.size(),
            m_string.get(),
        };
    }
}

auto utl::String_pool::make_guaranteed_new_string(std::string_view const string) -> Pooled_string
{
    Usize const index = m_string->size();
    m_string->append(string);
    return Pooled_string { index, string.size(), m_string.get() };
}
