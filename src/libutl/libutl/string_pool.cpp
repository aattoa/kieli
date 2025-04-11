#include <libutl/utilities.hpp>
#include <libutl/string_pool.hpp>

auto ki::utl::String_pool::make(std::string owned) -> String_id
{
    if (auto const it = m_map.find(owned); it != m_map.end()) {
        return it->second;
    }
    auto const id = m_vec.push(owned);
    m_map.insert_or_assign(std::move(owned), id);
    return id;
}

auto ki::utl::String_pool::make(std::string_view borrowed) -> String_id
{
    if (auto const it = m_map.find(borrowed); it != m_map.end()) {
        return it->second;
    }
    std::string copy(borrowed);
    auto const  id = m_vec.push(copy);
    m_map.insert_or_assign(std::move(copy), id);
    return id;
}

auto ki::utl::String_pool::get(String_id id) const -> std::string_view
{
    return m_vec[id];
}
