#pragma once

#include "utl/utilities.hpp"


APPLY_EXPLICIT_OBJECT_PARAMETER_HERE;

namespace utl {

    struct Safe_integer_error : std::exception {};

    struct Safe_integer_out_of_range final : Safe_integer_error {
        auto what() const noexcept -> char const* override {
            return "utl::Safe_integer out of range";
        }
    };
    struct Safe_integer_overflow final : Safe_integer_error {
        auto what() const noexcept -> char const* override {
            return "utl::Safe_integer overflow";
        }
    };
    struct Safe_integer_underflow final : Safe_integer_error {
        auto what() const noexcept -> char const* override {
            return "utl::Safe_integer underflow";
        }
    };
    struct Safe_integer_division_by_zero final : Safe_integer_error {
        auto what() const noexcept -> char const* override {
            return "utl::Safe_integer division by zero";
        }
    };


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
        return b != 0
            ? ((b > 0) && (a > 0) && (a > std::numeric_limits<T>::max() / b)) ||
              ((b < 0) && (a < 0) && (a < std::numeric_limits<T>::max() / b))
            : false;
    }
    template <std::integral T> [[nodiscard]]
    constexpr auto would_multiplication_underflow(T const a, T const b) noexcept -> bool {
        return b != 0
            ? ((b > 0) && (a < 0) && (a < std::numeric_limits<T>::min() / b)) ||
              ((b < 0) && (a > 0) && (a > std::numeric_limits<T>::min() / b))
            : false;
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
        T value = 0;
    public:
        Safe_integer() noexcept = default;

        static constexpr auto make_unchecked(std::integral auto const value) noexcept -> Safe_integer {
            assert(std::in_range<T>(value)); // Only checked in debug mode
            Safe_integer result;
            result.value = static_cast<T>(value);
            return result;
        }

        /* implicit */ constexpr Safe_integer(std::integral auto const value)
            : value { static_cast<T>(value) }
        {
            if (!std::in_range<T>(value)) [[unlikely]] {
                throw Safe_integer_out_of_range {};
            }
        }

        template <std::integral U> [[nodiscard]]
        explicit constexpr operator U() const {
            if (std::in_range<U>(value))
                return static_cast<U>(value);
            else
                throw Safe_integer_out_of_range {};
        }

        [[nodiscard]]
        constexpr auto get() const noexcept -> T {
            return value;
        }

        constexpr auto operator+=(Safe_integer const other) -> Safe_integer& {
            if (would_addition_overflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_overflow {};
            }
            if (would_addition_underflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_underflow {};
            }
            value += other.value;
            return *this;
        }
        constexpr auto operator+(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy += other;
        }

        constexpr auto operator-=(Safe_integer const other) -> Safe_integer& {
            if (would_subtraction_overflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_overflow {};
            }
            if (would_subtraction_underflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_underflow {};
            }
            value -= other.value;
            return *this;
        }
        constexpr auto operator-(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy -= other;
        }

        constexpr auto operator*=(Safe_integer const other) -> Safe_integer& {
            if (would_multiplication_overflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_overflow {};
            }
            if (would_multiplication_underflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_underflow {};
            }
            value *= other.value;
            return *this;
        }
        constexpr auto operator*(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy *= other;
        }

        constexpr auto operator/=(Safe_integer const other) -> Safe_integer& {
            if (other.value == 0) [[unlikely]] {
                throw Safe_integer_division_by_zero {};
            }
            if (would_division_overflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_overflow {};
            }
            value /= other.value;
            return *this;
        }
        constexpr auto operator/(Safe_integer const other) const -> Safe_integer {
            auto copy = *this;
            return copy /= other;
        }

        constexpr auto operator%=(Safe_integer const other) -> Safe_integer& {
            if (would_division_overflow(value, other.value)) [[unlikely]] {
                throw Safe_integer_overflow {};
            }
            value %= other.value;
            return *this;
        }
        constexpr auto operator%(Safe_integer const other) -> Safe_integer {
            auto copy = *this;
            return copy %= other;
        }

        constexpr auto operator++() -> Safe_integer& {
            if (would_increment_overflow(value)) [[unlikely]] {
                throw Safe_integer_overflow {};
            }
            ++value;
            return *this;
        }
        constexpr auto operator++(int) -> Safe_integer {
            auto copy = *this;
            ++*this;
            return copy;
        }

        constexpr auto operator--() -> Safe_integer& {
            if (would_decrement_underflow(value)) [[unlikely]] {
                throw Safe_integer_underflow {};
            }
            --value;
            return *this;
        }
        constexpr auto operator--(int) -> Safe_integer {
            auto copy = *this;
            ++*this;
            return copy;
        }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept {
            return value != 0;
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
    auto format(utl::Safe_integer<T> const value, auto& context) {
        return formatter<T>::format(value.get(), context);
    }
};
