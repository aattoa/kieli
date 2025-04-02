#ifndef KIELI_LIBPARSE_TEST_INTERFACE
#define KIELI_LIBPARSE_TEST_INTERFACE

#include <libutl/utilities.hpp>

namespace ki::parse {

    auto test_parse_expression(std::string text) -> std::string;
    auto test_parse_pattern(std::string text) -> std::string;
    auto test_parse_type(std::string text) -> std::string;

} // namespace ki::parse

#endif // KIELI_LIBPARSE_TEST_INTERFACE
