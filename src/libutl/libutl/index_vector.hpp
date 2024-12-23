#pragma once

#include <libutl/utilities.hpp>

namespace utl {

    // A type that models `vector_index` can be used as the index type of `Index_vector`.
    template <typename Index>
    concept vector_index = requires(Index const index) {
        { index.get() } -> std::same_as<std::size_t>;
        requires std::is_constructible_v<Index, std::size_t>;
    };

    // Wraps `Integral`. Specializations model `vector_index`.
    template <typename Uniqueness_tag, std::integral Integral = std::size_t>
    class Vector_index {
        Integral m_value;
    public:
        explicit constexpr Vector_index(std::size_t const value)
            noexcept(losslessly_convertible_to<std::size_t, Integral>)
            : m_value(safe_cast<Integral>(value))
        {}

        [[nodiscard]] constexpr auto get(this Vector_index const self)
            noexcept(losslessly_convertible_to<Integral, std::size_t>) -> std::size_t
        {
            return safe_cast<std::size_t>(self.m_value);
        }

        auto operator==(Vector_index const& other) const -> bool = default;
        auto operator<=>(Vector_index const& other) const        = default;
    };

    // Wraps a `std::vector`. The subscript operator uses `Index` instead of `size_t`.
    template <vector_index Index, typename T, typename Allocator = std::allocator<T>>
    struct [[nodiscard]] Index_vector {
        std::vector<T, Allocator> underlying;

        [[nodiscard]] constexpr auto operator[](this auto&& self, Index index) -> decltype(auto)
        {
            return std::forward_like<decltype(self)>(self.underlying.at(index.get()));
        }

        template <typename... Args>
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
    };

    template <vector_index Index, typename T, typename Allocator = std::allocator<T>>
    struct Index_arena {
        Index_vector<Index, T, Allocator> index_vector;
        std::vector<Index>                free_indices;

        void kill(Index const index)
        {
            free_indices.push_back(index);
        }

        template <typename... Args>
        [[nodiscard]] constexpr auto push(Args&&... args) -> Index
            requires std::is_constructible_v<T, Args...>
        {
            if (free_indices.empty()) {
                return index_vector.push(std::forward<Args>(args)...);
            }
            Index const index = free_indices.back();
            free_indices.pop_back();
            index_vector[index] = T(std::forward<Args>(args)...);
            return index;
        }
    };

    struct Hash_vector_index : std::hash<std::size_t> {
        auto operator()(vector_index auto const& index) const noexcept -> std::size_t
        {
            return std::hash<std::size_t>::operator()(index.get());
        }
    };

} // namespace utl
