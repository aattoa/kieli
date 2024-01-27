#pragma once

#include <libutl/common/utilities.hpp>

namespace liblex {
    enum class Numeric_error {
        out_of_range,
        invalid_argument,
    };
    [[nodiscard]] auto apply_scientific_exponent(std::size_t integer, std::size_t exponent)
        -> std::expected<std::size_t, Numeric_error>;
    [[nodiscard]] auto parse_integer(std::string_view digits, int base = 10)
        -> std::expected<std::size_t, Numeric_error>;
    [[nodiscard]] auto parse_floating(std::string_view digits)
        -> std::expected<double, Numeric_error>;
} // namespace liblex
