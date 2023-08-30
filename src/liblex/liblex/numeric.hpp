#pragma once

#include <libutl/common/utilities.hpp>

namespace liblex {
    enum class Numeric_error {
        out_of_range,
        invalid_argument,
    };
    [[nodiscard]] auto apply_scientific_exponent(utl::Usize integer, utl::Usize exponent)
        -> tl::expected<utl::Usize, Numeric_error>;
    [[nodiscard]] auto parse_integer(std::string_view digits, int base = 10)
        -> tl::expected<utl::Usize, Numeric_error>;
    [[nodiscard]] auto parse_floating(std::string_view) -> tl::expected<utl::Float, Numeric_error>;
} // namespace liblex
