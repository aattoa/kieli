#pragma once

#include "utl/utilities.hpp"


namespace utl {

    enum class Flatmap_strategy { store_keys, hash_keys };


    template <
        class Key,
        class Value,
        Flatmap_strategy = std::is_trivially_copyable_v<Key>
            ? Flatmap_strategy::store_keys
            : Flatmap_strategy::hash_keys,
        template <class...> class Container = std::vector
    >
    class Flatmap;


    template <class K, class V, template <class...> class Container>
    class [[nodiscard]] Flatmap<K, V, Flatmap_strategy::store_keys, Container> {
        Container<Pair<K, V>> m_pairs;
    public:
        using Key   = K;
        using Value = V;

        constexpr Flatmap(decltype(m_pairs)&& pairs) noexcept
            : m_pairs { std::move(pairs) } {}

        Flatmap() noexcept = default;

        template <class A, class B>
        constexpr auto add(A&& k, B&& v) &
            noexcept(std::is_nothrow_move_constructible_v<Pair<K, V>>) -> V*
            requires std::is_constructible_v<K, A> && std::is_constructible_v<V, B>
        {
            return std::addressof(m_pairs.emplace_back(std::forward<A>(k), std::forward<B>(v)).second);
        }

        [[nodiscard]]
        constexpr auto find(std::equality_comparable_with<K> auto const& key) const&
            noexcept(noexcept(key == std::declval<K const&>())) -> V const*
        {
            for (auto& [k, v] : m_pairs) {
                if (key == k) {
                    return std::addressof(v);
                }
            }
            return nullptr;
        }
        [[nodiscard]]
        constexpr auto find(std::equality_comparable_with<K> auto const& key) &
            noexcept(noexcept(key == std::declval<K const&>())) -> V*
        {
            return const_cast<V*>(const_cast<Flatmap const*>(this)->find(key));
        }

        [[nodiscard]]
        constexpr auto span() const noexcept -> std::span<Pair<K, V> const> {
            return m_pairs;
        }
        [[nodiscard]]
        constexpr auto size() const noexcept -> Usize {
            return m_pairs.size();
        }
        [[nodiscard]]
        constexpr auto empty() const noexcept -> bool {
            return m_pairs.empty();
        }

        constexpr auto begin()       { return m_pairs.begin(); }
        constexpr auto begin() const { return m_pairs.begin(); }
        constexpr auto end()         { return m_pairs.end(); }
        constexpr auto end()   const { return m_pairs.end(); }

        constexpr auto container()      & noexcept -> decltype(m_pairs)      & { return m_pairs; }
        constexpr auto container() const& noexcept -> decltype(m_pairs) const& { return m_pairs; }

        auto container()      && = delete;
        auto container() const&& = delete;

        auto hash() const
            noexcept(noexcept(::utl::hash(m_pairs))) -> Usize
            requires hashable<K> && hashable<V>
        {
            return ::utl::hash(m_pairs);
        }

        auto operator==(Flatmap const&) const noexcept -> bool = default;
    };


    template <class K, class V, template <class...> class Container>
    class [[nodiscard]] Flatmap<K, V, Flatmap_strategy::hash_keys, Container> :
        Flatmap<Usize, V, Flatmap_strategy::store_keys, Container>
    {
        using Parent = Flatmap<Usize, V, Flatmap_strategy::store_keys, Container>;
    public:
        using Parent::Parent;
        using Parent::operator=;
        using Parent::operator==;
        using Parent::span;
        using Parent::size;
        using Parent::empty;
        using Parent::container;
        using Parent::hash;

        // The member functions aren't constexpr because the standard library does not provide constexpr hashing support.

        auto add(K const& k, V&& v) &
            noexcept(std::is_nothrow_move_constructible_v<V> && noexcept(::utl::hash(k))) -> V*
        {
            return Parent::add(::utl::hash(k), std::move(v));
        }

        [[nodiscard]]
        auto find(K const& k) const&
            noexcept(noexcept(::utl::hash(k))) -> V const*
        {
            return Parent::find(::utl::hash(k));
        }
        [[nodiscard]]
        auto find(K const& k) &
            noexcept(noexcept(::utl::hash(k))) -> V*
        {
            return Parent::find(::utl::hash(k));
        }
    };

}
