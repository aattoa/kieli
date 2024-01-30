#pragma once

#include <libutl/common/utilities.hpp>

namespace libparse {
    auto test_parse_expression(std::string&&) -> std::string;
    auto test_parse_pattern(std::string&&) -> std::string;
    auto test_parse_type(std::string&&) -> std::string;
} // namespace libparse
