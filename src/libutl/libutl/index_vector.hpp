#pragma once

#include <libutl/utilities.hpp>

namespace utl {

    // A type that models `vector_index` can be used as the index type of `Index_vector`.
    template <class Index>
    concept vector_index = requires(Index const index) {
        { index.get() } -> std::same_as<std::size_t>;
        requires std::is_constructible_v<Index, std::size_t>;
    };

    // Wraps a `std::size_t`. Specializations model `vector_index`.
    template <class Uniqueness_tag>
    class Vector_index {
        std::size_t m_value;
    public:
        explicit constexpr Vector_index(std::size_t const value) noexcept : m_value { value } {}

        [[nodiscard]] constexpr auto get(this Vector_index const self) noexcept -> std::size_t
        {
            return self.m_value;
        };

        auto operator==(Vector_index const& other) const -> bool = default;
        auto operator<=>(Vector_index const& other) const        = default;
    };

    // Wraps a `std::vector`. The subscript operator uses `Index` instead of `size_t`.
    template <vector_index Index, class T, class Allocator = std::allocator<T>>
    struct [[nodiscard]] Index_vector {
        std::vector<T, Allocator> underlying;

        [[nodiscard]] constexpr auto operator[](this auto&& self, Index index) -> decltype(auto)
        {
            return std::forward_like<decltype(self)>(self.underlying.at(index.get()));
        }

        template <class... Args>
        [[nodiscard]] constexpr auto push(Args&&... args) -> Index
            requires std::is_constructible_v<T, Args...>
        {
            underlying.emplace_back(std::forward<Args>(args)...);
            return Index(size() - 1);
        }

        [[nodiscard]] constexpr auto size() const noexcept -> std::size_t
        {
            return underlying.size();
        }

        [[nodiscard]] constexpr auto empty() const noexcept -> bool
        {
            return underlying.empty();
        }

        auto operator==(Index_vector const& other) const -> bool = default;
        auto operator<=>(Index_vector const& other) const        = default;
    };

} // namespace utl
