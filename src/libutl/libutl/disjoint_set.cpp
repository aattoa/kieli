#include <libutl/utilities.hpp>
#include <libutl/disjoint_set.hpp>

utl::Disjoint_set::Disjoint_set(std::size_t const size)
    : m_parents(std::from_range, std::views::iota(0UZ, size))
    , m_weights(size, 1UZ)
{}

void utl::Disjoint_set::merge(std::size_t x, std::size_t y)
{
    x = find(x);
    y = find(y);
    if (x == y) {
        return;
    }
    if (m_weights.at(y) < m_weights.at(x)) {
        std::swap(x, y);
    }
    m_parents.at(y) = x;
    m_weights.at(x) += m_weights.at(y);
}

auto utl::Disjoint_set::add() -> std::size_t
{
    std::size_t const index = m_parents.size();
    m_parents.push_back(index);
    m_weights.push_back(1);
    return index;
}

auto utl::Disjoint_set::find(std::size_t x) -> std::size_t
{
    for (;;) {
        std::size_t& parent = m_parents.at(x);
        if (parent == x) {
            return parent;
        }
        x = parent = m_parents.at(parent);
    }
}

auto utl::Disjoint_set::find_without_compressing(std::size_t const x) const -> std::size_t
{
    std::size_t const parent = m_parents.at(x);
    return x == parent ? x : find_without_compressing(parent);
}
