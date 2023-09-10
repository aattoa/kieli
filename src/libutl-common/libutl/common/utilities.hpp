#pragma once

// This file is intended to be included by every single translation unit in the project

#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <cassert>

#include <new>
#include <limits>
#include <memory>
#include <chrono>
#include <utility>
#include <typeinfo>
#include <concepts>
#include <exception>
#include <functional>
#include <type_traits>
#include <source_location>

#include <fstream>
#include <iostream>
#include <filesystem>

#include <span>
#include <array>
#include <vector>
#include <unordered_map>

#include <tuple>
#include <variant>
#include <optional>

#include <string>
#include <format>
#include <charconv>
#include <string_view>

#include <numeric>
#include <algorithm>

#include <range/v3/all.hpp>
#include <tl/expected.hpp>

// 8-bit bytes are assumed
static_assert(CHAR_BIT == 8);

// A tag to search for when C++23 EOP are supported
#define APPLY_EXPLICIT_OBJECT_PARAMETER_HERE

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

    using Char  = char;
    using Float = double;

    template <class From, class To>
    concept losslessly_convertible_to = std::integral<From> && std::integral<To>
                                     && std::in_range<To>(std::numeric_limits<From>::min())
                                     && std::in_range<To>(std::numeric_limits<From>::max());

    struct Safe_cast_argument_out_of_range : std::invalid_argument {
        Safe_cast_argument_out_of_range();
    };

    template <std::integral To, std::integral From>
    [[nodiscard]] constexpr auto
    safe_cast(From const from) noexcept(losslessly_convertible_to<From, To>) -> To
    {
        if constexpr (losslessly_convertible_to<From, To>) {
            return static_cast<To>(from);
        }
        else if (std::in_range<To>(from)) {
            return static_cast<To>(from);
        }
        else {
            throw Safe_cast_argument_out_of_range {};
        }
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

#define UTL_INTEGER_LITERAL(name, type)                          \
    consteval auto operator""_##name(unsigned long long const n) \
    {                                                            \
        return safe_cast<type>(n);                               \
    }
    UTL_INTEGER_LITERAL(uz, Usize)
    UTL_INTEGER_LITERAL(iz, Isize)
    UTL_INTEGER_LITERAL(i8, I8)
    UTL_INTEGER_LITERAL(u8, U8)
    UTL_INTEGER_LITERAL(i16, I16)
    UTL_INTEGER_LITERAL(u16, U16)
    UTL_INTEGER_LITERAL(i32, I32)
    UTL_INTEGER_LITERAL(u32, U32)
    UTL_INTEGER_LITERAL(i64, I64)
    UTL_INTEGER_LITERAL(u64, U64)
#undef UTL_INTEGER_LITERAL

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

namespace bootleg {
    // Implementation copied from https://en.cppreference.com/w/cpp/utility/forward_like
    template <class T, class U>
    [[nodiscard]] constexpr auto&& forward_like(U&& x) noexcept
    {
        constexpr bool is_adding_const = std::is_const_v<std::remove_reference_t<T>>;
        if constexpr (std::is_lvalue_reference_v<T&&>) {
            if constexpr (is_adding_const) {
                return std::as_const(x);
            }
            else {
                return static_cast<U&>(x);
            }
        }
        else {
            if constexpr (is_adding_const) {
                return std::move(std::as_const(x));
            }
            else {
                return std::move(x);
            }
        }
    }
} // namespace bootleg

namespace utl::dtl {
    template <class, template <class...> class>
    struct Is_specialization_of : std::false_type {};

    template <class... Ts, template <class...> class F>
    struct Is_specialization_of<F<Ts...>, F> : std::true_type {};

    template <class>
    struct Always_false : std::false_type {};
} // namespace utl::dtl

namespace utl {

    template <class T, template <class...> class F>
    concept specialization_of = dtl::Is_specialization_of<T, F>::value;
    template <class T, class... Ts>
    concept one_of = std::disjunction_v<std::is_same<T, Ts>...>;
    template <class T>
    concept trivial = std::is_trivial_v<T>;
    template <class T>
    concept trivially_copyable = std::is_trivially_copyable_v<T>;

    template <class T>
    constexpr bool always_false = dtl::Always_false<T>::value;

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
        assert(is_valid_enumerator(e));
        return static_cast<Usize>(e);
    }

    inline constexpr bool compiling_in_debug_mode =
#ifdef NDEBUG
        false;
#else
        true;
#endif
    inline constexpr bool compiling_in_release_mode = !compiling_in_debug_mode;

    class [[nodiscard]] Exception : public std::exception {
        std::string m_message;
    public:
        explicit Exception(std::string&& message) noexcept;
        [[nodiscard]] auto what() const noexcept -> char const* override;
    };

    template <class... Args>
    auto exception(std::format_string<Args...> const fmt, Args&&... args) -> Exception
    {
        return Exception { std::format(fmt, std::forward<Args>(args)...) };
    }

    template <class... Args>
    auto print(std::format_string<Args...> const fmt, Args&&... args) -> void
    {
        if constexpr (sizeof...(Args) == 0) {
            std::cout << fmt.get();
        }
        else {
            std::format_to(
                std::ostreambuf_iterator { std::cout }, fmt, std::forward<Args>(args)...);
        }
    }

    [[noreturn]] auto abort(
        std::string_view message = "Invoked utl::abort",
        std::source_location     = std::source_location::current()) -> void;

    [[noreturn]] auto todo(std::source_location = std::source_location::current()) -> void;

    [[noreturn]] auto unreachable(std::source_location = std::source_location::current()) -> void;

    auto always_assert(bool condition, std::source_location = std::source_location::current())
        -> void;

    auto trace(std::source_location = std::source_location::current()) -> void;

    template <class Fst, class Snd = Fst>
    struct [[nodiscard]] Pair {
        Fst first {};
        Snd second {};

        Pair() = default;

        template <class F, class S>
        constexpr Pair(F&& f, S&& s) noexcept(
            std::is_nothrow_constructible_v<Fst, F>&& std::is_nothrow_constructible_v<Snd, S>)
            requires std::constructible_from<Fst, F> && std::constructible_from<Snd, S>
            : first { std::forward<F>(f) }
            , second { std::forward<S>(s) }
        {}

        [[nodiscard]] auto operator==(Pair const&) const -> bool = default;
    };

    template <class Fst, class Snd>
    Pair(Fst, Snd) -> Pair<std::remove_cvref_t<Fst>, std::remove_cvref_t<Snd>>;

    inline constexpr auto first = [](auto&& pair) noexcept -> decltype(auto) {
        return bootleg::forward_like<decltype(pair)>(pair.first);
    };
    inline constexpr auto second = [](auto&& pair) noexcept -> decltype(auto) {
        return bootleg::forward_like<decltype(pair)>(pair.second);
    };

    // Value wrapper that is used to disable default constructors
    template <class T>
    class [[nodiscard]] Explicit {
        T m_value;
    public:
        constexpr Explicit(T const& value) // NOLINT: implicit
            noexcept(std::is_nothrow_copy_constructible_v<T>)
            requires std::is_copy_constructible_v<T>
            : m_value { value }
        {}

        constexpr Explicit(T&& value) // NOLINT: implicit
            noexcept(std::is_nothrow_move_constructible_v<T>)
            requires std::is_move_constructible_v<T>
            : m_value { std::move(value) }
        {}

        APPLY_EXPLICIT_OBJECT_PARAMETER_HERE
        [[nodiscard]] constexpr auto get() const& -> T const&
        {
            return m_value;
        }

        [[nodiscard]] constexpr auto get() & -> T&
        {
            return m_value;
        }

        [[nodiscard]] constexpr auto get() const&& -> T const&&
        {
            return std::move(m_value);
        }

        [[nodiscard]] constexpr auto get() && -> T&&
        {
            return std::move(m_value);
        }

        [[nodiscard]] constexpr operator T const&() const&
        {
            return m_value;
        }

        [[nodiscard]] constexpr operator T&() &
        {
            return m_value;
        }

        [[nodiscard]] constexpr operator T const&&() const&&
        {
            return std::move(m_value);
        }

        [[nodiscard]] constexpr operator T&&() &&
        {
            return std::move(m_value);
        }
    };

    auto filename_without_path(std::string_view path) noexcept -> std::string_view;

    inline constexpr auto size
        = [](auto const& x) noexcept(noexcept(std::size(x))) -> Usize { return std::size(x); };

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

    template <class Variant, class Alternative>
    struct Alternative_index;

    template <class... Ts, class T>
    struct Alternative_index<std::variant<Ts...>, T>
        : std::integral_constant<Usize, std::variant<Type<Ts>...> { Type<T> {} }.index()> {};
    template <class Variant, class Alternative>
    constexpr Usize alternative_index = Alternative_index<Variant, Alternative>::value;

    template <class Variant, class Alternative>
    struct Variant_has_alternative;

    template <class... Ts, class T>
    struct Variant_has_alternative<std::variant<Ts...>, T>
        : std::bool_constant<one_of<T, Ts...>> {};
    template <class Variant, class Alternative>
    constexpr bool variant_has_alternative = Variant_has_alternative<Variant, Alternative>::value;

    template <class T, class V>
    [[nodiscard]] constexpr decltype(auto)
    get(V&& variant, std::source_location const caller = std::source_location::current()) noexcept
        requires requires { std::get_if<T>(&variant); }
    {
        if (auto* const alternative = std::get_if<T>(&variant)) [[likely]] {
            return bootleg::forward_like<V>(*alternative);
        }
        else [[unlikely]] {
            abort("Bad variant access", caller);
        }
    }

    template <Usize n, class V>
    [[nodiscard]] constexpr decltype(auto)
    get(V&& variant, std::source_location const caller = std::source_location::current()) noexcept
        requires requires { std::get_if<n>(&variant); }
    {
        return ::utl::get<std::variant_alternative_t<n, std::remove_cvref_t<V>>>(
            std::forward<V>(variant), caller);
    }

    template <class O>
    [[nodiscard]] constexpr decltype(auto)
    get(O&& optional, std::source_location const caller = std::source_location::current()) noexcept
        requires requires { optional.has_value(); }
    {
        if (optional.has_value()) [[likely]] {
            return bootleg::forward_like<O>(*optional);
        }
        else [[unlikely]] {
            abort("Bad optional access", caller);
        }
    }

    template <class O>
    [[nodiscard]] constexpr auto value_or_default(O&& optional)
        requires requires { optional.has_value(); }
    {
        if (optional.has_value()) {
            return bootleg::forward_like<O>(*optional);
        }
        else {
            return typename std::remove_cvref_t<O>::value_type {};
        }
    }

    [[nodiscard]] constexpr auto visitable(auto const&... variants) noexcept -> bool
    {
        return (!variants.valueless_by_exception() && ...);
    }

    template <class Variant, class... Arms>
    constexpr decltype(auto) match(Variant&& variant, Arms&&... arms) noexcept(noexcept(
        std::visit(Overload { std::forward<Arms>(arms)... }, std::forward<Variant>(variant))))
    {
        if (variant.valueless_by_exception()) [[unlikely]] {
            abort("utl::match was invoked with a valueless variant");
        }
        return std::visit(Overload { std::forward<Arms>(arms)... }, std::forward<Variant>(variant));
    }

    template <class Ok, std::derived_from<std::exception> Err>
    constexpr auto expect(tl::expected<Ok, Err>&& expected) -> Ok&&
    {
        if (expected) {
            return std::move(*expected);
        }
        else {
            throw std::move(expected.error());
        }
    }

    template <std::invocable Callback>
    class [[nodiscard]] Scope_success_handler {
        Callback callback;
        int      exception_count;
    public:
        explicit Scope_success_handler(Callback callback)
            : callback { std::move(callback) }
            , exception_count { std::uncaught_exceptions() }
        {}

        ~Scope_success_handler() noexcept(std::is_nothrow_invocable_v<Callback>)
        {
            if (exception_count == std::uncaught_exceptions()) {
                std::invoke(callback);
            }
        }
    };

    auto on_scope_success(std::invocable auto callback)
    {
        return Scope_success_handler { std::move(callback) };
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

    [[nodiscard]] auto string_with_capacity(Usize capacity) -> std::string;

    template <class T>
    [[nodiscard]] constexpr auto vector_with_capacity(Usize const capacity) -> std::vector<T>
    {
        std::vector<T> vector;
        vector.reserve(capacity);
        return vector;
    }

    namespace dtl {
        struct Vector_with_capacity_closure {
            Usize capacity;

            template <class T>
            [[nodiscard]] operator std::vector<T>() const // NOLINT: implicit
            {
                APPLY_EXPLICIT_OBJECT_PARAMETER_HERE;
                return vector_with_capacity<T>(capacity);
            }
        };
    } // namespace dtl

    [[nodiscard]] constexpr auto vector_with_capacity(Usize const capacity) noexcept
        -> dtl::Vector_with_capacity_closure
    {
        return { .capacity = capacity };
    }

    template <class T>
    constexpr auto release_vector_memory(std::vector<T>& vector) noexcept -> void
    {
        std::vector<T> {}.swap(vector);
    }

    template <class T, Usize n>
    [[nodiscard]] constexpr auto to_vector(T (&&array)[n]) -> std::vector<T>
    {
        return ranges::to<std::vector>(ranges::views::move(array));
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

    [[nodiscard]] constexpr auto digit_count(std::integral auto integer) noexcept -> Usize
    {
        Usize digits = 0;
        do {
            integer /= 10;
            ++digits;
        } while (integer != 0);
        return digits;
    }

    template <class F, class Vector>
    constexpr auto map(F&& f, Vector&& input)
        requires std::is_invocable_v<F&&, decltype(bootleg::forward_like<Vector>(input.front()))>
    {
        using Result = std::remove_cvref_t<decltype(std::invoke(
            f, bootleg::forward_like<Vector>(input.front())))>;
        auto output  = vector_with_capacity<Result>(input.size());
        for (auto& element : input) {
            output.push_back(std::invoke(f, bootleg::forward_like<Vector>(element)));
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

    [[nodiscard]] auto local_time() -> std::chrono::local_time<std::chrono::system_clock::duration>;

    struct [[nodiscard]] Relative_string {
        utl::Usize offset {};
        utl::Usize length {};

        [[nodiscard]] auto view_in(std::string_view) const -> std::string_view;

        template <class... Args>
        static auto
        format_to(std::string& string, std::format_string<Args...> const fmt, Args&&... args)
            -> Relative_string
        {
            Usize const old_size = string.size();
            std::format_to(std::back_inserter(string), fmt, std::forward<Args>(args)...);
            return { .offset = old_size, .length = string.size() - old_size };
        }
    };

    namespace formatting {
        struct Formatter_base {
            constexpr auto parse(auto& parse_context)
            {
                return parse_context.begin();
            }
        };

        template <std::integral Integer>
        struct Integer_with_ordinal_indicator_formatter_closure {
            Integer integer {};
        };

        template <std::integral Integer>
        constexpr auto integer_with_ordinal_indicator(Integer const integer) noexcept
            -> Integer_with_ordinal_indicator_formatter_closure<Integer>
        {
            return { .integer = integer };
        }

        template <ranges::sized_range Range>
        struct Range_formatter_closure {
            Range const*     range {};
            std::string_view delimiter;
        };

        template <ranges::sized_range Range>
        auto delimited_range(Range const& range, std::string_view const delimiter)
            -> Range_formatter_closure<Range>
        {
            return { std::addressof(range), delimiter };
        }
    } // namespace formatting

} // namespace utl

template <class... Ts>
struct std::formatter<std::variant<Ts...>> : utl::formatting::Formatter_base {
    auto format(std::variant<Ts...> const& variant, auto& context) const
    {
        return utl::match(variant, [&](auto const& alternative) {
            return std::format_to(context.out(), "{}", alternative);
        });
    }
};

template <class T, utl::Usize n>
struct std::formatter<std::span<T, n>> : utl::formatting::Formatter_base {
    auto format(std::span<T, n> const span, auto& context) const
    {
        return std::format_to(context.out(), "{}", utl::formatting::delimited_range(span, ", "));
    }
};

template <class T>
struct std::formatter<std::vector<T>> : std::formatter<std::span<T>> {
    auto format(std::vector<T> const& vector, auto& context) const
    {
        return std::format_to(context.out(), "{}", std::span { vector });
    }
};

template <class F, class S>
struct std::formatter<utl::Pair<F, S>> : utl::formatting::Formatter_base {
    auto format(utl::Pair<F, S> const& pair, auto& context) const
    {
        return std::format_to(context.out(), "({}, {})", pair.first, pair.second);
    }
};

template <class Range>
struct std::formatter<utl::formatting::Range_formatter_closure<Range>>
    : utl::formatting::Formatter_base {
    auto format(auto const closure, auto& context) const
    {
        if (closure.range->empty()) {
            return context.out();
        }

        auto       it  = ranges::begin(*closure.range);
        auto const end = ranges::end(*closure.range);
        auto       out = std::format_to(context.out(), "{}", *it++);

        while (it != end) {
            out = std::format_to(out, "{}{}", closure.delimiter, *it++);
        }
        return out;
    }
};

template <class T>
struct std::formatter<utl::formatting::Integer_with_ordinal_indicator_formatter_closure<T>>
    : utl::formatting::Formatter_base {
    auto format(auto const closure, auto& context) const
    {
        // https://stackoverflow.com/questions/61786685/how-do-i-print-ordinal-indicators-in-a-c-program-cant-print-numbers-with-st
        static constexpr auto suffixes
            = std::to_array<std::string_view>({ "th", "st", "nd", "rd" });
        T n = closure.integer % 100;
        if (n == 11 || n == 12 || n == 13) {
            n = 0;
        }
        else {
            n %= 10;
            if (n > 3) {
                n = 0;
            }
        }
        return std::format_to(context.out(), "{}{}", closure.integer, suffixes.at(n));
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
struct std::formatter<std::monostate> : utl::formatting::Formatter_base {
    auto format(std::monostate, auto& context) const
    {
        return std::format_to(context.out(), "std::monostate");
    }
};
