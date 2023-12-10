#include <charconv>
#include <libutl/common/utilities.hpp>
#include <libutl/common/safe_integer.hpp>
#include <liblex/numeric.hpp>

namespace {
    auto without_separators(std::string_view const digits) -> std::string_view
    {
        if (!digits.contains('\'')) {
            return digits;
        }
        static thread_local std::string buffer;
        buffer.clear();
        ranges::copy_if(digits, std::back_inserter(buffer), [](char const c) { return c != '\''; });
        return buffer;
    }

    template <class T>
    auto parse_impl(std::string_view const string, std::same_as<int> auto const... base)
        -> std::expected<T, liblex::Numeric_error>
    {
        char const* const begin = string.data();
        char const* const end   = begin + string.size();
        utl::always_assert(begin != end);

        T value {};
        auto const [ptr, ec] = std::from_chars(begin, end, value, base...);

        if (ptr != end) {
            return std::unexpected { liblex::Numeric_error::invalid_argument };
        }
        if (ec != std::errc {}) {
            utl::always_assert(ec == std::errc::result_out_of_range);
            return std::unexpected { liblex::Numeric_error::out_of_range };
        }
        return value;
    }
} // namespace

auto liblex::apply_scientific_exponent(utl::Usize integer, utl::Usize const exponent)
    -> std::expected<utl::Usize, Numeric_error>
{
    for (utl::Usize i = 0; i != exponent; ++i) {
        if (utl::would_multiplication_overflow(integer, 10UZ)) {
            return std::unexpected { Numeric_error::out_of_range };
        }
        integer *= 10;
    }
    return integer;
}

auto liblex::parse_integer(std::string_view const digits, int const base)
    -> std::expected<utl::Usize, Numeric_error>
{
    return parse_impl<utl::Usize>(without_separators(digits), base);
}

auto liblex::parse_floating(std::string_view const digits) -> std::expected<double, Numeric_error>
{
    // TODO: when from_chars for floating points is supported:
    // return parse_impl<double>(without_separators(digits));

    static std::string float_digits;
    float_digits = without_separators(digits);

    try {
        return std::stod(float_digits);
    }
    catch (std::out_of_range const&) {
        return std::unexpected { liblex::Numeric_error::out_of_range };
    }
    catch (std::invalid_argument const&) {
        return std::unexpected { liblex::Numeric_error::invalid_argument };
    }
}
