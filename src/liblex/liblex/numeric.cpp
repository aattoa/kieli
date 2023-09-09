#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <liblex/numeric.hpp>

namespace {
    auto without_separators(std::string_view const digits)
        -> utl::Pair<std::string, std::string_view>
    {
        if (!digits.contains('\'')) {
            return { std::string {}, digits };
        }
        std::string buffer;
        utl::disable_short_string_optimization(buffer);
        ranges::copy_if(digits, std::back_inserter(buffer), [](char const c) { return c != '\''; });
        std::string_view const view = buffer;
        return { std::move(buffer), view };
    }

    template <class T>
    auto parse_impl(std::string_view const raw_string, std::same_as<int> auto const... base)
        -> tl::expected<T, liblex::Numeric_error>
    {
        auto const [buffer, string] = without_separators(raw_string);
        assert(!string.empty());

        char const* const begin = string.data();
        char const* const end   = begin + string.size();
        T                 value {};
        auto const [ptr, ec] = std::from_chars(begin, end, value, base...);

        if (ptr != end) {
            return tl::unexpected { liblex::Numeric_error::invalid_argument };
        }
        else if (ec != std::errc {}) {
            utl::always_assert(ec == std::errc::result_out_of_range);
            return tl::unexpected { liblex::Numeric_error::out_of_range };
        }
        else {
            return value;
        }
    }
} // namespace

auto liblex::apply_scientific_exponent(utl::Usize integer, utl::Usize const exponent)
    -> tl::expected<utl::Usize, Numeric_error>
{
    for (utl::Usize i = 0; i != exponent; ++i) {
        if (utl::would_multiplication_overflow(integer, 10_uz)) {
            return tl::unexpected { Numeric_error::out_of_range };
        }
        integer *= 10;
    }
    return integer;
}

auto liblex::parse_integer(std::string_view const raw_string, int const base)
    -> tl::expected<utl::Usize, Numeric_error>
{
    return parse_impl<utl::Usize>(raw_string, base);
}

auto liblex::parse_floating(std::string_view const raw_string)
    -> tl::expected<utl::Float, Numeric_error>
{
    return parse_impl<utl::Float>(raw_string);
}
