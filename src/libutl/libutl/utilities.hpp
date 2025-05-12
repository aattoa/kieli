#ifndef KIELI_LIBUTL_UTILITIES
#define KIELI_LIBUTL_UTILITIES

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
#include <iostream>
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

// Literal operators are useless when not easily accessible, so it is best to
// make them available everywhere. There is no risk of name collision because
// the standard reserves literal operators that do not begin with an underscore.
using namespace std::literals; // NOLINT

namespace ki::utl {

    template <typename T, typename... Ts>
    concept one_of = std::disjunction_v<std::is_same<T, Ts>...>;

    template <std::size_t length>
    struct Metastring {
        std::array<char, length> array;

        consteval Metastring(char const (&string)[length]) // NOLINT
        {
            std::copy_n(static_cast<char const*>(string), length, array.data());
        }

        [[nodiscard]] consteval auto view() const noexcept -> std::string_view
        {
            return std::string_view(array.data(), length - 1);
        }
    };

    struct View {
        std::uint32_t offset {};
        std::uint32_t length {};

        [[nodiscard]] auto string(std::string_view string) const -> std::string_view;
    };

    template <typename... Fs>
    struct Overload : Fs... {
        using Fs::operator()...;
    };

    template <typename T>
    struct Transparent_hash : std::hash<T> {
        using is_transparent = void;
        using std::hash<T>::hash;
    };

    inline constexpr auto first = [](auto&& pair) noexcept -> decltype(auto) {
        auto& [first, _] = pair;
        return std::forward_like<decltype(pair)>(first);
    };

    inline constexpr auto second = [](auto&& pair) noexcept -> decltype(auto) {
        auto& [_, second] = pair;
        return std::forward_like<decltype(pair)>(second);
    };

    // clang-format off
    template <typename T>
    inline constexpr auto make = []<typename... Args>(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args...>) -> T
        requires std::is_constructible_v<T, Args...>
    {
        return T(std::forward<Args>(args)...);
    };
    // clang-format on

    template <typename T, std::size_t n>
    [[nodiscard]] constexpr auto to_vector(T (&&array)[n]) -> std::vector<T> // NOLINT
    {
        return std::ranges::to<std::vector>(std::views::as_rvalue(array));
    }

} // namespace ki::utl

#endif // KIELI_LIBUTL_UTILITIES
