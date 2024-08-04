#pragma once

// This file is intended to be used as a precompiled header across the entire project.

#include <cpputil/util.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <numeric>
#include <optional>
#include <print>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

// Literal operators are useless when not easily accessible.
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
        std::array<char, length> m_string;

        consteval Metastring(char const (&string)[length])
        {
            std::copy_n(static_cast<char const*>(string), length, m_string.data());
        }

        [[nodiscard]] consteval auto view() const noexcept -> std::string_view
        {
            return { m_string.data(), length - 1 };
        }
    };

    template <class... Fs>
    struct Overload : Fs... {
        using Fs::operator()...;
    };

    static constexpr auto first = [](auto&& pair) noexcept -> decltype(auto) {
        auto& [first, _] = pair;
        return std::forward_like<decltype(pair)>(first);
    };

    static constexpr auto second = [](auto&& pair) noexcept -> decltype(auto) {
        auto& [_, second] = pair;
        return std::forward_like<decltype(pair)>(second);
    };

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
        cpputil::always_assert(is_valid_enumerator(e));
        return static_cast<std::size_t>(e);
    }

    auto times(std::size_t const count, auto&& callback) -> void
    {
        for (std::size_t i = 0; i != count; ++i) {
            std::invoke(callback);
        }
    }

    // Value wrapper that is used to disable default constructors
    template <class T>
    class [[nodiscard]] Explicit {
        T m_value;
    public:
        template <class Arg>
        constexpr Explicit(Arg&& arg) noexcept(std::is_nothrow_constructible_v<T, Arg&&>)
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

    template <class T, class A>
    constexpr auto release_vector_memory(std::vector<T, A>& vector) noexcept -> void
    {
        std::vector<T, A> {}.swap(vector);
    }

    template <class T, std::size_t n>
    [[nodiscard]] constexpr auto to_vector(T (&&array)[n]) -> std::vector<T>
    {
        return std::ranges::to<std::vector>(std::views::as_rvalue(array));
    }

    template <class T, class A>
    [[nodiscard]] auto pop_back(std::vector<T, A>& vector) -> std::optional<T>
    {
        if (vector.empty()) {
            return std::nullopt;
        }
        T back = std::move(vector.back());
        vector.pop_back();
        return back;
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
        case 1:  return "st";
        case 2:  return "nd";
        case 3:  return "rd";
        default: return "th";
        }
    }
} // namespace utl

namespace utl::fmt {
    template <std::integral Integer>
    struct Integer_with_ordinal_indicator_closure {
        Integer integer {};
    };

    template <std::integral Integer>
    constexpr auto integer_with_ordinal_indicator(Integer const integer) noexcept
        -> Integer_with_ordinal_indicator_closure<Integer>
    {
        return { integer };
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
} // namespace utl::fmt

template <class Char, std::formattable<Char>... Ts>
struct std::formatter<std::variant<Ts...>, Char> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(std::variant<Ts...> const& variant, auto& context)
    {
        auto const visitor = [&](auto const& alternative) {
            return std::format_to(context.out(), "{}", alternative);
        };
        return std::visit(visitor, variant);
    }
};

template <class Char>
struct std::formatter<std::monostate, Char> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(std::monostate const&, auto& context)
    {
        return std::format_to(context.out(), "std::monostate");
    }
};

template <class Char, std::formattable<Char> T, std::formattable<Char> E>
struct std::formatter<std::expected<T, E>, Char> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(std::expected<T, E> const& expected, auto& context)
    {
        if (expected.has_value()) {
            return std::format_to(context.out(), "{}", expected.value());
        }
        return std::format_to(context.out(), "std::unexpected({})", expected.error());
    }
};

template <class Char, std::formattable<Char> E>
struct std::formatter<std::unexpected<E>, Char> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(std::unexpected<E> const& unexpected, auto& context)
    {
        return std::format_to(context.out(), "std::unexpected({})", unexpected.error());
    }
};

template <class Char, class Range>
struct std::formatter<utl::fmt::Join_closure<Range>, Char> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(auto const closure, auto& context)
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
struct std::formatter<utl::fmt::Integer_with_ordinal_indicator_closure<T>, Char> {
    static constexpr auto parse(auto& context)
    {
        return context.begin();
    }

    static auto format(auto const closure, auto& context)
    {
        return std::format_to(
            context.out(), "{}{}", closure.integer, utl::ordinal_indicator(closure.integer));
    }
};

template <std::formattable<char> T>
struct std::formatter<utl::Explicit<T>> : std::formatter<T> {
    auto format(utl::Explicit<T> const& e, auto& context) const
    {
        return std::formatter<T>::format(e.get(), context);
    }
};
