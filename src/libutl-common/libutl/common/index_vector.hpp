#pragma once

#include <libutl/common/utilities.hpp>

namespace utl {

    // A type that models `vector_index` can be used as the index type of `Index_vector`.
    template <class Index>
    concept vector_index = requires(Index const index) {
        // clang-format off
        { index.get() } -> std::same_as<std::size_t>;
        // clang-format on
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

    // Wraps a `std::vector`. The subscript operator uses `Index` instead of `std::size_t`.
    template <vector_index Index, class T, class Allocator = std::allocator<T>>
    struct [[nodiscard]] Index_vector {
        std::vector<T, Allocator> underlying;

        constexpr auto operator[](this auto&& self, Index const index) -> decltype(auto)
        {
            return std::forward_like<decltype(self)>(self.underlying.at(index.get()));
        }

        auto operator==(Index_vector const& other) const -> bool = default;
        auto operator<=>(Index_vector const& other) const        = default;
    };

} // namespace utl
