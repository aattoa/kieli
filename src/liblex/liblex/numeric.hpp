#pragma once

#include <libutl/utilities.hpp>

namespace liblex {
    enum class Numeric_error {
        out_of_range,
        invalid_argument,
    };
    [[nodiscard]] auto numeric_error_string(Numeric_error error) -> std::string_view;

    [[nodiscard]] auto apply_scientific_exponent(std::size_t integer, std::size_t exponent)
        -> std::expected<std::size_t, Numeric_error>;
    [[nodiscard]] auto parse_integer(std::string_view digits, int base = 10)
        -> std::expected<std::size_t, Numeric_error>;
    [[nodiscard]] auto parse_floating(std::string_view digits)
        -> std::expected<double, Numeric_error>;
} // namespace liblex

template <>
struct std::formatter<liblex::Numeric_error> : utl::fmt::Formatter_base {
    auto format(liblex::Numeric_error const& error, auto& context) const
    {
        return std::format_to(context.out(), "{}", liblex::numeric_error_string(error));
    }
};
