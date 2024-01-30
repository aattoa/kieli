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
        buffer.append_range(std::views::filter(digits, [](char const c) { return c != '\''; }));
        return buffer;
    }

    template <class T>
    auto parse_impl(std::string_view const string, std::same_as<int> auto const... base)
        -> std::expected<T, liblex::Numeric_error>
    {
        char const* const begin = string.data();
        char const* const end   = begin + string.size();
        cpputil::always_assert(begin != end);

        T value {};
        auto const [ptr, ec] = std::from_chars(begin, end, value, base...);

        if (ptr != end) {
            return std::unexpected { liblex::Numeric_error::invalid_argument };
        }
        if (ec != std::errc {}) {
            cpputil::always_assert(ec == std::errc::result_out_of_range);
            return std::unexpected { liblex::Numeric_error::out_of_range };
        }
        return value;
    }
} // namespace

auto liblex::numeric_error_string(Numeric_error const error) -> std::string_view
{
    switch (error) {
    case liblex::Numeric_error::out_of_range:
        return "liblex::Numeric_error::out_of_range";
    case liblex::Numeric_error::invalid_argument:
        return "liblex::Numeric_error::invalid_argument";
    default:
        cpputil::unreachable();
    }
}

auto liblex::apply_scientific_exponent(std::size_t integer, std::size_t const exponent)
    -> std::expected<std::size_t, Numeric_error>
{
    for (std::size_t i = 0; i != exponent; ++i) {
        if (utl::would_multiplication_overflow(integer, 10UZ)) {
            return std::unexpected { Numeric_error::out_of_range };
        }
        integer *= 10;
    }
    return integer;
}

auto liblex::parse_integer(std::string_view const digits, int const base)
    -> std::expected<std::size_t, Numeric_error>
{
    return parse_impl<std::size_t>(without_separators(digits), base);
}

auto liblex::parse_floating(std::string_view const digits) -> std::expected<double, Numeric_error>
{
    // TODO: when from_chars for floating points is supported:
    // return parse_impl<double>(without_separators(digits));

    try {
        return std::stod(std::string(without_separators(digits)));
    }
    catch (std::out_of_range const&) {
        return std::unexpected { liblex::Numeric_error::out_of_range };
    }
    catch (std::invalid_argument const&) {
        return std::unexpected { liblex::Numeric_error::invalid_argument };
    }
}
