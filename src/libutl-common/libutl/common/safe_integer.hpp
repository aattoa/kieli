#pragma once

#include <libutl/common/utilities.hpp>


APPLY_EXPLICIT_OBJECT_PARAMETER_HERE;

namespace utl {

    struct Safe_integer_error : std::exception {};

    template <Metastring message>
    struct Stateless_safe_integer_error : Safe_integer_error {
        [[nodiscard]]
        auto what() const noexcept -> char const* override {
            return message.view().data();
        }
    };

    struct Safe_integer_out_of_range final
        : Stateless_safe_integer_error<"utl::Safe_integer out of range"> {};
    struct Safe_integer_overflow final
        : Stateless_safe_integer_error<"utl::Safe_integer overflow"> {};
    struct Safe_integer_underflow final
        : Stateless_safe_integer_error<"utl::Safe_integer underflow"> {};
    struct Safe_integer_division_by_zero final
        : Stateless_safe_integer_error<"utl::Safe_integer division by zero"> {};


    // Overflow & underflow detection logic for addition, subtraction, multiplication, and division
    // taken from https://vladris.com/blog/2018/10/13/arithmetic-overflow-and-underflow.html


    template <std::integral T> [[nodiscard]]
    constexpr auto would_addition_overflow(T const a, T const b) noexcept -> bool {
        return (b >= 0) && (a > std::numeric_limits<T>::max() - b);
    }
    template <std::integral T> [[nodiscard]]
    constexpr auto would_addition_underflow(T const a, T const b) noexcept -> bool {
        return (b < 0) && (a < std::numeric_limits<T>::min() - b);
    }

    template <std::integral T> [[nodiscard]]
    constexpr auto would_subtraction_overflow(T const a, T const b) noexcept -> bool {
        return (b < 0) && (a > std::numeric_limits<T>::max() + b);
    }
    template <std::integral T> [[nodiscard]]
    constexpr auto would_subtraction_underflow(T const a, T const b) noexcept -> bool {
        return (b >= 0) && (a < std::numeric_limits<T>::min() + b);
    }

    template <std::integral T> [[nodiscard]]
    constexpr auto would_multiplication_overflow(T const a, T const b) noexcept -> bool {
        return b != 0 && (((b > 0) && (a > 0) && (a > std::numeric_limits<T>::max() / b)) ||
                          ((b < 0) && (a < 0) && (a < std::numeric_limits<T>::max() / b)));
    }
    template <std::integral T> [[nodiscard]]
    constexpr auto would_multiplication_underflow(T const a, T const b) noexcept -> bool {
        return b != 0 && (((b > 0) && (a < 0) && (a < std::numeric_limits<T>::min() / b)) ||
                          ((b < 0) && (a > 0) && (a > std::numeric_limits<T>::min() / b)));
    }

    template <std::integral T> [[nodiscard]]
    constexpr auto would_division_overflow(T const a, T const b) noexcept -> bool {
        return (a == std::numeric_limits<T>::min()) && (b == -1)
            && (a != 0);
    }

    template <std::integral X> [[nodiscard]]
    constexpr auto would_increment_overflow(X const x) noexcept -> bool {
        return x == std::numeric_limits<X>::max();
    }
    template <std::integral X> [[nodiscard]]
    constexpr auto would_decrement_underflow(X const x) noexcept -> bool {
        return x == std::numeric_limits<X>::min();
    }


    template <std::integral T>
    class [[nodiscard]] Safe_integer {
        T m_value {};
    public:
        using Underlying_integer = T;

        Safe_integer() noexcept = default;

        /*implicit*/ constexpr Safe_integer(std::integral auto const value) { // NOLINT
            if (std::in_range<T>(value))
                m_value = static_cast<T>(value);
            else
                throw Safe_integer_out_of_range {};
        }

        template <std::integral U> requires (!std::same_as<T, U>)
        explicit constexpr Safe_integer(Safe_integer<U> const other)
            : Safe_integer { other.get() } {}

        template <std::integral U> [[nodiscard]]
        explicit constexpr operator U() const {
            if (std::in_range<U>(m_value))
                return static_cast<U>(m_value);
            else
                throw Safe_integer_out_of_range {};
        }

        [[nodiscard]]
        constexpr auto get() const noexcept -> T {
            return m_value;
        }

        constexpr auto operator+=(Safe_integer const other) -> Safe_integer& {
            if (would_addition_overflow(m_value, other.m_value))
                throw Safe_integer_overflow {};
            if (would_addition_underflow(m_value, other.m_value))
                throw Safe_integer_underflow {};

            m_value += other.m_value;
            return *this;
        }
        constexpr auto operator+(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy += other;
        }

        constexpr auto operator-=(Safe_integer const other) -> Safe_integer& {
            if (would_subtraction_overflow(m_value, other.m_value)) [[unlikely]]
                throw Safe_integer_overflow {};
            if (would_subtraction_underflow(m_value, other.m_value)) [[unlikely]]
                throw Safe_integer_underflow {};

            m_value -= other.m_value;
            return *this;
        }
        constexpr auto operator-(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy -= other;
        }

        constexpr auto operator*=(Safe_integer const other) -> Safe_integer& {
            if (would_multiplication_overflow(m_value, other.m_value)) [[unlikely]]
                throw Safe_integer_overflow {};
            if (would_multiplication_underflow(m_value, other.m_value)) [[unlikely]]
                throw Safe_integer_underflow {};

            m_value *= other.m_value;
            return *this;
        }
        constexpr auto operator*(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy *= other;
        }

        constexpr auto operator/=(Safe_integer const other) -> Safe_integer& {
            if (other.m_value == 0) [[unlikely]]
                throw Safe_integer_division_by_zero {};
            if (would_division_overflow(m_value, other.m_value)) [[unlikely]]
                throw Safe_integer_overflow {};

            m_value /= other.m_value;
            return *this;
        }
        constexpr auto operator/(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy /= other;
        }

        constexpr auto operator%=(Safe_integer const other) -> Safe_integer& {
            if (would_division_overflow(m_value, other.m_value)) [[unlikely]]
                throw Safe_integer_overflow {};

            m_value %= other.m_value;
            return *this;
        }
        constexpr auto operator%(Safe_integer const other) -> Safe_integer {
            auto copy = *this;
            return copy %= other;
        }

        constexpr auto operator++() -> Safe_integer& {
            if (would_increment_overflow(m_value)) [[unlikely]]
                throw Safe_integer_overflow {};

            ++m_value;
            return *this;
        }
        constexpr auto operator++(int) -> Safe_integer {
            auto copy = *this;
            ++*this;
            return copy;
        }

        constexpr auto operator--() -> Safe_integer& {
            if (would_decrement_underflow(m_value)) [[unlikely]]
                throw Safe_integer_underflow {};

            --m_value;
            return *this;
        }
        constexpr auto operator--(int) -> Safe_integer {
            auto copy = *this;
            --*this;
            return copy;
        }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept {
            return m_value != 0;
        }

        [[nodiscard]]
        auto operator==(Safe_integer const&) const noexcept -> bool = default;
        [[nodiscard]]
        auto operator<=>(Safe_integer const&) const noexcept -> std::strong_ordering = default;
    };


    using Safe_i8  = Safe_integer<I8 >;
    using Safe_i16 = Safe_integer<I16>;
    using Safe_i32 = Safe_integer<I32>;
    using Safe_i64 = Safe_integer<I64>;

    using Safe_u8  = Safe_integer<U8 >;
    using Safe_u16 = Safe_integer<U16>;
    using Safe_u32 = Safe_integer<U32>;
    using Safe_u64 = Safe_integer<U64>;

    using Safe_usize = Safe_integer<Usize>;
    using Safe_isize = Safe_integer<Isize>;

}


template <class T>
struct fmt::formatter<utl::Safe_integer<T>> : fmt::formatter<T> {
    auto format(utl::Safe_integer<T> const value, auto& context) { // NOLINT
        return formatter<T>::format(value.get(), context);
    }
};