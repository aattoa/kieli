#pragma once

// This file is intended to be included by every translation unit in the project

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <cassert>

#include <new>
#include <limits>
#include <memory>
#include <utility>
#include <concepts>
#include <exception>
#include <filesystem>
#include <functional>
#include <type_traits>
#include <source_location>

#include <span>
#include <array>
#include <vector>

#include <tuple>
#include <variant>
#include <optional>
#include <expected>

#include <print>
#include <format>
#include <string>
#include <string_view>

#include <ranges>
#include <numeric>
#include <algorithm>

namespace utl {
    template <class From, class To>
    concept losslessly_convertible_to = std::integral<From> && std::integral<To>
                                     && std::in_range<To>(std::numeric_limits<From>::min())
                                     && std::in_range<To>(std::numeric_limits<From>::max());

    struct Safe_cast_argument_out_of_range : std::invalid_argument {
        Safe_cast_argument_out_of_range();
    };

    template <std::integral To, std::integral From>
    [[nodiscard]] constexpr auto safe_cast(From const from)
        noexcept(losslessly_convertible_to<From, To>) -> To
    {
        if constexpr (!losslessly_convertible_to<From, To>) {
            if (!std::in_range<To>(from)) {
                throw Safe_cast_argument_out_of_range {};
            }
        }
        return static_cast<To>(from);
    }

    template <std::size_t length>
    struct [[nodiscard]] Metastring {
        char characters[length];

        consteval Metastring(char const* pointer) noexcept // NOLINT: implicit
        {
            std::copy_n(pointer, length, characters);
        }

        [[nodiscard]] consteval auto view() const noexcept -> std::string_view
        {
            return { characters, length - 1 };
        }
    };

    template <std::size_t length>
    Metastring(char const (&)[length]) -> Metastring<length>;
} // namespace utl

namespace utl::inline literals {
    template <Metastring string>
    consteval auto operator""_format() noexcept
    {
        return []<class... Args>(Args&&... args) -> std::string {
            return std::format(string.view(), std::forward<Args>(args)...);
        };
    }
} // namespace utl::inline literals

// Literal operators are useless when not easily accessible.
using namespace utl::literals;
using namespace std::literals;

namespace utl::dtl {
    template <class, template <class...> class>
    struct Is_specialization_of : std::false_type {};

    template <class... Ts, template <class...> class F>
    struct Is_specialization_of<F<Ts...>, F> : std::true_type {};
} // namespace utl::dtl

namespace utl {

    template <class T, template <class...> class F>
    concept specialization_of = dtl::Is_specialization_of<T, F>::value;

    template <class T, class... Ts>
    concept one_of = std::disjunction_v<std::is_same<T, Ts>...>;

    template <class... Fs>
    struct Overload : Fs... {
        using Fs::operator()...;
    };

    [[noreturn]] auto abort(
        std::string_view message = "Invoked utl::abort",
        std::source_location     = std::source_location::current()) -> void;

    [[noreturn]] auto todo(std::source_location = std::source_location::current()) -> void;

    [[noreturn]] auto unreachable(std::source_location = std::source_location::current()) -> void;

    auto trace(std::source_location = std::source_location::current()) -> void;

    auto always_assert(bool, std::source_location = std::source_location::current()) -> void;

    template <class E>
        requires std::is_enum_v<E> && requires { E::_enumerator_count; }
    constexpr std::size_t enumerator_count = static_cast<std::size_t>(E::_enumerator_count);

    template <class E, E min = E {}, E max = E::_enumerator_count>
    [[nodiscard]] constexpr auto is_valid_enumerator(E const e) noexcept -> bool
        requires std::is_enum_v<E>
    {
        return min <= e && max > e;
    }

    template <class E>
    [[nodiscard]] constexpr auto as_index(E const e) noexcept -> std::size_t
        requires std::is_enum_v<E>
    {
        always_assert(is_valid_enumerator(e));
        return static_cast<std::size_t>(e);
    }

    // Value wrapper that is used to disable default constructors
    template <class T>
    class [[nodiscard]] Explicit {
        T m_value;
    public:
        template <class Arg>
        constexpr Explicit(Arg&& arg) // NOLINT: implicit
            noexcept(std::is_nothrow_constructible_v<T, Arg&&>)
            requires(!std::is_same_v<Explicit, std::remove_cvref_t<Arg>>)
                 && std::is_constructible_v<T, Arg&&>
            : m_value { std::forward<Arg>(arg) }
        {}

        [[nodiscard]] constexpr auto get(this auto&& self) noexcept -> decltype(auto)
        {
            return std::forward_like<decltype(self)>(self.m_value);
        }
    };

    auto disable_short_string_optimization(std::string&) -> void;

    template <class T>
    constexpr auto release_vector_memory(std::vector<T>& vector) noexcept -> void
    {
        std::vector<T> {}.swap(vector);
    }

    template <class T, std::size_t n>
    [[nodiscard]] constexpr auto to_vector(T (&&array)[n]) -> std::vector<T>
    {
        return std::ranges::to<std::vector>(std::views::as_rvalue(array));
    }

    [[nodiscard]] constexpr auto ordinal_indicator(std::integral auto n) noexcept
        -> std::string_view
    {
        // https://stackoverflow.com/questions/61786685/how-do-i-print-ordinal-indicators-in-a-c-program-cant-print-numbers-with-st
        n %= 100;
        if (n == 11 || n == 12 || n == 13) {
            return "th";
        }
        switch (n % 10) {
        case 1:
            return "st";
        case 2:
            return "nd";
        case 3:
            return "rd";
        default:
            return "th";
        }
    }

    struct [[nodiscard]] Relative_string {
        std::size_t offset {};
        std::size_t length {};

        [[nodiscard]] auto view_in(std::string_view) const -> std::string_view;

        template <class... Args>
        static auto format_to(
            std::string&                      out,
            std::format_string<Args...> const fmt,
            Args&&... args) -> Relative_string
        {
            auto const old_size = out.size();
            std::format_to(std::back_inserter(out), fmt, std::forward<Args>(args)...);
            return { .offset = old_size, .length = out.size() - old_size };
        }
    };

    namespace fmt {
        struct Formatter_base {
            constexpr auto parse(auto& parse_context)
            {
                return parse_context.begin();
            }
        };

        template <std::integral Integer>
        struct Integer_with_ordinal_indicator_closure {
            Integer integer {};
        };

        template <std::integral Integer>
        constexpr auto integer_with_ordinal_indicator(Integer const integer) noexcept
            -> Integer_with_ordinal_indicator_closure<Integer>
        {
            return { .integer = integer };
        }

        template <std::ranges::input_range Range>
        struct Join_closure {
            Range*           range {};
            std::string_view delimiter;
        };

        template <std::ranges::input_range Range>
        auto join(Range&& range, std::string_view const delimiter)
        {
            return Join_closure { std::addressof(range), delimiter };
        }
    } // namespace fmt

} // namespace utl

template <class Char, std::formattable<Char>... Ts>
struct std::formatter<std::variant<Ts...>, Char> : utl::fmt::Formatter_base {
    auto format(std::variant<Ts...> const& variant, auto& context) const
    {
        return std::visit(
            [&](auto const& alternative) {
                return std::format_to(context.out(), "{}", alternative);
            },
            variant);
    }
};

template <class Char, class Range>
struct std::formatter<utl::fmt::Join_closure<Range>, Char> : utl::fmt::Formatter_base {
    auto format(auto const closure, auto& context) const
    {
        if (closure.range->empty()) {
            return context.out();
        }
        auto       it  = std::ranges::begin(*closure.range);
        auto const end = std::ranges::end(*closure.range);
        auto       out = std::format_to(context.out(), "{}", *it++);
        while (it != end) {
            out = std::format_to(out, "{}{}", closure.delimiter, *it++);
        }
        return out;
    }
};

template <class Char, std::formattable<Char> T>
struct std::formatter<utl::fmt::Integer_with_ordinal_indicator_closure<T>, Char>
    : utl::fmt::Formatter_base {
    auto format(auto const closure, auto& context) const
    {
        return std::format_to(
            context.out(), "{}{}", closure.integer, utl::ordinal_indicator(closure.integer));
    }
};

template <class Char, std::formattable<Char> T>
struct std::formatter<utl::Explicit<T>, Char> : std::formatter<T> {
    auto format(utl::Explicit<T> const& expl, auto& context) const
    {
        return std::formatter<T>::format(expl.get(), context);
    }
};

template <class Char>
struct std::formatter<std::monostate, Char> : utl::fmt::Formatter_base {
    auto format(std::monostate const&, auto& context) const
    {
        return std::format_to(context.out(), "std::monostate");
    }
};
