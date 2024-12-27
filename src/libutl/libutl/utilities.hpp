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
#include <mutex>
#include <numeric>
#include <optional>
#include <print>
#include <queue>
#include <ranges>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// Literal operators are useless when not easily accessible.
using namespace std::literals;

namespace utl::dtl {
    template <typename, template <typename...> typename>
    struct Is_specialization_of : std::false_type {};

    template <typename... Ts, template <typename...> typename F>
    struct Is_specialization_of<F<Ts...>, F> : std::true_type {};
} // namespace utl::dtl

namespace utl {
    template <typename T, template <typename...> typename F>
    concept specialization_of = dtl::Is_specialization_of<T, F>::value;

    template <typename T, typename... Ts>
    concept one_of = std::disjunction_v<std::is_same<T, Ts>...>;

    template <typename From, typename To>
    concept losslessly_convertible_to = std::integral<From> and std::integral<To>
                                    and std::in_range<To>(std::numeric_limits<From>::min())
                                    and std::in_range<To>(std::numeric_limits<From>::max());

    struct Safe_cast_argument_out_of_range : std::invalid_argument {
        Safe_cast_argument_out_of_range();
    };

    template <std::integral To, std::integral From>
    [[nodiscard]] constexpr auto safe_cast(From const from)
        noexcept(losslessly_convertible_to<From, To>) -> To
    {
        if constexpr (not losslessly_convertible_to<From, To>) {
            if (not std::in_range<To>(from)) {
                throw Safe_cast_argument_out_of_range {};
            }
        }
        return static_cast<To>(from);
    }

    template <std::size_t length>
    struct Metastring {
        std::array<char, length> array;

        consteval Metastring(char const (&string)[length]) // NOLINT(*explicit-conversions)
        {
            std::copy_n(static_cast<char const*>(string), length, array.data());
        }

        [[nodiscard]] consteval auto view() const noexcept -> std::string_view
        {
            return std::string_view(array.data(), length - 1);
        }
    };

    template <typename... Fs>
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

    // clang-format off
    template <typename T>
    static constexpr auto make = []<typename... Args>(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>) -> T
        requires std::is_constructible_v<T, Args...>
    {
        return T(std::forward<Args>(args)...);
    };
    // clang-format on

    void times(std::size_t const count, auto&& callback)
    {
        for (std::size_t i = 0; i != count; ++i) {
            std::invoke(callback);
        }
    }

    template <typename T, std::size_t n>
    [[nodiscard]] constexpr auto to_vector(T (&&array)[n]) -> std::vector<T>
    {
        return std::ranges::to<std::vector>(std::views::as_rvalue(array));
    }

    template <typename T, typename A>
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
        if (n == 11 or n == 12 or n == 13) {
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

template <typename Char, std::formattable<Char>... Ts>
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

template <typename Char>
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

template <typename Char, std::formattable<Char> T, std::formattable<Char> E>
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

template <typename Char, std::formattable<Char> E>
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
