#pragma once

#include <libutl/common/utilities.hpp>


namespace libparse {
    enum class Test_parse_failure { unconsumed_input, no_parse };
    using Test_parse_result = tl::expected<std::string, Test_parse_failure>;

    auto test_parse_expression(std::string&&) -> Test_parse_result;
    auto test_parse_pattern   (std::string&&) -> Test_parse_result;
    auto test_parse_type      (std::string&&) -> Test_parse_result;

    // Make Test_parse_result formattable by Catch2
    auto operator<<(std::ostream&, Test_parse_result const&) -> std::ostream&;
}
