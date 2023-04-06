#pragma once

#include "utl/utilities.hpp"


namespace utl {

    template <
        class Key,
        class Value,
        class Key_equal = std::equal_to<void>,
        class Container = std::vector<Pair<Key, Value>>
    >
    class Flatmap {
        Container m_container;
    public:
        using key_type    = Key;
        using mapped_type = Value;
        using value_type  = typename Container::value_type;

        Flatmap() = default;

        explicit constexpr Flatmap(Container&& container) noexcept
            : m_container { std::move(container) } {}

        template <class K, class V>
        constexpr auto add_or_assign(K&& new_k, V&& new_v) & -> Value&
            requires std::constructible_from<Key, K>
                  && std::assignable_from<Key&, K>
                  && std::constructible_from<Value, V>
                  && std::assignable_from<Value&, V>
                  && std::is_invocable_r_v<bool, Key_equal&&, Key const&, decltype(std::as_const(new_k))>
        {
            for (auto& [k, v] : m_container) {
                if (std::invoke(Key_equal {}, std::as_const(k), std::as_const(new_k)))
                    return v = std::forward<V>(new_v);
            }
            return m_container.emplace_back(std::forward<K>(new_k), std::forward<V>(new_v)).second;
        }

        template <class K, class V>
        constexpr auto add_new_or_abort(K&& new_k, V&& new_v, std::source_location const caller = std::source_location::current()) & -> Value&
            requires std::constructible_from<Key, K>
                  && std::constructible_from<Value, V>
                  && std::is_invocable_r_v<bool, Key_equal&&, Key const&, decltype(std::as_const(new_k))>
        {
            for (auto& [k, v] : m_container) {
                if (std::invoke(Key_equal {}, std::as_const(k), std::as_const(new_k))) [[unlikely]]
                    abort("utl::Flatmap::add_new_or_abort: key already present in flatmap", caller);
            }
            return m_container.emplace_back(std::forward<K>(new_k), std::forward<V>(new_v)).second;
        }

        template <class K> [[nodiscard]]
        constexpr auto find(K const& key) const
            noexcept(std::is_nothrow_invocable_r_v<bool, Key_equal&&, Key const&, K const&>) -> Value const*
            requires std::is_invocable_r_v<bool, Key_equal&&, Key const&, K const&>
        {
            for (auto& [k, v] : m_container) {
                if (std::invoke(Key_equal {}, k, key))
                    return std::addressof(v);
            }
            return nullptr;
        }
        template <class K> [[nodiscard]]
        constexpr auto find(K const& key)
            noexcept(noexcept(const_cast<Flatmap const*>(this)->find(key))) -> Value*
            requires requires { const_cast<Flatmap const*>(this)->find(key); }
        {
            return const_cast<Value*>(const_cast<Flatmap const*>(this)->find(key));
        }

        [[nodiscard]]
        constexpr auto size() const noexcept -> Usize {
            return m_container.size();
        }
        [[nodiscard]]
        constexpr auto empty() const noexcept -> bool {
            return m_container.empty();
        }
        [[nodiscard]]
        constexpr auto span() const noexcept -> std::span<value_type const>
            requires std::contiguous_iterator<typename Container::const_iterator>
        {
            return { begin(), end() };
        }

        [[nodiscard]] constexpr auto container() const noexcept -> Container const& { return m_container; }
        [[nodiscard]] constexpr auto container()       noexcept -> Container      & { return m_container; }

        constexpr auto begin()       { return m_container.begin(); }
        constexpr auto begin() const { return m_container.begin(); }
        constexpr auto end()         { return m_container.end(); }
        constexpr auto end()   const { return m_container.end(); }

        [[nodiscard]]
        auto operator==(Flatmap const&) const noexcept -> bool = default;
    };

    template <class Key, class Value, template <class...> class Container>
    Flatmap(Container<Pair<Key, Value>>&&)
        -> Flatmap<Key, Value, std::equal_to<void>, Container<Pair<Key, Value>>>;

}