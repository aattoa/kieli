#pragma once

#include <libutl/utilities.hpp>

namespace utl {

    template <
        class Key,
        class Value,
        class Key_equal = std::equal_to<void>,
        class Container = std::vector<std::pair<Key, Value>>>
    class Flatmap {
        Container m_container;
    public:
        using key_type    = Key;
        using mapped_type = Value;
        using value_type  = Container::value_type;

        Flatmap() = default;

        explicit constexpr Flatmap(Container&& container) noexcept
            : m_container { std::move(container) }
        {}

        template <class K, class V>
        constexpr auto add_or_assign(K&& key, V&& value) & -> mapped_type&
            requires std::constructible_from<key_type, K> && std::assignable_from<key_type&, K>
                  && std::constructible_from<mapped_type, V>
                  && std::assignable_from<mapped_type&, V>
                  && std::is_invocable_r_v<
                         bool,
                         Key_equal&&,
                         key_type const&,
                         decltype(std::as_const(key))>
        {
            if (mapped_type* const existing = find(std::as_const(key))) {
                return *existing = std::forward<V>(value);
            }
            return m_container.emplace_back(std::forward<K>(key), std::forward<V>(value)).second;
        }

        template <class K, class V>
        constexpr auto add_new_or_abort(
            K&&                        key,
            V&&                        value,
            std::source_location const caller = std::source_location::current()) & -> mapped_type&
            requires std::constructible_from<key_type, K> && std::constructible_from<mapped_type, V>
                  && std::is_invocable_r_v<
                         bool,
                         Key_equal&&,
                         key_type const&,
                         decltype(std::as_const(key))>
        {
            if (find(std::as_const(key)) != nullptr) {
                cpputil::abort(
                    "utl::Flatmap::add_new_or_abort: key already present in flatmap", caller);
            }
            return m_container.emplace_back(std::forward<K>(key), std::forward<V>(value)).second;
        }

        template <class K, class V>
        constexpr auto add_new_unchecked(K&& key, V&& value) & -> mapped_type&
            requires std::constructible_from<key_type, K> && std::constructible_from<mapped_type, V>
                  && std::is_invocable_r_v<
                         bool,
                         Key_equal&&,
                         key_type const&,
                         decltype(std::as_const(key))>
        {
            assert(find(std::as_const(key)) == nullptr);
            return m_container.emplace_back(std::forward<K>(key), std::forward<V>(value)).second;
        }

        template <class Self, class K>
        [[nodiscard]] constexpr auto find(this Self& self, K const& key)
            noexcept(std::is_nothrow_invocable_v<Key_equal&&, key_type const&, K const&>)
                -> std::conditional_t<std::is_const_v<Self>, mapped_type const, mapped_type>*
            requires std::is_invocable_r_v<bool, Key_equal&&, key_type const&, K const&>
        {
            for (auto& [k, v] : self.m_container) {
                if (std::invoke(Key_equal {}, k, key)) {
                    return std::addressof(v);
                }
            }
            return nullptr;
        }

        template <class K>
        [[nodiscard]] constexpr auto operator[](this auto&& self, K const& key) -> decltype(auto)
            requires std::is_invocable_r_v<bool, Key_equal&&, key_type const&, K const&>
        {
            if (auto* const pointer = self.find(key)) {
                return std::forward_like<decltype(self)>(*pointer);
            }
            throw std::out_of_range { "utl::Flatmap::operator[] out of range" };
        }

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
        {
            return m_container.size();
        }

        [[nodiscard]] constexpr auto empty() const noexcept -> bool
        {
            return m_container.empty();
        }

        [[nodiscard]] constexpr auto span() const noexcept -> std::span<value_type const>
            requires std::contiguous_iterator<typename Container::const_iterator>
        {
            return { begin(), end() };
        }

        [[nodiscard]] constexpr auto container(this auto&& self) noexcept -> decltype(auto)
        {
            return std::forward_like<decltype(self)>(self.m_container);
        }

        [[nodiscard]] constexpr auto begin(this auto& self)
        {
            return self.m_container.begin();
        }

        [[nodiscard]] constexpr auto end(this auto& self)
        {
            return self.m_container.end();
        }

        [[nodiscard]] auto operator==(Flatmap const&) const -> bool = default;
    };

    template <class Container>
    Flatmap(Container) -> Flatmap<
                           std::tuple_element_t<0, typename Container::value_type>,
                           std::tuple_element_t<1, typename Container::value_type>,
                           std::equal_to<void>,
                           Container>;

} // namespace utl
