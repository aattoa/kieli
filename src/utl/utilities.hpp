#pragma once


// This file is intended to be included by every single translation unit in the project


#include "disable_unnecessary_warnings.hpp"

#include <cstdlib>
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

#include <string>
#include <charconv>
#include <string_view>

#include <numeric>
#include <algorithm>

#include <fmt/format.h>
#include <fmt/chrono.h>
#include <range/v3/all.hpp>
#include <tl/optional.hpp>
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
    concept losslessly_convertible_to = requires {
        requires std::integral<From> && std::integral<To>;
        requires std::cmp_greater_equal(
            std::numeric_limits<From>::min(),
            std::numeric_limits<To>::min());
        requires std::cmp_less_equal(
            std::numeric_limits<From>::max(),
            std::numeric_limits<To>::max());
    };

    static_assert(losslessly_convertible_to<I8, I16>);
    static_assert(losslessly_convertible_to<I32, I32>);
    static_assert(losslessly_convertible_to<U8, I32>);
    static_assert(losslessly_convertible_to<U32, I64>);
    static_assert(!losslessly_convertible_to<I8, U8>);
    static_assert(!losslessly_convertible_to<U64, I8>);
    static_assert(!losslessly_convertible_to<I8, U64>);
    static_assert(!losslessly_convertible_to<I16, I8>);


    struct Safe_cast_invalid_argument : std::invalid_argument {
        Safe_cast_invalid_argument()
            : invalid_argument { "utl::safe_cast argument out of target range" } {}
    };

    template <std::integral To, std::integral From> [[nodiscard]]
    constexpr auto safe_cast(From const from)
        noexcept(losslessly_convertible_to<From, To>) -> To
    {
        if constexpr (losslessly_convertible_to<From, To>) {
            // Still checked in debug mode just to be sure
            assert(std::in_range<To>(from));
            return static_cast<To>(from);
        }
        else {
            if (std::in_range<To>(from))
                return static_cast<To>(from);
            else
                throw Safe_cast_invalid_argument {};
        }
    }
}


namespace utl::inline literals {
    consteval auto operator"" _i8 (unsigned long long const n) noexcept { return safe_cast<I8 >(n); }
    consteval auto operator"" _i16(unsigned long long const n) noexcept { return safe_cast<I16>(n); }
    consteval auto operator"" _i32(unsigned long long const n) noexcept { return safe_cast<I32>(n); }
    consteval auto operator"" _i64(unsigned long long const n) noexcept { return safe_cast<I64>(n); }

    consteval auto operator"" _u8 (unsigned long long const n) noexcept { return safe_cast<U8 >(n); }
    consteval auto operator"" _u16(unsigned long long const n) noexcept { return safe_cast<U16>(n); }
    consteval auto operator"" _u32(unsigned long long const n) noexcept { return safe_cast<U32>(n); }
    consteval auto operator"" _u64(unsigned long long const n) noexcept { return safe_cast<U64>(n); }

    consteval auto operator"" _uz(unsigned long long const n) noexcept { return safe_cast<Usize>(n); }
    consteval auto operator"" _iz(unsigned long long const n) noexcept { return safe_cast<Isize>(n); }

    consteval auto operator"" _format(char const* const s, Usize const n) noexcept {
        return [fmt = std::string_view(s, n)](auto const&... args) -> std::string {
            return fmt::vformat(fmt, fmt::make_format_args(args...));
        };
    }
}


// Literal operators are useless when not easily accessible, so let's make them available everywhere.
using namespace utl::literals;
using namespace std::literals;


namespace bootleg {
    template <class E> requires std::is_enum_v<E>
    auto to_underlying(E const e) noexcept {
        return static_cast<std::underlying_type_t<E>>(e);
    }

    // Implementation copied from https://en.cppreference.com/w/cpp/utility/forward_like
    template<class T, class U> [[nodiscard]]
    constexpr auto&& forward_like(U&& x) noexcept {
        constexpr bool is_adding_const = std::is_const_v<std::remove_reference_t<T>>;
        if constexpr (std::is_lvalue_reference_v<T&&>) {
            if constexpr (is_adding_const)
                return std::as_const(x);
            else
                return static_cast<U&>(x);
        }
        else {
            if constexpr (is_adding_const)
                return std::move(std::as_const(x));
            else
                return std::move(x);
        }
    }
}


namespace utl {

    namespace dtl {
        template <class, template <class...> class>
        struct Is_instance_of : std::false_type {};
        template <class... Ts, template <class...> class F>
        struct Is_instance_of<F<Ts...>, F> : std::true_type {};
    }

    template <class T, template <class...> class F>
    concept instance_of = dtl::Is_instance_of<T, F>::value;

    template <class T, class... Ts>
    concept one_of = std::disjunction_v<std::is_same<T, Ts>...>;

    template <class T>
    concept trivial = std::is_trivial_v<T>;

    template <class T>
    concept trivially_copyable = std::is_trivially_copyable_v<T>;


    template <class>
    struct Always_false : std::false_type {};
    template <class T>
    constexpr bool always_false = Always_false<T>::value;


    template <class E> requires std::is_enum_v<E> && requires { E::_enumerator_count; }
    constexpr Usize enumerator_count = static_cast<Usize>(E::_enumerator_count);

    template <class E, E min = E {}, E max = E::_enumerator_count> [[nodiscard]]
    constexpr auto is_valid_enumerator(E const e) noexcept -> bool
        requires std::is_enum_v<E>
    {
        return min <= e && max > e;
    }

    [[nodiscard]]
    constexpr auto as_index(auto const e) noexcept -> Usize
        requires requires { is_valid_enumerator(e); }
    {
        assert(is_valid_enumerator(e));
        return static_cast<Usize>(e);
    }


    constexpr bool compiling_in_debug_mode =
#ifdef NDEBUG
        false;
#else
        true;
#endif

    constexpr bool compiling_in_release_mode = !compiling_in_debug_mode;


    constexpr auto filename_without_path(std::string_view path) noexcept -> std::string_view {
        auto const trim_if = [&](char const c) {
            if (auto const pos = path.find_last_of(c); pos != std::string_view::npos)
                path.remove_prefix(pos + 1);
        };
        trim_if('\\');
        trim_if('/');
        assert(!path.empty());
        return path;
    }

    static_assert(filename_without_path("aaa/bbb/ccc")   == "ccc");
    static_assert(filename_without_path("aaa\\bbb\\ccc") == "ccc");


    class [[nodiscard]] Exception : public std::exception {
        std::string message;
    public:
        explicit Exception(std::string&& message) noexcept
            : message { std::move(message) } {}
        [[nodiscard]]
        auto what() const noexcept -> char const* override {
            return message.c_str();
        }
    };

    template <class... Args>
    auto exception(fmt::format_string<Args...> const fmt, Args const&... args) -> Exception {
        return Exception { fmt::vformat(fmt.get(), fmt::make_format_args(args...))};
    }

    [[noreturn]]
    inline auto abort(
        std::string_view     const message = "no message",
        std::source_location const caller  = std::source_location::current()) -> void
    {
        fmt::println(
            "[{}:{}:{}] utl::abort invoked with message: {}, in function '{}'",
            filename_without_path(caller.file_name()),
            caller.line(),
            caller.column(),
            message,
            caller.function_name());
        std::exit(EXIT_FAILURE);
    }

    inline auto always_assert(
        bool                 const assertion,
        std::source_location const caller = std::source_location::current()) -> void
    {
        if (!assertion) [[unlikely]]
            abort("Assertion failed", caller);
    }

    inline auto trace(
        std::source_location const caller = std::source_location::current()) -> void
    {
        fmt::println(
            "utl::trace: Reached line {} in {}, in function '{}'",
            caller.line(),
            filename_without_path(caller.file_name()),
            caller.function_name());
    }


    [[noreturn]]
    inline auto todo(std::source_location const caller = std::source_location::current()) -> void {
        abort("Unimplemented branch reached", caller);
    }
    [[noreturn]]
    inline auto unreachable(std::source_location const caller = std::source_location::current()) -> void {
        abort("Unreachable branch reached", caller);
    }


    template <class Fst, class Snd = Fst>
    struct [[nodiscard]] Pair {
        Fst first;
        Snd second;

        Pair() = default;

        template <class F, class S>
        constexpr Pair(F&& f, S&& s)
            noexcept(std::is_nothrow_constructible_v<Fst, F> && std::is_nothrow_constructible_v<Snd, S>)
            requires std::constructible_from<Fst, F> && std::constructible_from<Snd, S>
            : first  { std::forward<F>(f) }
            , second { std::forward<S>(s) } {}

        [[nodiscard]]
        auto operator==(Pair const&) const -> bool = default;
    };

    template <class Fst, class Snd>
    Pair(Fst, Snd) -> Pair<std::remove_cvref_t<Fst>, std::remove_cvref_t<Snd>>;

    constexpr auto first  = [](auto&& pair) noexcept -> decltype(auto) { return bootleg::forward_like<decltype(pair)>(pair.first); };
    constexpr auto second = [](auto&& pair) noexcept -> decltype(auto) { return bootleg::forward_like<decltype(pair)>(pair.second); };


    // Value wrapper that is used to disable default constructors
    template <class T>
    class [[nodiscard]] Strong {
        T m_value;
    public:
        /*implicit*/ constexpr Strong(T const& value) // NOLINT
            noexcept(std::is_nothrow_copy_constructible_v<T>)
            requires std::is_copy_constructible_v<T>
            : m_value { value } {}
        /*implicit*/ constexpr Strong(T&& value) // NOLINT
            noexcept(std::is_nothrow_move_constructible_v<T>)
            requires std::is_move_constructible_v<T>
            : m_value { std::move(value) } {}

        APPLY_EXPLICIT_OBJECT_PARAMETER_HERE
        [[nodiscard]] constexpr auto get() const & -> T const & { return m_value; }
        [[nodiscard]] constexpr auto get()       & -> T       & { return m_value; }
        [[nodiscard]] constexpr auto get() const&& -> T const&& { return std::move(m_value); }
        [[nodiscard]] constexpr auto get()      && -> T      && { return std::move(m_value); }
    };


    constexpr auto size = [](auto const& x)
        noexcept(noexcept(std::size(x))) -> Usize
    {
        return std::size(x);
    };

    template <class T>
    constexpr auto make = []<class... Args>(Args&&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args&&...>) -> T
    {
        return T(std::forward<Args>(args)...);
    };


    template <class F, class G, class... Hs> [[nodiscard]]
    constexpr auto compose(F&& f, G&& g, Hs&&... hs) {
        if constexpr (sizeof...(Hs) != 0) {
            return compose(std::forward<F>(f), compose(std::forward<G>(g), std::forward<Hs>(hs)...));
        }
        else {
            return [f = std::forward<F>(f), g = std::forward<G>(g)]<class... Args>(Args&&... args) -> decltype(auto) {
                return std::invoke(f, std::invoke(g, std::forward<Args>(args)...));
            };
        }
    };

    static_assert(
        compose(
            [](int x) { return x * x; },
            [](int x) { return x + 1; },
            [](int a, int b) { return a + b; }
        )(2, 3) == 36);


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


    template <class T, class V> [[nodiscard]]
    constexpr decltype(auto) get(
        V&&                        variant,
        std::source_location const caller = std::source_location::current()) noexcept
        requires requires { std::get_if<T>(&variant); }
    {
        if (auto* const alternative = std::get_if<T>(&variant)) [[likely]]
            return bootleg::forward_like<V>(*alternative);
        else [[unlikely]]
            abort("Bad variant access", caller);
    }

    template <Usize n, class V> [[nodiscard]]
    constexpr decltype(auto) get(
        V&&                        variant,
        std::source_location const caller = std::source_location::current()) noexcept
        requires requires { std::get_if<n>(&variant); }
    {
        return ::utl::get<std::variant_alternative_t<n, std::remove_cvref_t<V>>>(std::forward<V>(variant), caller);
    }

    template <class O> [[nodiscard]]
    constexpr decltype(auto) get(
        O&&                        optional,
        std::source_location const caller = std::source_location::current()) noexcept
        requires requires { optional.has_value(); }
    {
        if (optional.has_value()) [[likely]]
            return bootleg::forward_like<O>(*optional);
        else [[unlikely]]
            abort("Bad optional access", caller);
    }

    template <class Variant, class... Arms>
    constexpr decltype(auto) match(Variant&& variant, Arms&&... arms)
        noexcept(noexcept(std::visit(Overload { std::forward<Arms>(arms)... }, std::forward<Variant>(variant))))
    {
        if (variant.valueless_by_exception()) [[unlikely]]
            abort("utl::match was invoked with a valueless variant");
        return std::visit(Overload { std::forward<Arms>(arms)... }, std::forward<Variant>(variant));
    }

    template <class Ok, std::derived_from<std::exception> Err>
    constexpr auto expect(tl::expected<Ok, Err>&& expected) -> Ok&& {
        if (expected)
            return std::move(*expected);
        else
            throw std::move(expected.error());
    }


    template <std::invocable Callback>
    class [[nodiscard]] Scope_success_handler {
        Callback callback;
        int exception_count;
    public:
        explicit Scope_success_handler(Callback callback)
            : callback { std::move(callback) }
            , exception_count { std::uncaught_exceptions() } {}
        ~Scope_success_handler() noexcept(std::is_nothrow_invocable_v<Callback>) {
            if (exception_count == std::uncaught_exceptions())
                std::invoke(callback);
        }
    };
    auto on_scope_success(std::invocable auto callback) {
        return Scope_success_handler { std::move(callback) };
    }

    template <std::invocable Callback>
    class [[nodiscard]] Scope_exit_handler {
        Callback callback;
    public:
        explicit Scope_exit_handler(Callback callback)
            : callback { std::move(callback) } {}
        ~Scope_exit_handler() noexcept(std::is_nothrow_invocable_v<Callback>) {
            std::invoke(callback);
        }
    };
    auto on_scope_exit(std::invocable auto callback) {
        return Scope_exit_handler { std::move(callback) };
    }


    inline auto disable_short_string_optimization(std::string& string) -> void {
        if (string.capacity() <= sizeof(std::string))
            string.reserve(sizeof(std::string) + 1);
    }


    [[nodiscard]]
    inline auto string_with_capacity(Usize const capacity) -> std::string {
        std::string string;
        string.reserve(capacity);
        return string;
    }

    template <class T> [[nodiscard]]
    constexpr auto vector_with_capacity(Usize const capacity) -> std::vector<T> {
        std::vector<T> vector;
        vector.reserve(capacity);
        return vector;
    }

    namespace dtl {
        struct Vector_with_capacity_closure {
            Usize capacity;
            template <class T> [[nodiscard]]
            /*implicit*/ operator std::vector<T>() const { // NOLINT
                APPLY_EXPLICIT_OBJECT_PARAMETER_HERE;
                return vector_with_capacity<T>(capacity);
            }
        };
    }
    [[nodiscard]]
    constexpr auto vector_with_capacity(Usize const capacity)
        noexcept -> dtl::Vector_with_capacity_closure
    {
        return { .capacity = capacity };
    }

    template <class T>
    constexpr auto release_vector_memory(std::vector<T>& vector) noexcept -> void {
        std::vector<T> {}.swap(vector);
    }

    template <class T, Usize n> [[nodiscard]]
    constexpr auto to_vector(T(&&array)[n]) -> std::vector<T> {
        return ranges::to<std::vector>(ranges::views::move(array));
    }

    // Unlike `std::vector<T>::resize`, this does not require `T` to be default constructible.
    template <class T>
    constexpr auto resize_down_vector(std::vector<T>& vector, Usize const new_size) -> void {
        always_assert(vector.size() >= new_size);
        vector.erase(vector.begin() + safe_cast<typename std::vector<T>::iterator::difference_type>(new_size), vector.end());
    }


    [[nodiscard]]
    constexpr auto unsigned_distance(auto const start, auto const stop) noexcept -> Usize {
        always_assert(start <= stop);
        return static_cast<Usize>(std::distance(start, stop));
    }

    [[nodiscard]]
    constexpr auto digit_count(std::integral auto integer) noexcept -> Usize {
        Usize digits = 0;
        do { integer /= 10; ++digits; } while (integer != 0);
        return digits;
    }

    static_assert(digit_count(0) == 1);
    static_assert(digit_count(-10) == 2);
    static_assert(digit_count(-999) == 3);
    static_assert(digit_count(12345) == 5);


    template <class F, class Vector>
    constexpr auto map(F&& f, Vector&& input) {
        using Result = std::remove_cvref_t<decltype(std::invoke(f, bootleg::forward_like<Vector>(input.front())))>;
        auto output = vector_with_capacity<Result>(input.size());
        for (auto& element : input)
            output.push_back(std::invoke(f, bootleg::forward_like<Vector>(element)));
        return output;
    }

    template <class F>
    constexpr auto map(F&& f) {
        return [f = std::forward<F>(f)]<class Vector>(Vector&& input) mutable {
            return map(std::forward<F>(f), std::forward<Vector>(input));
        };
    }


    template <class T> requires std::is_arithmetic_v<T> [[nodiscard]]
    constexpr auto try_parse(std::string_view const string) noexcept -> tl::optional<T> {
        if (string.empty())
            return tl::nullopt;
        T value;
        auto const [ptr, ec] = std::from_chars(string.data(), string.data() + string.size(), value);
        return ec == std::errc {} ? tl::optional<T> { value } : tl::nullopt;
    }


    template <class T>
    concept hashable = requires {
        requires std::is_default_constructible_v<std::hash<T>>;
        requires std::is_invocable_r_v<Usize, std::hash<T>, T const&>;
    };


    template <hashable Head, hashable... Tail>
    auto hash_combine_with_seed(
        Usize          seed,
        Head const&    head,
        Tail const&... tail) -> Usize
    {
        // https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x

        seed ^= (std::hash<Head>{}(head) + 0x9e3779b9 + (seed << 6) + (seed >> 2));

        if constexpr (sizeof...(tail) != 0)
            return hash_combine_with_seed(seed, tail...);
        else
            return seed;
    }

    auto hash_combine(hashable auto const&... args) -> Usize {
        return hash_combine_with_seed(0, args...);
    }


    // inline auto local_time() -> std::chrono::local_time<std::chrono::system_clock::duration> {
    //     return std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    // }


    auto serialize_to(std::output_iterator<std::byte> auto out, trivially_copyable auto const... args)
        noexcept -> void
    {
        (std::invoke([=, &out]() mutable noexcept {
            auto* const memory = reinterpret_cast<std::byte const*>(&args);
            for (Usize i = 0; i != sizeof args; ++i)
                *out++ = memory[i];
        }), ...);
    }


    template <Usize length>
    struct [[nodiscard]] Metastring {
        char string[length];

        /*implicit*/ consteval Metastring(char const* pointer) noexcept { // NOLINT
            std::copy_n(pointer, length, string);
        }
        [[nodiscard]]
        consteval auto view() const noexcept -> std::string_view {
            return { string, length - 1 };
        }
    };

    template <Usize length>
    Metastring(char const(&)[length]) -> Metastring<length>;


    namespace formatting {
        struct Formatter_base {
            constexpr auto parse(auto& parse_context) {
                return parse_context.end();
            }
        };
        struct Visitor_base {
            fmt::format_context::iterator out;

            template <class... Args>
            auto format(fmt::format_string<Args...> const fmt, Args const&... args) {
                return out = fmt::vformat_to(out, fmt.get(), fmt::make_format_args(args...));
            }
        };

        template <std::integral Integer>
        struct Integer_with_ordinal_indicator_formatter_closure {
            Integer integer;
        };
        template <std::integral Integer>
        constexpr auto integer_with_ordinal_indicator(Integer const integer) noexcept
            -> Integer_with_ordinal_indicator_formatter_closure<Integer>
        {
            return { .integer = integer };
        }

        template <ranges::sized_range Range>
        struct Range_formatter_closure {
            Range const*     range;
            std::string_view delimiter;
        };
        template <ranges::sized_range Range>
        auto delimited_range(Range const& range, std::string_view const delimiter)
            -> Range_formatter_closure<Range>
        {
            return { std::addressof(range), delimiter };
        }
    }

}


template <utl::hashable T>
struct std::hash<std::vector<T>> {
    auto operator()(std::vector<T> const& vector) const -> utl::Usize {
        utl::Usize seed = 0;
        for (auto& element : vector)
            seed = utl::hash_combine_with_seed(seed, element);
        return seed;
    }
};

template <utl::hashable Fst, utl::hashable Snd>
struct std::hash<utl::Pair<Fst, Snd>> {
    auto operator()(utl::Pair<Fst, Snd> const& pair) const -> utl::Usize {
        return utl::hash_combine(pair.first, pair.second);
    }
};


#define DECLARE_FORMATTER_FOR_TEMPLATE(...)                                \
struct fmt::formatter<__VA_ARGS__> : utl::formatting::Formatter_base {     \
    [[nodiscard]] auto format(__VA_ARGS__ const&, fmt::format_context&) \
        -> fmt::format_context::iterator;                                  \
}

#define DECLARE_FORMATTER_FOR(...) \
template <> DECLARE_FORMATTER_FOR_TEMPLATE(__VA_ARGS__)

#define DEFINE_FORMATTER_FOR(...) \
auto fmt::formatter<__VA_ARGS__>::format(__VA_ARGS__ const& value, fmt::format_context& context) \
    -> fmt::format_context::iterator

#define DIRECTLY_DEFINE_FORMATTER_FOR(...) \
DECLARE_FORMATTER_FOR(__VA_ARGS__);        \
DEFINE_FORMATTER_FOR(__VA_ARGS__)


template <class... Ts>
DECLARE_FORMATTER_FOR_TEMPLATE(std::variant<Ts...>);
template <class T, utl::Usize extent>
DECLARE_FORMATTER_FOR_TEMPLATE(std::span<T, extent>);
template <class T>
DECLARE_FORMATTER_FOR_TEMPLATE(std::vector<T>);
template <class F, class S>
DECLARE_FORMATTER_FOR_TEMPLATE(utl::Pair<F, S>);
template <class Range>
DECLARE_FORMATTER_FOR_TEMPLATE(utl::formatting::Range_formatter_closure<Range>);
template <class T>
DECLARE_FORMATTER_FOR_TEMPLATE(utl::formatting::Integer_with_ordinal_indicator_formatter_closure<T>);


template <class T>
struct fmt::formatter<tl::optional<T>> : fmt::formatter<T> {
    auto format(tl::optional<T> const& optional, auto& context) {
        if (optional)
            return formatter<T>::format(*optional, context);
        else
            return context.out();
    }
};


template <class... Ts>
DEFINE_FORMATTER_FOR(std::variant<Ts...>) {
    return std::visit([&](auto const& alternative) {
        return fmt::format_to(context.out(), "{}", alternative);
    }, value);
}

template <class T, utl::Usize extent>
DEFINE_FORMATTER_FOR(std::span<T, extent>) {
    return fmt::format_to(context.out(), "{}", utl::formatting::delimited_range(value, ", "));
}

template <class T>
DEFINE_FORMATTER_FOR(std::vector<T>) {
    return fmt::format_to(context.out(), "{}", std::span { value });
}

template <class F, class S>
DEFINE_FORMATTER_FOR(utl::Pair<F, S>) {
    return fmt::format_to(context.out(), "({}, {})", value.first, value.second);
}

template <class Range>
DEFINE_FORMATTER_FOR(utl::formatting::Range_formatter_closure<Range>) {
    if (value.range->empty())
        return context.out();
    auto out = fmt::format_to(context.out(), "{}", value.range->front());
    for (auto& element : *value.range | ranges::views::drop(1))
        out = fmt::format_to(out, "{}{}", value.delimiter, element);
    return out;
}

template <class T>
DEFINE_FORMATTER_FOR(utl::formatting::Integer_with_ordinal_indicator_formatter_closure<T>) {
    // https://stackoverflow.com/questions/61786685/how-do-i-print-ordinal-indicators-in-a-c-program-cant-print-numbers-with-st

    static constexpr auto suffixes = std::to_array<std::string_view>({ "th", "st", "nd", "rd" });

    T n = value.integer % 100;

    if (n == 11 || n == 12 || n == 13) {
        n = 0;
    }
    else {
        n %= 10;
        if (n > 3)
            n = 0;
    }

    return fmt::format_to(context.out(), "{}{}", value.integer, suffixes.at(n));
}


template <class T>
struct fmt::formatter<utl::Strong<T>> : fmt::formatter<T> {
    auto format(utl::Strong<T> const& strong, auto& context) {
        return fmt::formatter<T>::format(strong.get(), context);
    }
};

template <>
struct fmt::formatter<std::monostate> : utl::formatting::Formatter_base {
    auto format(std::monostate, auto& context) {
        return fmt::format_to(context.out(), "std::monostate");
    }
};
