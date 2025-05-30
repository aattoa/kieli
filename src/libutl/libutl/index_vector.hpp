#ifndef KIELI_LIBUTL_INDEX_VECTOR
#define KIELI_LIBUTL_INDEX_VECTOR

#include <libutl/utilities.hpp>
#include <cpputil/num.hpp>

namespace ki::utl {

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
            noexcept(cpputil::num::losslessly_convertible<std::size_t, Integral>)
            : m_value(cpputil::num::safe_cast<Integral>(value))
        {}

        [[nodiscard]] constexpr auto get(this Vector_index const self)
            noexcept(cpputil::num::losslessly_convertible<Integral, std::size_t>) -> std::size_t
        {
            return cpputil::num::safe_cast<std::size_t>(self.m_value);
        }

        auto operator==(Vector_index const& other) const -> bool = default;
        auto operator<=>(Vector_index const& other) const        = default;
    };

    // Wraps a `std::vector`. The subscript operator uses `Index` instead of `size_t`.
    template <vector_index Index, typename T, typename Allocator = std::allocator<T>>
    class [[nodiscard]] Index_vector {
        std::vector<T, Allocator> underlying;
    public:
        [[nodiscard]] constexpr auto operator[](this auto&& self, Index index) -> decltype(auto)
        {
            return std::forward_like<decltype(self)>(self.underlying.at(index.get()));
        }

        template <typename... Args>
        [[nodiscard]] constexpr auto push(Args&&... args) -> Index
            requires std::is_constructible_v<T, Args...>
        {
            underlying.emplace_back(std::forward<Args>(args)...);
            return Index(underlying.size() - 1);
        }
    };

    struct Hash_vector_index {
        static auto operator()(vector_index auto const& index) noexcept -> std::size_t
        {
            return index.get();
        }
    };

} // namespace ki::utl

#endif // KIELI_LIBUTL_INDEX_VECTOR
