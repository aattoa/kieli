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
    using I8  = std::int8_t;
    using I16 = std::int16_t;
    using I32 = std::int32_t;
    using I64 = std::int64_t;

    using U8  = std::uint8_t;
    using U16 = std::uint16_t;
    using U32 = std::uint32_t;
    using U64 = std::uint64_t;

    using Usize = std::size_t;
    using Isize = std::make_signed_t<Usize>;

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

    template <Usize length>
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

    template <Usize length>
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

    [[noreturn]] auto abort(
        std::string_view message = "Invoked utl::abort",
        std::source_location     = std::source_location::current()) -> void;

    [[noreturn]] auto todo(std::source_location = std::source_location::current()) -> void;

    [[noreturn]] auto unreachable(std::source_location = std::source_location::current()) -> void;

    auto trace(std::source_location = std::source_location::current()) -> void;

    auto always_assert(bool, std::source_location = std::source_location::current()) -> void;

    template <class E>
        requires std::is_enum_v<E> && requires { E::_enumerator_count; }
    constexpr Usize enumerator_count = static_cast<Usize>(E::_enumerator_count);

    template <class E, E min = E {}, E max = E::_enumerator_count>
    [[nodiscard]] constexpr auto is_valid_enumerator(E const e) noexcept -> bool
        requires std::is_enum_v<E>
    {
        return min <= e && max > e;
    }

    [[nodiscard]] constexpr auto as_index(auto const e) noexcept -> Usize
        requires requires { is_valid_enumerator(e); }
    {
        always_assert(is_valid_enumerator(e));
        return static_cast<Usize>(e);
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

    // Filename without path
    auto basename(std::string_view full_path) noexcept -> std::string_view;

    template <class T>
    constexpr auto make = []<class... Args>(Args&&... args) noexcept(
                              std::is_nothrow_constructible_v<T, Args&&...>) -> T {
        return T(std::forward<Args>(args)...);
    };

    template <class F, class G, class... Hs>
    [[nodiscard]] constexpr auto compose(F&& f, G&& g, Hs&&... hs)
    {
        if constexpr (sizeof...(Hs) != 0) {
            return compose(
                std::forward<F>(f), compose(std::forward<G>(g), std::forward<Hs>(hs)...));
        }
        else {
            return [f = std::forward<F>(f),
                    g = std::forward<G>(g)]<class... Args>(Args&&... args) -> decltype(auto) {
                return std::invoke(f, std::invoke(g, std::forward<Args>(args)...));
            };
        }
    };

    template <class... Fs>
    struct Overload : Fs... {
        using Fs::operator()...;
    };
    template <class... Fs>
    Overload(Fs...) -> Overload<Fs...>;

    template <class>
    struct Type {};

    template <class T>
    constexpr Type<T> type;

    template <auto>
    struct Value {};

    template <auto x>
    constexpr Value<x> value;

    template <class T, class V>
    [[nodiscard]] constexpr auto get(
        V&&                        variant,
        std::source_location const caller
        = std::source_location::current()) noexcept -> decltype(auto)
        requires requires { std::get_if<T>(&variant); }
    {
        if (auto* const alternative = std::get_if<T>(&variant)) [[likely]] {
            return std::forward_like<V>(*alternative);
        }
        else [[unlikely]] {
            abort("Bad variant access", caller);
        }
    }

    template <Usize n, class V>
    [[nodiscard]] constexpr auto get(
        V&&                        variant,
        std::source_location const caller
        = std::source_location::current()) noexcept -> decltype(auto)
        requires requires { std::get_if<n>(&variant); }
    {
        return ::utl::get<std::variant_alternative_t<n, std::remove_cvref_t<V>>>(
            std::forward<V>(variant), caller);
    }

    template <class O>
    [[nodiscard]] constexpr auto get(
        O&&                        optional,
        std::source_location const caller
        = std::source_location::current()) noexcept -> decltype(auto)
        requires requires { optional.has_value(); }
    {
        if (optional.has_value()) [[likely]] {
            return std::forward_like<O>(*optional);
        }
        else [[unlikely]] {
            abort("Bad optional access", caller);
        }
    }

    [[nodiscard]] constexpr auto visitable(auto const&... variants) noexcept -> bool
    {
        return (!variants.valueless_by_exception() && ...);
    }

    template <class Variant, class... Arms>
    constexpr auto match(Variant&& variant, Arms&&... arms) noexcept(noexcept(std::visit(
        Overload { std::forward<Arms>(arms)... },
        std::forward<Variant>(variant)))) -> decltype(auto)
    {
        if (variant.valueless_by_exception()) [[unlikely]] {
            abort("utl::match was invoked with a valueless variant");
        }
        return std::visit(Overload { std::forward<Arms>(arms)... }, std::forward<Variant>(variant));
    }

    template <std::invocable Callback>
    class [[nodiscard]] Scope_exit_handler {
        Callback callback;
    public:
        explicit Scope_exit_handler(Callback callback) : callback { std::move(callback) } {}

        ~Scope_exit_handler() noexcept(std::is_nothrow_invocable_v<Callback>)
        {
            std::invoke(callback);
        }
    };

    auto on_scope_exit(std::invocable auto callback)
    {
        return Scope_exit_handler { std::move(callback) };
    }

    auto disable_short_string_optimization(std::string&) -> void;

    template <class T>
    constexpr auto release_vector_memory(std::vector<T>& vector) noexcept -> void
    {
        std::vector<T> {}.swap(vector);
    }

    template <class T, Usize n>
    [[nodiscard]] constexpr auto to_vector(T (&&array)[n]) -> std::vector<T>
    {
        return std::ranges::to<std::vector>(std::views::as_rvalue(array));
    }

    // Unlike `std::vector<T>::resize`, this does not require `T` to be default constructible.
    template <class T>
    constexpr auto resize_down_vector(std::vector<T>& vector, Usize const new_size) -> void
    {
        always_assert(vector.size() >= new_size);
        auto const offset = safe_cast<typename std::vector<T>::iterator::difference_type>(new_size);
        vector.erase(vector.begin() + offset, vector.end());
    }

    template <class T>
    constexpr auto append_vector(std::vector<T>& to, std::vector<T>&& from) -> void
    {
        always_assert(&to != &from);
        to.insert(to.end(), std::move_iterator { from.begin() }, std::move_iterator { from.end() });
        from.clear();
    }

    [[nodiscard]] constexpr auto unsigned_distance(
        auto const                 start,
        auto const                 stop,
        std::source_location const caller = std::source_location::current()) noexcept -> Usize
    {
        always_assert(std::less_equal()(start, stop), caller);
        return static_cast<Usize>(std::distance(start, stop));
    }

    template <
        std::input_or_output_iterator     It,
        std::sentinel_for<It>             Se,
        std::indirect_unary_predicate<It> F>
    [[nodiscard]] constexpr auto find_nth_if(It it, Se se, Usize const n, F f) -> It
    {
        return std::ranges::find_if(
            std::move(it), std::move(se), [&f, n, i = 0](auto const& x) mutable {
                return f(x) && (i++ == n);
            });
    }

    template <
        std::input_or_output_iterator                        It,
        std::sentinel_for<It>                                Se,
        std::equality_comparable_with<std::iter_value_t<It>> T>
    [[nodiscard]] constexpr auto find_nth(It it, Se se, Usize const n, T const& x) -> It
    {
        return find_nth_if(std::move(it), std::move(se), n, [&x](auto const& y) { return x == y; });
    }

    template <class F, class Vector>
    constexpr auto map(F&& f, Vector&& input)
        requires std::is_invocable_v<F&&, decltype(std::forward_like<Vector>(input.front()))>
    {
        using Result = std::remove_cvref_t<decltype(std::invoke(
            f, std::forward_like<Vector>(input.front())))>;
        std::vector<Result> output;
        output.reserve(input.size());
        for (auto& element : input) {
            output.push_back(std::invoke(f, std::forward_like<Vector>(element)));
        }
        return output;
    }

    template <class F>
    constexpr auto map(F&& f)
    {
        return [f = std::forward<F>(f)]<class Vector>(Vector&& input) mutable {
            return map(std::forward<F>(f), std::forward<Vector>(input));
        };
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
        utl::Usize offset {};
        utl::Usize length {};

        [[nodiscard]] auto view_in(std::string_view) const -> std::string_view;

        template <class... Args>
        static auto format_to(
            std::string&                      out,
            std::format_string<Args...> const fmt,
            Args&&... args) -> Relative_string
        {
            Usize const old_size = out.size();
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

template <class... Ts>
struct std::formatter<std::variant<Ts...>> : utl::fmt::Formatter_base {
    auto format(std::variant<Ts...> const& variant, auto& context) const
    {
        return utl::match(variant, [&](auto const& alternative) {
            return std::format_to(context.out(), "{}", alternative);
        });
    }
};

template <class Range>
struct std::formatter<utl::fmt::Join_closure<Range>> : utl::fmt::Formatter_base {
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

template <class T>
struct std::formatter<utl::fmt::Integer_with_ordinal_indicator_closure<T>>
    : utl::fmt::Formatter_base {
    auto format(auto const closure, auto& context) const
    {
        return std::format_to(
            context.out(), "{}{}", closure.integer, utl::ordinal_indicator(closure.integer));
    }
};

template <class T>
struct std::formatter<utl::Explicit<T>> : std::formatter<T> {
    auto format(utl::Explicit<T> const& strong, auto& context) const
    {
        return std::formatter<T>::format(strong.get(), context);
    }
};

template <>
struct std::formatter<std::monostate> : utl::fmt::Formatter_base {
    auto format(std::monostate, auto& context) const
    {
        return std::format_to(context.out(), "std::monostate");
    }
};
